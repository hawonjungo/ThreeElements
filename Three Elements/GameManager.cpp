
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS  // plain fopen/fscanf (LoadTopScores/SaveTopScores) instead of the MSVC-only *_s variants,
#endif                            // so the same code compiles unchanged under Emscripten's clang for the web build
#include "GameManager.h"
#include "MainPlayer.h"
#include "Skill.h"
#include "ImpTimer.h"
#include "PixelText.h"
#include "Draw.h"
#include "Audio.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


GameManager* GameManager::instance_ = NULL;

static void FormatTime(char* out, size_t size, float seconds);  // "mm:ss", defined with the HUD code below
static const char* PointsText(int points);                     // "+1" / "+10", defined with PLAY below

// Where the small save files (top scores, records, settings) live: next to the exe on desktop, as always; in the
// app's private data folder on Android, where an app cannot write to its install location. (The web build keeps
// them in localStorage instead and never calls this.)
static std::string SavePath(const char* name)
{
#ifdef __ANDROID__
    static std::string dir;
    if (dir.empty())
    {
        char* pref = SDL_GetPrefPath("relifes", "ThreeElements");
        if (pref != NULL)
        {
            dir = pref;
            SDL_free(pref);
        }
    }
    return dir + name;
#else
    return name;
#endif
}


GameManager::GameManager()
{
    m_window = NULL;
    m_screen = NULL;
    for (int i = 0; i < 12; ++i)
    {
        backgroundLayers[i] = NULL;  // Close() frees whichever layers were loaded
        backgroundPositions[i] = 0.0f;
        backgroundSpeeds[i] = 0.0f;
    }

}

GameManager::~GameManager()
{
}

bool GameManager::InitSDL()
{
    bool success = true;
    int ret = SDL_Init(SDL_INIT_VIDEO);
    if (ret < 0)
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    // Sound is optional: without an audio device the game runs silent instead of failing to start.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
        m_audioReady = audio::Init();
    else
        printf("SDL audio unavailable (%s): running without sound\n", SDL_GetError());

    // Nearest-neighbour scaling for everything by default: this is pixel art (the old "1" = linear blurred every
    // scaled sprite). Only the painted skill icons opt into smoothing (Skill::LoadIcon).
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    Uint32 windowFlags = SDL_WINDOW_SHOWN;
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");  // the game is landscape only
    windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif
    m_window = SDL_CreateWindow("Injoker",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        windowFlags);

    if (m_window == NULL)
    {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        success = false;
    }
    else
    {
        m_screen = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (m_screen == NULL)
        {
            printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
            success = false;
        }
        else
        {
            SDL_SetRenderDrawColor(m_screen, RENDER_DRAW_COLOR, RENDER_DRAW_COLOR, RENDER_DRAW_COLOR, RENDER_DRAW_COLOR);
            // The game is always drawn in 928 x 544 game pixels. A screen of another size or shape (a phone) gets it
            // scaled and letterboxed by SDL; on desktop and web the window already is 928 x 544, so nothing changes.
            SDL_RenderSetLogicalSize(m_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
            int imgFlags = IMG_INIT_PNG;
            if (!(IMG_Init(imgFlags) && imgFlags))
            {
                printf("IMG_Init failed: %s\n", IMG_GetError());
                success = false;
            }
        }

    }
    if (!success) {
        return false;
    }
    // Load background layers
    if (!loadBackgroundLayers()) {
        return false;
    }
    return success;
}

// Loads everything once, then runs frames until the player quits. Native: a plain loop capped at
// FRAME_PER_SECOND. Web: the browser calls RunFrame() once per display refresh (requestAnimationFrame)
// through emscripten_set_main_loop_arg, so there is no blocking loop and no ASYNCIFY in the web build.
// Every motion and animation runs on real time (dt), so the frame rate does not change the game speed.
void GameManager::LoopGame()
{
    LoadAssets();
    m_lastTick = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg([](void* self) { static_cast<GameManager*>(self)->RunFrame(); }, this, 0, 1);
#else
    ImpTimer fps_timer;
    bool running = true;
    while (running)
    {
        fps_timer.start();
        running = RunFrame();

        int real_imp_time = fps_timer.get_ticks();
        int time_one_frame = 1000 / FRAME_PER_SECOND;// ms
        if (real_imp_time < time_one_frame)
            SDL_Delay(time_one_frame - real_imp_time);
    }
    Close();
#endif
}

void GameManager::LoadAssets()
{
    m_hasPlayer = m_player.LoadImgAlpha(PLAYER_SPRITE_PATH, m_screen);
    m_titleLogo.LoadImgAlpha(TITLE_LOGO_PATH, m_screen);

    // key icons: the orbs (Q/W/E), invoke (R: touch button only, the keyboard has no on-screen R icon),
    // and the slot labels (D/F)
    Keyboard* keyIcons[] = { &m_keyQ, &m_keyW, &m_keyE, &m_keyR, &m_keyD, &m_keyF };
    const char* keyPaths[] = {
        "assets/keyboard/keyQ.png", "assets/keyboard/keyW.png", "assets/keyboard/keyE.png",
        "assets/keyboard/keyR.png", "assets/keyboard/keyD.png", "assets/keyboard/keyF.png" };
    for (int i = 0; i < 6; ++i)
    {
        keyIcons[i]->LoadImg(keyPaths[i], m_screen);
        keyIcons[i]->set_clips();
    }

    // skill icons: one sprite per Core skill, indexed by SkillId; icon paths come from Core
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
    {
        m_skillIcons[i].LoadIcon(invoker::GetSkillDefinition(static_cast<invoker::SkillId>(i)).icon, m_screen);
    }

    // enemy sprites: one per Practice enemy definition, loaded once (not on every spawn)
    for (int i = 0; i < practice::ENEMY_TYPE_COUNT; ++i)
    {
        const practice::EnemyDefinition& def = practice::GetEnemyDefinition(i);
        if (m_enemySprites[i].LoadImg(def.sprite, m_screen, def.frames))
            m_enemySprites[i].set_clips();
        else
            printf("Failed to load enemy sprite %s\n", def.sprite);
    }

    LoadTornadoSheet();  // if it is missing the game still plays, the Tornado is just not drawn
    LoadGhostWalkSheet();  // same for the Ghost Walk aura
    m_playerRunSheet = LoadSheet(PLAYER_RUN_SHEET_PATH, PLAYER_RUN_COLUMNS * PLAYER_RUN_FRAME);
    m_meteorSheet = LoadSheet(METEOR_SHEET_PATH, METEOR_COLUMNS * METEOR_FRAME);
    m_forgeSheet = LoadSheet(FORGE_SHEET_PATH, FORGE_COLUMNS * FORGE_FRAME);
    LoadTopRuns();
    LoadBests();
    LoadBossTimes();
    LoadPlayRuns();
    LoadGold();
    LoadSettings();
    m_session.RestoreBestCombo(m_bests.combo);  // the HUD's BEST is the all-time record from the start


#ifdef __EMSCRIPTEN__
    // Phones/tablets show the on-screen Q/W/E/R/D/F cluster from the start; a PC browser keeps it hidden
    // (same media query the web shell uses). Any finger touch later turns it on too (see SDL_FINGERDOWN).
    m_showTouchControls = EM_ASM_INT({ return window.matchMedia('(hover: none) and (pointer: coarse)').matches ? 1 : 0; }) != 0;
#endif
#ifdef __ANDROID__
    m_showTouchControls = true;  // a phone: the on-screen Q/W/E/R/D/F buttons are the controls
#endif

    printf("Three Elements - Practice Mode. Press Enter to start.\n");

}

// One frame: input, rules, drawing. Returns false once the player has asked to quit (native only).
bool GameManager::RunFrame()
{
    // Real time since the previous frame; everything that moves or animates runs on this, not on the frame
    // count. Capped so a long stall (tab in the background, debugger pause) does not jump the scene.
    Uint32 now = SDL_GetTicks();
    float dt = static_cast<float>(now - m_lastTick) / 1000.0f;
    m_lastTick = now;
    if (dt > MAX_FRAME_DT)
        dt = MAX_FRAME_DT;

    bool quit = false;
    //Handle events on queue
    while (SDL_PollEvent(&m_event) != 0)
    {
        //User requests quit
        if (m_event.type == SDL_QUIT)
        {
            quit = true;
        }
        else if (m_event.type == SDL_APP_WILLENTERBACKGROUND)
        {
            SaveRecordsSoFar();
        }
        else if (m_event.type == SDL_WINDOWEVENT && m_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            // The drawable area changed (a phone rotating to landscape at start-up, the navigation bar hiding):
            // centre the 928 x 544 game in the new size again instead of relying on the old letterbox.
            SDL_RenderSetLogicalSize(m_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
        }
        else if (m_event.type == SDL_KEYDOWN)
        {
            HandleKeyDown(m_event, quit);
        }
        else if (m_event.type == SDL_FINGERDOWN)  // phone/tablet touch (normalised 0..1 coordinates)
        {
            m_showTouchControls = true;  // a touch screen is in use, whatever the media query said
            int gameX = 0, gameY = 0;
            TouchToGame(m_event.tfinger.x, m_event.tfinger.y, gameX, gameY);
            HandlePointerDown(gameX, gameY, quit);
        }
        else if (m_event.type == SDL_MOUSEBUTTONDOWN && m_event.button.which != SDL_TOUCH_MOUSEID)
        {
            // which == SDL_TOUCH_MOUSEID would be a synthetic mouse event for a tap SDL_FINGERDOWN
            // already handled above; a real mouse click (desktop testing) is not, and still works.
            HandlePointerDown(m_event.button.x, m_event.button.y, quit);
        }
    }

    // Practice rules: enemy movement, spawning, leaks, Game Over (input of this frame is already applied).
    // The enemy's position is read first: a Tornado hit removes it inside Update(), and the "+1" goes where it was.
    practice::Bounds enemyBefore = m_session.Enemy().active ? practice::EnemyBounds(m_session.Enemy())
                                                            : practice::Bounds{ 0.0f, 0.0f, 0.0f, 0.0f };
    practice::UpdateResult update = m_session.Update(dt);
    LogUpdate(update);
    if (update.cast != practice::CastOutcome::None)
        OnCastJudged(update.cast, enemyBefore, update.cast != practice::CastOutcome::Correct ? NULL
            : update.kill.killed ? PointsText(update.kill.points) : "HIT");
    if (update.kill.killed)
        OnKill(update.kill, enemyBefore);
    if (update.leaked && update.shieldUsed)  // PLAY Shield rune: the hit is blocked
    {
        const SDL_Color cyan = { 110, 210, 255, 255 };
        for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
            if (m_floatTexts[i].left <= 0.0f) { m_floatTexts[i] = { FEEDBACK_TEXT_TIME, 150, 360, "BLOCKED", cyan }; break; }
        audio::Play(audio::Sfx::CastCorrect);
    }
    else if (update.leaked)
        OnLeak(m_session.GetStats().hp);  // HP was already reduced: this square was just lost
    if (update.gameOver)
    {
        EndSession();
        audio::Play(audio::Sfx::GameOver);
    }
    if (m_tutorialActive && !m_showRecipes)  // the recipe list pauses the tutorial run
    {
        practice::TutorialUpdateResult t = m_tutorial.Update(dt);
        if (t.spawned)
            m_tutorialSpawnFlash = TUTORIAL_SPAWN_FLASH;
        if (t.leaked)
            OnLeak(m_tutorial.Hearts());
        if (t.finished)
            audio::Play(audio::Sfx::Start);
    }
    if (m_bossActive)
        PresentBossUpdate(m_boss.Update(dt));
    m_bossDamageLeft -= dt;
    m_announceLeft -= dt;  // the fight's clock stops by itself once it is won or lost
    m_tutorialWrongFlash -= dt;
    m_orbFlash -= dt;
    m_tutorialSpawnFlash -= dt;
    UpdateFeedback(dt);
    m_ghostWalkLeft = m_ghostWalkLeft > dt ? m_ghostWalkLeft - dt : 0.0f;
    for (int vi = 0; vi < invoker::SKILL_COUNT; ++vi)
        m_skillVfx[vi].left = m_skillVfx[vi].left > dt ? m_skillVfx[vi].left - dt : 0.0f;
    for (int ei = 0; ei < practice::ENEMY_TYPE_COUNT; ++ei)
        m_enemySprites[ei].Update(dt);

    //Clear screen (black: the strip a screen shake uncovers at the edge stays dark)
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 0xFF);
    SDL_RenderClear(m_screen);

    // The scene shakes briefly after a leak: it is drawn into a viewport nudged by a few pixels. The HUD and
    // touch buttons are drawn afterwards at the normal position, so they never move under the player's thumb.
    // The normal viewport is SDL's letterbox (SDL_RenderSetLogicalSize): on a screen of another shape (a phone)
    // it is offset to centre the game, so it is saved and restored as it is. Resetting it with NULL would put the
    // game at the left edge (the bug seen on Android; invisible on desktop / web, where there is no letterbox).
    SDL_Rect sceneViewport;
    SDL_RenderGetViewport(m_screen, &sceneViewport);
    if (m_shakeLeft > 0.0f)
    {
        float strength = m_shakeLeft / FEEDBACK_SHAKE_TIME;
        float t = static_cast<float>(now) / 1000.0f;
        SDL_Rect shaken = sceneViewport;
        shaken.x += static_cast<int>(FEEDBACK_SHAKE_PX * strength * (0.5f + 0.5f * std::sin(t * 90.0f)));
        shaken.y += static_cast<int>(FEEDBACK_SHAKE_PX * strength * (0.5f + 0.5f * std::cos(t * 70.0f)));
        SDL_RenderSetViewport(m_screen, &shaken);
    }

    // Update and render background layers
    updateBackgroundLayers(dt);
    renderBackgroundLayers();

    RenderGhostWalk();  // behind the player: the player is never covered
    RenderPlayer();
    RenderEnemy();
    RenderBossImpacts();  // rings on the ground where delayed spells will land, under the boss
    RenderBoss();
    RenderTornadoes();  // above the background, player and enemy, below the HUD
    RenderSkillVfx();   // code-drawn skill effects, above everything else in the scene
    RenderFeedback();   // "+1" / "MISS" and the defeat ring, part of the scene
    SDL_RenderSetViewport(m_screen, &sceneViewport);  // end of the (possibly shaken) scene: back to the letterbox

    RenderLeakFlash();
    RenderTouchControls();  // on top of the scene, only while Playing
    RenderInvokerHud();
    RenderStatsHud();
    RenderTargetHint();
    RenderBossCombo();

    if (m_tutorialActive)
        RenderTutorial();
    else if (m_bossActive)
    {
        if (m_boss.State() != practice::BossState::Fighting)
            RenderBossResult();
    }
    else if (m_session.State() == practice::GameState::Ready)
        RenderReadyScreen();
    else if (m_session.State() == practice::GameState::GameOver)
        RenderGameOverScreen();
    RenderAnnouncement();
    if (m_showRecipes && m_session.State() != practice::GameState::Playing)
        RenderRecipes();
    if (m_showLeaderboard && !m_tutorialActive && m_session.State() != practice::GameState::Playing)
        RenderLeaderboard();
    if (m_showBossSelect)
        RenderBossSelect();
    RenderSoundButton();  // every state, above the Ready / Game Over dimming

    //Update screen
    SDL_RenderPresent(m_screen);
    return !quit;
}

// ------------------------------------------------------------------ input and session

void GameManager::HandleKeyDown(const SDL_Event& e, bool& quit)
{
    if (e.key.repeat)  // auto-repeat of a held key is not a new press (Enter/Esc included)
        return;

    SDL_Keycode sym = e.key.keysym.sym;
    if (m_showRecipes || m_showLeaderboard)  // an overlay is open: any key just closes it
    {
        m_showRecipes = m_showLeaderboard = false;
        return;
    }
    if (m_tutorialActive)
    {
        HandleTutorialKey(sym, e);
        return;
    }
    if (m_showBossSelect)
    {
        HandleBossSelectKey(sym);
        return;
    }
    if (m_bossActive)
    {
        HandleBossKey(sym, e);
        return;
    }
    bool menu = m_session.State() == practice::GameState::Ready;
    if (menu && (sym == SDLK_UP || sym == SDLK_DOWN))  // move through the menu (wraps round)
    {
        int n = MenuItemCount();
        m_menuIndex = (m_menuIndex + (sym == SDLK_UP ? n - 1 : 1)) % n;
        return;
    }
    if (menu && (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE))
    {
        ActivateMenuItem(MenuItemAt(m_menuIndex), quit);
        return;
    }
    if (sym == SDLK_h && m_session.State() != practice::GameState::Playing) { m_showRecipes = true; return; }
    if (sym == SDLK_l && m_session.State() != practice::GameState::Playing) { m_showLeaderboard = true; return; }
    if (sym == SDLK_t && menu) { StartTutorial(); return; }
    if (sym == SDLK_b && menu) { m_showBossSelect = true; return; }
    if (sym == SDLK_s && menu) { StartMode(practice::SessionMode::Survival); return; }
    // Enter / Esc only map to the session control calls; the rules are in PracticeSession.
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) { PressEscapeAction(quit); return; }  // AC_BACK: Android Back
    if (sym == SDLK_m) { ToggleMute(); return; }  // not a gameplay key: works in every state
    if (sym == SDLK_g) { ToggleRecipeHint(); return; }  // likewise
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) { PressEnterAction(); return; }

    invoker::InputAction action;
    if (MainPlayer::TranslateKey(e, action))
        ProcessAction(action);
}

// Ready / Game Over -> new session (ignored while Playing, per PracticeSession::PressEnter itself).
// Shared by the Enter key and every touch "start/restart" tap zone: one place, one rule.
void GameManager::PressEnterAction()
{
    unsigned seed = static_cast<unsigned>(std::time(NULL)) ^ (SDL_GetTicks() << 8);
    if (m_session.PressEnter(seed))
    {
        ResetVisualEffects();
        m_lastBestUpdate = { false, false, false };
        printf("[practice] session started (seed %u)\n", seed);
        if (m_recipeHint)
            m_session.MarkAssisted();  // started with the recipe hint: not ranked (spec §17)
        audio::Play(audio::Sfx::Start);
    }
}

// Ready -> quit the application. Playing / Game Over -> step back to Ready, never quit. Shared by the
// Esc key and every touch "back/quit" tap zone. A web page has no application to quit (stopping the
// main loop would only freeze the canvas), so there Esc on Ready does nothing.
void GameManager::PressEscapeAction(bool& quit)
{
    ResetVisualEffects();
    if (m_session.State() == practice::GameState::Playing)
        EndSession();  // leaving mid-session still counts towards the records (read before the reset)
    if (m_session.PressEscape())
    {
#ifndef __EMSCRIPTEN__
        quit = true;
#endif
    }
    else
        printf("[practice] back to Ready\n");
}

// Touch (SDL_FINGERDOWN) and mouse (SDL_MOUSEBUTTONDOWN) input, already converted to window-pixel
// coordinates by the caller. Hit-tests exactly the regions drawn by RenderTouchControls() and the
// Ready/Game Over/HUD text, then reuses the very same calls the keyboard uses - no separate rules.
void GameManager::HandlePointerDown(int x, int y, bool& quit)
{
    auto hit = [x, y](const SDL_Rect& r)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };

    bool menu = !m_tutorialActive && !m_bossActive && m_session.State() == practice::GameState::Ready;
    if (m_showRecipes || m_showLeaderboard)  // an overlay is open: a tap anywhere closes it
    {
        m_showRecipes = m_showLeaderboard = false;
        return;
    }
    if (m_showBossSelect)  // the boss list: a tap on a boss starts the fight, anywhere else closes the list
    {
        for (int i = 0; i < practice::BOSS_COUNT; ++i)
        {
            if (hit(BossSelectRect(i)))
            {
                StartBoss(i);
                return;
            }
        }
        m_showBossSelect = false;
        return;
    }
    if (!menu && hit(SOUND_BUTTON_RECT))  // sound on/off while playing (the menu has its own line for it)
    {
        ToggleMute();
        return;
    }
    if (!menu && !m_tutorialActive && hit(HINT_BUTTON_RECT))  // recipe hint on/off (not a tutorial button)
    {
        ToggleRecipeHint();
        return;
    }
    if (m_tutorialActive)
    {
        HandleTutorialPointer(x, y);
        return;
    }
    if (m_bossActive)
    {
        HandleBossPointer(x, y);
        return;
    }
    if (m_session.State() == practice::GameState::GameOver && hit(RECIPES_BUTTON_GAMEOVER_RECT))
    {
        m_showRecipes = true;
        return;
    }
    if (m_session.State() == practice::GameState::GameOver && hit(LEADERBOARD_BUTTON_GAMEOVER_RECT))
    {
        m_showLeaderboard = true;
        return;
    }

    switch (m_session.State())
    {
    case practice::GameState::Ready:
        for (int i = 0; i < MenuItemCount(); ++i)
        {
            if (hit(MenuItemRect(i)))
            {
                m_menuIndex = i;
                ActivateMenuItem(MenuItemAt(i), quit);
                return;
            }
        }
        if (hit(TOP3_PANEL_RECT))
            m_showLeaderboard = true;
        break;

    case practice::GameState::GameOver:
        if (hit(TOUCH_GAMEOVER_RESTART_RECT))
            PressEnterAction();
        else if (hit(TOUCH_GAMEOVER_MENU_RECT))
            PressEscapeAction(quit);
        break;

    case practice::GameState::Playing:
        if (hit(TOUCH_PLAYING_MENU_RECT))
        {
            PressEscapeAction(quit);
            return;
        }
        for (int i = 0; i < 6 && m_showTouchControls; ++i)  // hidden buttons are not clickable either
        {
            if (hit(kTouchButtons[i].rect))
            {
                ProcessAction(kTouchButtons[i].action);
                break;
            }
        }
        break;
    }
}

// Feeds one logical key to the session and prints the development log.
void GameManager::ProcessAction(invoker::InputAction action)
{
    // A correct cast removes the enemy inside Input() itself, so its position has to be read before that call.
    bool hadEnemy = m_session.Enemy().active;
    practice::Bounds enemyBody = hadEnemy ? practice::EnemyBounds(m_session.Enemy()) : practice::Bounds{ 0.0f, 0.0f, 0.0f, 0.0f };

    practice::InputResult r = m_session.Input(action);
    if (!r.accepted)  // Ready / Game Over: the gameplay keys do nothing
        return;

    switch (action)
    {
    case invoker::InputAction::Q: printf("=====Q===== "); break;
    case invoker::InputAction::W: printf("====W===== "); break;
    case invoker::InputAction::E: printf("====E====== "); break;
    default: break;
    }

    const char* label = r.cast != practice::CastOutcome::Correct ? NULL : r.kill.killed ? PointsText(r.kill.points) : "HIT";
    PresentInvokerResult(action, r.invoker, r.cast, hadEnemy, enemyBody, label);
    if (r.kill.killed)
        OnKill(r.kill, enemyBody);

    const invoker::InvokerState& inv = m_session.Invoker();
    const char* slotName = action == invoker::InputAction::D ? "D" : "F";
    if (r.invoker.event == invoker::InvokerEvent::Invoked)
    {
        printf("Slot D: %s, Slot F: %s\n",
            invoker::RecipeLetters(inv.GetSlot(invoker::Slot::D)).c_str(),
            invoker::RecipeLetters(inv.GetSlot(invoker::Slot::F)).c_str());
        printf("Current combination: %s\n", invoker::RecipeLetters(r.invoker.skill).c_str());
    }
    else if (r.invoker.event == invoker::InvokerEvent::Cast)
    {
        printf("Cast %s: %s\n", slotName, invoker::GetSkillDefinition(r.invoker.skill).name);
    }
    else if (r.invoker.event == invoker::InvokerEvent::CastEmpty)
    {
        printf("Cast %s: (empty)\n", slotName);
    }

    if (r.tornadoLaunched)
        printf("[practice] Tornado launched (judged when it hits the enemy)\n");
    LogOutcome(r.cast);
}

void GameManager::PresentInvokerResult(invoker::InputAction action, const invoker::InvokerResult& result,
    practice::CastOutcome cast, bool hadEnemy, const practice::Bounds& enemyBody, const char* label)
{
    // sounds: each orb has its element's sound, R only sounds when it really invoked, a judged cast sounds right /
    // wrong (OnCastJudged), and a cast that is not judged at once (Tornado launch, no enemy) just whooshes
    if (result.event == invoker::InvokerEvent::OrbAdded)
        audio::Play(action == invoker::InputAction::Q ? audio::Sfx::OrbQuas
            : action == invoker::InputAction::W ? audio::Sfx::OrbWex : audio::Sfx::OrbExort);
    else if (result.event == invoker::InvokerEvent::Invoked)
    {
        audio::Play(audio::Sfx::Invoke);
        m_orbFlash = PLAYER_ORB_FLASH;
    }
    else if (result.event == invoker::InvokerEvent::Cast && cast == practice::CastOutcome::None)
        audio::Play(audio::Sfx::Cast);

    if (result.event == invoker::InvokerEvent::Cast && result.skill == invoker::SkillId::GhostWalk)
        m_ghostWalkLeft = GHOST_WALK_DURATION;  // visual only; the cast is judged like any other spell
    // no-op for Tornado / Ghost Walk (own effects); in a boss fight Sun Strike, Chaos Meteor and EMP are drawn when
    // they land (PresentBossUpdate), not when they are cast
    if (result.event == invoker::InvokerEvent::Cast && !(m_bossActive && practice::BossSpellDelay(result.skill) > 0.0f))
        StartSkillVfx(result.skill, hadEnemy, enemyBody);
    if (cast != practice::CastOutcome::None)
        OnCastJudged(cast, enemyBody, label);
}

// The six touch buttons (Q/W/E/R/D/F), drawn only while Playing (matches the keyboard: those keys are
// no-ops in Ready/Game Over too). Reuses the already-loaded keyboard icons at a larger size - no new art.
void GameManager::RenderTouchControls()
{
    if (!IsPlayView() || !m_showTouchControls)
        return;

    Keyboard* icons[6] = { &m_keyQ, &m_keyW, &m_keyE, &m_keyR, &m_keyD, &m_keyF };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 6; ++i)
    {
        const SDL_Rect& r = kTouchButtons[i].rect;
        SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 150);
        SDL_RenderFillRect(m_screen, &r);
        if (i < 3)  // Q/W/E: a band of their element colour along the bottom, matching the HUD orbs
        {
            SDL_Color c = kOrbColors[i];
            SDL_Rect band = { r.x + 1, r.y + r.h - 8, r.w - 2, 7 };
            SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 220);
            SDL_RenderFillRect(m_screen, &band);
        }
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 90);
        SDL_RenderDrawRect(m_screen, &r);

        SDL_Texture* tex = icons[i]->p_object_;
        if (tex != NULL)
        {
            SDL_Rect src = { 0, 0, 32, 32 };  // the first (unpressed) frame of the 64x32, 2-frame sheet
            const int pad = 10;
            SDL_Rect dst = { r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2 };
            SDL_RenderCopy(m_screen, tex, &src, &dst);
        }
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    // tutorial: the button of the key to press now
    char expected = m_tutorialActive ? m_tutorial.ExpectedKey() : 0;
    const char letters[6] = { 'Q', 'W', 'E', 'R', 'D', 'F' };
    for (int i = 0; i < 6 && expected != 0; ++i)
        if (letters[i] == expected)
            RenderHighlight(kTouchButtons[i].rect);
}

// One log line per judged cast; shared by casts that are judged at once and by Tornado hits.
void GameManager::LogOutcome(practice::CastOutcome outcome)
{
    const practice::Stats& st = m_session.GetStats();
    if (outcome == practice::CastOutcome::Correct)
    {
        printf("[practice] correct cast: score %d, combo %d (best %d)\n", st.score, st.combo, st.bestCombo);
    }
    else if (outcome == practice::CastOutcome::Incorrect)
    {
        printf("[practice] incorrect cast: the enemy keeps coming (HP %d/%d)\n", st.hp, st.maxHp);
    }
}

void GameManager::LogUpdate(const practice::UpdateResult& result)
{
    const practice::Stats& st = m_session.GetStats();
    if (result.spawned)
    {
        const practice::ActiveEnemy& e = m_session.Enemy();
        printf("[practice] enemy #%d spawned\n", m_session.SpawnCount());
        if (m_debug)  // development only: never printed in normal play
        {
            printf("[debug] enemy #%d %s -> target %s (recipe %s), speed %.0f px/s\n",
                m_session.SpawnCount(), practice::GetEnemyDefinition(e.definition).name,
                invoker::GetSkillDefinition(e.target).name, invoker::RecipeLetters(e.target).c_str(), e.speed);
        }
    }
    if (result.cast != practice::CastOutcome::None)
    {
        printf("[practice] Tornado hit the enemy\n");
        LogOutcome(result.cast);
    }
    if (result.leaked)
    {
        printf("[practice] enemy reached the player: HP %d/%d, combo reset\n", st.hp, st.maxHp);
    }
    if (result.gameOver)
    {
        printf("[practice] GAME OVER: score %d, best combo %d, accuracy %.1f%% (%d/%d), survived %.1f s\n",
            st.score, st.bestCombo, st.Accuracy() * 100.0, st.correctCasts, st.TotalCasts(), st.survivalTime);
        m_lastRank = SubmitRun(st);
        if (m_lastRank > 0)
            printf("[practice] rank %d on this device\n", m_lastRank);
    }
}

// ------------------------------------------------------------------ drawing


// One active orb: a disc in its element colour (ice / lightning / fire) with a light core and its key letter.
void GameManager::RenderOrb(invoker::Orb orb, int centerX, int centerY)
{
    SDL_Color c = kOrbColors[static_cast<int>(orb)];
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 70);   // soft glow
    draw::FillCircle(m_screen, centerX, centerY, 26);
    SDL_SetRenderDrawColor(m_screen, 10, 12, 18, 255);     // dark rim
    draw::FillCircle(m_screen, centerX, centerY, 20);
    SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 255);
    draw::FillCircle(m_screen, centerX, centerY, 18);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 110);  // highlight, top-left
    draw::FillCircle(m_screen, centerX - 6, centerY - 6, 6);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const char* letter = orb == invoker::Orb::Quas ? "Q" : orb == invoker::Orb::Wex ? "W" : "E";
    const SDL_Color white = { 255, 255, 255, 255 };
    pixeltext::DrawShadowed(m_screen, letter, centerX - pixeltext::Width(letter, 2) / 2, centerY - 7, 2, white);
}

// The single active enemy, drawn where the Practice session says it is.
void GameManager::RenderEnemy()
{
    const practice::ActiveEnemy& e = ShownEnemy();
    if (!e.active)
        return;

    const practice::EnemyDefinition& def = practice::GetEnemyDefinition(e.definition);
    EnemyObject& sprite = m_enemySprites[e.definition];
    // e.x is the left edge of the visible body; the frame starts bodyLeft pixels earlier
    sprite.SetPos(static_cast<int>(e.x) - def.bodyLeft, static_cast<int>(practice::GROUND_LINE_Y) - def.feetRow);
    bool flash = m_enemyFlashLeft > 0.0f && sprite.p_object_ != NULL;  // red tint after a wrong cast
    if (sprite.p_object_ == NULL)
        return;
    if (flash)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 90, 90);
    else if (e.kind == practice::EnemyKind::Elite)  // PLAY: elites and bosses are larger and tinted (P3-2)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 200, 130);
    else if (e.kind == practice::EnemyKind::Boss)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 130, 130);
    if (e.scale > 1.0f)
        sprite.RenderFrameScaled(m_screen, static_cast<int>(e.x - def.bodyLeft * e.scale),
            static_cast<int>(practice::GROUND_LINE_Y - def.feetRow * e.scale), e.scale);
    else
        sprite.Render(m_screen);
    SDL_SetTextureColorMod(sprite.p_object_, 255, 255, 255);
}

// Loads the Tornado sprite sheet as a plain texture: no colour key (BaseObject::LoadImg would make grey pixels
// transparent) and nearest-neighbour scaling, so the pixel art stays crisp.
bool GameManager::LoadTornadoSheet()
{
    SDL_Surface* surface = IMG_Load(TORNADO_SHEET_PATH);
    if (surface == NULL)
    {
        printf("Failed to load Tornado sheet %s: %s\n", TORNADO_SHEET_PATH, IMG_GetError());
        return false;
    }
    bool sizeOk = surface->w == TORNADO_SHEET_COLUMNS * TORNADO_FRAME_SIZE && surface->h == TORNADO_SHEET_COLUMNS * TORNADO_FRAME_SIZE;
    if (!sizeOk)
        printf("Tornado sheet %s is %dx%d, expected %dx%d: not used\n", TORNADO_SHEET_PATH, surface->w, surface->h,
            TORNADO_SHEET_COLUMNS * TORNADO_FRAME_SIZE, TORNADO_SHEET_COLUMNS * TORNADO_FRAME_SIZE);
    else
        m_tornadoSheet = SDL_CreateTextureFromSurface(m_screen, surface);
    SDL_FreeSurface(surface);
    if (m_tornadoSheet == NULL)
        return false;

    SDL_SetTextureBlendMode(m_tornadoSheet, SDL_BLENDMODE_BLEND);   // transparent background
    SDL_SetTextureScaleMode(m_tornadoSheet, SDL_ScaleModeNearest);  // no smoothing
    return true;
}

// Tornado projectiles: the Practice session says where they are and which frame to show.
void GameManager::RenderTornadoes()
{
    if (m_tornadoSheet == NULL)
        return;

    const int size = static_cast<int>(TORNADO_FRAME_SIZE * TORNADO_DRAW_SCALE);  // same on both axes: no distortion
    auto draw = [&](const practice::Tornado& t)
    {
        int frame = practice::TornadoFrame(t.animTime);
        SDL_Rect src = { (frame % TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE, (frame / TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE,
            TORNADO_FRAME_SIZE, TORNADO_FRAME_SIZE };
        SDL_Rect dst = { static_cast<int>(t.x) - size / 2, static_cast<int>(t.y) - size / 2, size, size };
        SDL_RenderCopy(m_screen, m_tornadoSheet, &src, &dst);
    };
    for (int i = 0; i < m_session.ActiveTornadoCount(); ++i)
        draw(m_session.GetTornado(i));
    for (int i = 0; m_bossActive && i < m_boss.ProjectileCount(); ++i)  // a Deafening Blast is drawn by SkillVfx
        if (m_boss.GetProjectile(i).skill == invoker::SkillId::Tornado)
            draw(m_boss.GetProjectile(i).motion);
}

// One shared reset for every temporary visual effect: called on Esc (back to Ready) and on Enter (new session),
// exactly where m_ghostWalkLeft used to be cleared by itself.
void GameManager::ResetVisualEffects()
{
    m_ghostWalkLeft = 0.0f;
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
        m_skillVfx[i].left = 0.0f;
    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
        m_floatTexts[i].left = 0.0f;
    for (int i = 0; i < FEEDBACK_MAX_BURSTS; ++i)
        m_bursts[i].left = 0.0f;
    m_enemyFlashLeft = m_leakFlashLeft = m_shakeLeft = m_hpBlinkLeft = 0.0f;
}

// ------------------------------------------------------------------ feedback (presentation only)

// Correct: a gold ring where the enemy was and a rising "+1". Wrong: the enemy flashes red and "MISS" rises
// above it. `enemy` is where the enemy was when the cast (or the Tornado hit) was judged.
void GameManager::OnCastJudged(practice::CastOutcome outcome, const practice::Bounds& enemy, const char* label)
{
    int cx = static_cast<int>(enemy.x + enemy.w * 0.5f);
    int cy = static_cast<int>(enemy.y + enemy.h * 0.5f);
    // an enemy that is still entering from the right edge would put the text off screen: keep it readable
    const int margin = 60;
    int textX = cx < margin ? margin : (cx > SCREEN_WIDTH - margin ? SCREEN_WIDTH - margin : cx);
    bool correct = outcome == practice::CastOutcome::Correct;
    audio::Play(correct ? audio::Sfx::CastCorrect : audio::Sfx::CastWrong);
    if (correct)
    {
        for (int i = 0; i < FEEDBACK_MAX_BURSTS; ++i)
        {
            if (m_bursts[i].left <= 0.0f)
            {
                m_bursts[i] = { FEEDBACK_BURST_TIME, cx, cy };
                break;
            }
        }
    }
    else
    {
        m_enemyFlashLeft = FEEDBACK_ENEMY_FLASH;
    }

    const SDL_Color gold = { 255, 215, 80, 255 };
    const SDL_Color red = { 255, 90, 90, 255 };
    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
    {
        if (m_floatTexts[i].left <= 0.0f)
        {
            m_floatTexts[i] = { FEEDBACK_TEXT_TIME, textX, static_cast<int>(enemy.y) - 10,
                label != NULL ? label : correct ? "+1" : "MISS", correct ? gold : red };
            break;
        }
    }
}

// An enemy reached the player: a red frame, a short light shake, and the lost HP square blinks.
void GameManager::OnLeak(int lostHeartIndex)
{
    audio::Play(audio::Sfx::Leak);
    m_leakFlashLeft = FEEDBACK_LEAK_FLASH;
    m_shakeLeft = FEEDBACK_SHAKE_TIME;
    m_hpBlinkLeft = FEEDBACK_HP_BLINK;
    m_hpBlinkIndex = lostHeartIndex;
}

void GameManager::UpdateFeedback(float dt)
{
    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
        m_floatTexts[i].left -= dt;
    for (int i = 0; i < FEEDBACK_MAX_BURSTS; ++i)
        m_bursts[i].left -= dt;
    m_enemyFlashLeft -= dt;
    m_leakFlashLeft -= dt;
    m_shakeLeft -= dt;
    m_hpBlinkLeft -= dt;
}


void GameManager::RenderFeedback()
{
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < FEEDBACK_MAX_BURSTS; ++i)
    {
        const Burst& b = m_bursts[i];
        if (b.left <= 0.0f)
            continue;
        float progress = 1.0f - b.left / FEEDBACK_BURST_TIME;  // 0 -> 1
        SDL_SetRenderDrawColor(m_screen, 255, 215, 80, static_cast<Uint8>(220 * (1.0f - progress)));
        draw::Ring(m_screen, b.x, b.y, 12 + static_cast<int>(44 * progress), 3);
        draw::Ring(m_screen, b.x, b.y, 6 + static_cast<int>(24 * progress), 3);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
    {
        const FloatText& t = m_floatTexts[i];
        if (t.left <= 0.0f)
            continue;
        float progress = 1.0f - t.left / FEEDBACK_TEXT_TIME;
        int y = t.y - static_cast<int>(FEEDBACK_TEXT_RISE * progress);
        pixeltext::DrawShadowed(m_screen, t.text, t.x - pixeltext::Width(t.text, 3) / 2, y, 3, t.color);
    }
}

// Red frame around the screen after a leak, fading out.
void GameManager::RenderLeakFlash()
{
    if (m_leakFlashLeft <= 0.0f)
        return;
    const int thickness = 24;
    Uint8 alpha = static_cast<Uint8>(110 * (m_leakFlashLeft / FEEDBACK_LEAK_FLASH));
    SDL_Rect edges[4] = {
        { 0, 0, SCREEN_WIDTH, thickness },
        { 0, SCREEN_HEIGHT - thickness, SCREEN_WIDTH, thickness },
        { 0, thickness, thickness, SCREEN_HEIGHT - 2 * thickness },
        { SCREEN_WIDTH - thickness, thickness, thickness, SCREEN_HEIGHT - 2 * thickness } };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 230, 30, 30, alpha);
    SDL_RenderFillRects(m_screen, edges, 4);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
}

// This device's top 10 runs by survival time (the leaderboard until the online boards exist): a small text
// file next to the exe on desktop / in the app folder on Android, localStorage on the web. Unreadable or missing
// data is an empty list. (The old score-based top 10 is no longer read.)
void GameManager::LoadTopRuns()
{
    int raw[practice::TOP_RUNS * 2] = {};  // milliseconds, score, ...
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var parts = String(localStorage.getItem('threeElements_topTimes')).split(',');
            for (var i = 0; i < $1; ++i) {
                var p = (parts[i] || '').split(':');
                HEAP32[($0 >> 2) + i * 2] = Number(p[0]) | 0;
                HEAP32[($0 >> 2) + i * 2 + 1] = Number(p[1]) | 0;
            }
        } catch (e) {}
    }, raw, practice::TOP_RUNS);
#else
    FILE* f = fopen(SavePath("toptimes.txt").c_str(), "r");
    if (f != NULL)
    {
        for (int i = 0; i < practice::TOP_RUNS && fscanf(f, "%d %d", &raw[i * 2], &raw[i * 2 + 1]) == 2; ++i) {}
        fclose(f);
    }
#endif
    for (int i = 0; i < practice::TOP_RUNS; ++i)
        m_topRuns[i] = { 0.0f, 0 };
    for (int i = 0; i < practice::TOP_RUNS; ++i)  // re-inserted one by one, so a damaged file still comes out sorted
        if (raw[i * 2] > 0)
            practice::InsertTopRun(m_topRuns, { raw[i * 2] / 1000.0f, raw[i * 2 + 1] > 0 ? raw[i * 2 + 1] : 0 });
}

void GameManager::SaveTopRuns()
{
    int raw[practice::TOP_RUNS * 2];
    for (int i = 0; i < practice::TOP_RUNS; ++i)
    {
        raw[i * 2] = static_cast<int>(m_topRuns[i].survivalTime * 1000.0f);
        raw[i * 2 + 1] = m_topRuns[i].score;
    }
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var parts = [];
            for (var i = 0; i < $1; ++i)
                parts.push(HEAP32[($0 >> 2) + i * 2] + ':' + HEAP32[($0 >> 2) + i * 2 + 1]);
            localStorage.setItem('threeElements_topTimes', parts.join(','));
        } catch (e) {}
    }, raw, practice::TOP_RUNS);
#else
    FILE* f = fopen(SavePath("toptimes.txt").c_str(), "w");
    if (f != NULL)
    {
        for (int i = 0; i < practice::TOP_RUNS; ++i)
            fprintf(f, "%d %d\n", raw[i * 2], raw[i * 2 + 1]);
        fclose(f);
    }
#endif
}

// A finished run enters this device's top 10 if its survival time is good enough; saved at once.
int GameManager::SubmitRun(const practice::Stats& st)
{
    if (st.assisted)  // played with the recipe hint: not ranked
        return 0;
    if (m_session.Mode() == practice::SessionMode::Play)  // the PLAY board ranks by score (P3-5)
    {
        int playRank = practice::InsertPlayRun(m_playRuns, { st.score, st.survivalTime, st.bossesDefeated + 1 });
        if (playRank > 0)
            SavePlayRuns();
        return playRank;
    }
    int rank = practice::InsertTopRun(m_topRuns, { st.survivalTime, st.score });
    if (rank > 0)
        SaveTopRuns();
    return rank;
}

// Best Score / Best Combo / Best Survival Time (spec §13), stored the same way as the top-10 list: bests.txt
// next to the exe on desktop, localStorage on the web. Anything missing or unreadable counts as "no record"
// (EC-17); a browser that blocks storage (private mode) just keeps the records for this visit.
void GameManager::LoadBests()
{
    int raw[3] = { 0, 0, 0 };  // score, combo, survival time in milliseconds
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var values = String(localStorage.getItem('threeElements_bests')).split(',').map(Number);  // null -> NaN -> 0
            for (var i = 0; i < 3; ++i)
                HEAP32[($0 >> 2) + i] = values[i] | 0;
        } catch (e) {}
    }, raw);
#else
    FILE* f = fopen(SavePath("bests.txt").c_str(), "r");
    if (f != NULL)
    {
        if (fscanf(f, "%d %d %d", &raw[0], &raw[1], &raw[2]) != 3)
            raw[0] = raw[1] = raw[2] = 0;
        fclose(f);
    }
#endif
    m_bests.score = raw[0] > 0 ? raw[0] : 0;
    m_bests.combo = raw[1] > 0 ? raw[1] : 0;
    m_bests.survivalTime = raw[2] > 0 ? raw[2] / 1000.0f : 0.0f;
}

void GameManager::SaveBests(const practice::BestStats& bests)
{
    int raw[3] = { bests.score, bests.combo, static_cast<int>(bests.survivalTime * 1000.0f) };
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            localStorage.setItem('threeElements_bests',
                [HEAP32[$0 >> 2], HEAP32[($0 >> 2) + 1], HEAP32[($0 >> 2) + 2]].join(','));
        } catch (e) {}
    }, raw);
#else
    FILE* f = fopen(SavePath("bests.txt").c_str(), "w");
    if (f != NULL)
    {
        fprintf(f, "%d %d %d\n", raw[0], raw[1], raw[2]);
        fclose(f);
    }
#endif
}

// The app is going to the background (Android) and may be closed there without warning: write the records the
// current session has already beaten. m_bests itself is left alone, so the Game Over screen can still say
// "NEW BEST!" when the session really ends.
void GameManager::SaveRecordsSoFar()
{
    if (m_session.State() != practice::GameState::Playing || m_session.Mode() != practice::SessionMode::Survival)
        return;  // PLAY banks its gold as it is earned and has no other records
    practice::BestStats sofar = m_bests;
    if (practice::MergeBests(sofar, m_session.GetStats()).Any())
        SaveBests(sofar);
}

// Game coordinates (0..928, 0..544) of a touch. With a logical size set, SDL's renderer already rewrites
// SDL_FINGER* positions to 0..1 of the letterboxed game area (SDL_render.c, SDL_RendererEventWatch), so they only
// need scaling. (Converting them through the viewport again moved every touch on a letterboxed phone screen.)
void GameManager::TouchToGame(float normX, float normY, int& gameX, int& gameY)
{
    gameX = static_cast<int>(normX * SCREEN_WIDTH);
    gameY = static_cast<int>(normY * SCREEN_HEIGHT);
}

// A session is over, by Game Over or by Esc while Playing: its score, combo and survival time can set records.
void GameManager::EndSession()
{
    if (m_session.Mode() != practice::SessionMode::Survival)  // the records are Survival's (PLAY ranks by score)
    {
        m_lastBestUpdate = { false, false, false };
        return;
    }
    m_lastBestUpdate = practice::MergeBests(m_bests, m_session.GetStats());
    if (m_lastBestUpdate.Any())
    {
        SaveBests(m_bests);
        printf("[practice] new record(s):%s%s%s\n", m_lastBestUpdate.score ? " score" : "",
            m_lastBestUpdate.combo ? " combo" : "", m_lastBestUpdate.survivalTime ? " survival time" : "");
    }
}

// Sound on/off, remembered like the records: settings.txt next to the exe on desktop, localStorage on the web.
void GameManager::LoadSettings()
{
    int muted = 0;
    int tutorialDone = 0;
    int hint = 0;
#ifdef __EMSCRIPTEN__
    hint = EM_ASM_INT({
        try { return localStorage.getItem('threeElements_recipeHint') === '1' ? 1 : 0; } catch (e) { return 0; }
    });
    muted = EM_ASM_INT({
        try { return localStorage.getItem('threeElements_muted') === '1' ? 1 : 0; } catch (e) { return 0; }
    });
    tutorialDone = EM_ASM_INT({
        try { return localStorage.getItem('threeElements_tutorialDone') === '1' ? 1 : 0; } catch (e) { return 0; }
    });
#else
    FILE* f = fopen(SavePath("settings.txt").c_str(), "r");
    if (f != NULL)
    {
        if (fscanf(f, "muted %d tutorial %d hint %d", &muted, &tutorialDone, &hint) < 1)
            muted = 0;
        fclose(f);
    }
#endif
    audio::SetMuted(muted != 0);
    m_tutorialDone = tutorialDone != 0;
    m_recipeHint = hint != 0;
}

void GameManager::SaveSettings()
{
    int muted = audio::IsMuted() ? 1 : 0;
    int tutorialDone = m_tutorialDone ? 1 : 0;
    int hint = m_recipeHint ? 1 : 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            localStorage.setItem('threeElements_muted', $0 ? '1' : '0');
            localStorage.setItem('threeElements_tutorialDone', $1 ? '1' : '0');
            localStorage.setItem('threeElements_recipeHint', $2 ? '1' : '0');
        } catch (e) {}
    }, muted, tutorialDone, hint);
#else
    FILE* f = fopen(SavePath("settings.txt").c_str(), "w");
    if (f != NULL)
    {
        fprintf(f, "muted %d tutorial %d hint %d\n", muted, tutorialDone, hint);
        fclose(f);
    }
#endif
}

void GameManager::ToggleRecipeHint()
{
    m_recipeHint = !m_recipeHint;
    SaveSettings();
    if (m_recipeHint)
    {
        m_session.MarkAssisted();  // only while Playing: this run is no longer ranked
        if (m_bossActive)
            m_boss.MarkAssisted(); // only while Fighting: no best time for this fight
    }
    printf("[practice] recipe hint %s\n", m_recipeHint ? "on" : "off");
}

void GameManager::ToggleMute()
{
    audio::SetMuted(!audio::IsMuted());
    SaveSettings();
    printf("[audio] sound %s\n", audio::IsMuted() ? "off" : "on");
}

// "SOUND ON" / "SOUND OFF" under ACC (tap or click it, or press M; hidden when there is no audio device at all),
// and "HINT ON" / "HINT OFF" under it (G), the recipe hint of spec §17.
void GameManager::RenderSoundButton()
{
    if (!m_tutorialActive && !m_bossActive && m_session.State() == practice::GameState::Ready)
        return;  // on the Ready screen the menu has SOUND and RECIPE HINT lines instead
    const SDL_Color on = { 200, 235, 200, 255 };
    const SDL_Color off = { 170, 170, 180, 255 };
    if (m_audioReady)  // no audio device: no sound button
    {
        const SDL_Rect& r = SOUND_BUTTON_RECT;
        bool muted = audio::IsMuted();
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 160);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 110);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        const char* label = muted ? "SOUND OFF" : "SOUND ON";
        pixeltext::DrawShadowed(m_screen, label, r.x + (r.w - pixeltext::Width(label, 1)) / 2, r.y + (r.h - 7) / 2, 1,
            muted ? off : on);
    }

    if (m_tutorialActive)  // the tutorial shows its own keys; the hint belongs to Practice
        return;
    const SDL_Rect& h = HINT_BUTTON_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 160);
    SDL_RenderFillRect(m_screen, &h);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 110);
    SDL_RenderDrawRect(m_screen, &h);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    const char* hintLabel = m_recipeHint ? "HINT ON" : "HINT OFF";
    pixeltext::DrawShadowed(m_screen, hintLabel, h.x + (h.w - pixeltext::Width(hintLabel, 1)) / 2, h.y + (h.h - 7) / 2, 1,
        m_recipeHint ? on : off);
}

// ------------------------------------------------------------------ main menu and leaderboard

int GameManager::MenuItemCount() const
{
#ifdef __EMSCRIPTEN__
    return MENU_QUIT;      // a web page has nothing to quit: no QUIT line
#else
    return MENU_QUIT + 1;
#endif
}

MenuItem GameManager::MenuItemAt(int index) const { return static_cast<MenuItem>(index); }

SDL_Rect GameManager::MenuItemRect(int index) const
{
    SDL_Rect r = { MENU_X, MENU_Y + index * MENU_STEP, MENU_W, MENU_ITEM_H };
    return r;
}

void GameManager::ActivateMenuItem(MenuItem item, bool& quit)
{
    switch (item)
    {
    case MENU_PLAY:        StartMode(practice::SessionMode::Play); break;
    case MENU_SURVIVAL:    StartMode(practice::SessionMode::Survival); break;
    case MENU_TUTORIAL:    StartTutorial(); break;
    case MENU_BOSS:        m_showBossSelect = true; break;
    case MENU_RECIPES:     m_showRecipes = true; break;
    case MENU_LEADERBOARD: m_showLeaderboard = true; break;
    case MENU_SOUND:       ToggleMute(); break;
    case MENU_HINT:        ToggleRecipeHint(); break;
    case MENU_QUIT:        PressEscapeAction(quit); break;
    }
}

// One option per line with its key on the right; the highlighted line has a gold frame and a marker. TUTORIAL
// pulses until it has been finished once.
void GameManager::RenderMenu()
{
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 140, 145, 160, 255 };
    for (int i = 0; i < MenuItemCount(); ++i)
    {
        MenuItem item = MenuItemAt(i);
        SDL_Rect r = MenuItemRect(i);
        bool selected = i == m_menuIndex;
        const char* label = "";
        const char* key = "";
        switch (item)
        {
        case MENU_PLAY:        label = "PLAY";        key = "ENTER"; break;
        case MENU_SURVIVAL:    label = "SURVIVAL";    key = "S"; break;
        case MENU_TUTORIAL:    label = "TUTORIAL";    key = "T"; break;
        case MENU_BOSS:        label = "BOSS FIGHTS"; key = "B"; break;
        case MENU_RECIPES:     label = "RECIPES";     key = "H"; break;
        case MENU_LEADERBOARD: label = "LEADERBOARD"; key = "L"; break;
        case MENU_SOUND:       label = audio::IsMuted() ? "SOUND: OFF" : "SOUND: ON"; key = "M"; break;
        case MENU_HINT:        label = m_recipeHint ? "RECIPE HINT: ON" : "RECIPE HINT: OFF"; key = "G"; break;
        case MENU_QUIT:        label = "QUIT";        key = "ESC"; break;
        }
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 16, 18, 28, selected ? 235 : 170);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, selected ? 255 : 120, selected ? 210 : 125, selected ? 90 : 140, 220);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        const int textY = r.y + (MENU_ITEM_H - 14) / 2;
        if (selected)
            pixeltext::DrawShadowed(m_screen, ">", r.x + 10, textY, 2, gold);
        pixeltext::DrawShadowed(m_screen, label, r.x + 30, textY, 2, selected ? gold : white);
        if (!m_showTouchControls)  // keyboard hints mean nothing on a touch screen
            pixeltext::DrawShadowed(m_screen, key, r.x + r.w - pixeltext::Width(key, 1) - 10, r.y + (MENU_ITEM_H - 7) / 2, 1, grey);
        if (item == MENU_TUTORIAL && !m_tutorialDone)
            RenderHighlight(r);
    }
}

// The best three runs beside the menu; a tap on the panel opens the top 10.
void GameManager::RenderTop3Panel()
{
    const SDL_Rect& panel = TOP3_PANEL_RECT;
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 140, 145, 160, 255 };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 200);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 160);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    pixeltext::DrawShadowed(m_screen, "TOP 3  PLAY", panel.x + 12, panel.y + 10, 2, gold);  // PLAY ranks by score (P3-5)
    char buf[48];
    if (m_playRuns[0].score <= 0)
        pixeltext::DrawShadowed(m_screen, "NO RUNS YET", panel.x + 12, panel.y + 50, 2, grey);
    for (int i = 0; i < 3 && m_playRuns[i].score > 0; ++i)
    {
        snprintf(buf, sizeof(buf), "%d.  %d", i + 1, m_playRuns[i].score);
        pixeltext::DrawShadowed(m_screen, buf, panel.x + 12, panel.y + 40 + i * 26, 2, i == 0 ? gold : white);
        snprintf(buf, sizeof(buf), "STAGE %d", m_playRuns[i].stage);
        pixeltext::DrawShadowed(m_screen, buf, panel.x + panel.w - pixeltext::Width(buf, 1) - 12, panel.y + 44 + i * 26, 1, grey);
    }
    const char* more = m_showTouchControls ? "TAP FOR TOP 10" : "L / CLICK: TOP 10";
    pixeltext::DrawShadowed(m_screen, more, panel.x + 12, panel.y + panel.h - 22, 2, grey);
}

// The top 10 by survival time, and the player's own records underneath. (Online boards come later: Google Play
// Games on Android, a server for the web; this list is this device's.)
void GameManager::RenderLeaderboard()
{
    DimScreen(190);
    const SDL_Rect panel = { 110, 30, SCREEN_WIDTH - 220, SCREEN_HEIGHT - 60 };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 235);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 150, 155, 170, 255 };
    pixeltext::DrawCentered(m_screen, "LEADERBOARD", SCREEN_WIDTH, panel.y + 16, 3, gold);
    pixeltext::DrawCentered(m_screen, "THIS DEVICE", SCREEN_WIDTH, panel.y + 48, 1, grey);

    // two boards side by side: PLAY by score (spec §26), SURVIVAL by time
    const int leftX = panel.x + 36, rightX = panel.x + panel.w / 2 + 24;
    pixeltext::DrawShadowed(m_screen, "PLAY - SCORE", leftX, panel.y + 66, 2, gold);
    pixeltext::DrawShadowed(m_screen, "SURVIVAL - TIME", rightX, panel.y + 66, 2, gold);
    char buf[64], time[16];
    for (int i = 0; i < practice::TOP_RUNS; ++i)
    {
        int y = panel.y + 94 + i * 26;
        SDL_Color c = i < 3 ? white : grey;
        if (m_playRuns[i].score > 0)
        {
            snprintf(buf, sizeof(buf), "%2d. %d", i + 1, m_playRuns[i].score);
            pixeltext::DrawShadowed(m_screen, buf, leftX, y, 2, c);
            snprintf(buf, sizeof(buf), "STAGE %d", m_playRuns[i].stage);
            pixeltext::DrawShadowed(m_screen, buf, leftX + 170, y + 4, 1, grey);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%2d. --", i + 1);
            pixeltext::DrawShadowed(m_screen, buf, leftX, y, 2, grey);
        }
        bool empty = m_topRuns[i].survivalTime <= 0.0f;
        FormatTime(time, sizeof(time), m_topRuns[i].survivalTime);
        snprintf(buf, sizeof(buf), "%2d. %s", i + 1, empty ? "--:--" : time);
        pixeltext::DrawShadowed(m_screen, buf, rightX, y, 2, empty ? grey : c);
    }
    snprintf(buf, sizeof(buf), "GOLD BANK %d", m_goldBank);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, panel.y + panel.h - 54, 2, gold);
    pixeltext::DrawCentered(m_screen, "PRESS ANY KEY OR TAP TO CLOSE", SCREEN_WIDTH, panel.y + panel.h - 28, 2, grey);
}

// The player (no magic ring under the feet any more, owner 2026-10-01): the loaded orbs circling the character (behind it on the far
// half of the orbit, in front on the near half) and the Injoker picture gliding up and down between them.
void GameManager::RenderPlayer()
{
    float t = SDL_GetTicks() / 1000.0f;
    // the running sheet carries its own motion; only the standing picture glides up and down
    int bob = m_playerRunSheet != NULL ? 0 : static_cast<int>(PLAYER_BOB_PX * std::sin(t * PLAYER_BOB_SPEED));
    const int ground = static_cast<int>(practice::GROUND_LINE_Y);
    const int cx = PLAYER_BODY_CENTER_X;

    // the orbs the player has loaded (0..3), spaced evenly on the orbit; sin(angle) < 0 is the far side
    const invoker::InvokerState& inv = ShownInvoker();
    int count = IsPlayView() ? inv.OrbCount() : 0;
    auto drawOrb = [&](int i)
    {
        float a = t * PLAYER_ORBIT_SPEED + 6.2831853f * i / 3.0f;
        int x = cx + static_cast<int>(PLAYER_ORBIT_RX * std::cos(a));
        int y = PLAYER_ORBIT_Y + bob + static_cast<int>(PLAYER_ORBIT_RY * std::sin(a));
        bool far = std::sin(a) < 0.0f;
        SDL_Color c = kOrbColors[static_cast<int>(inv.GetOrb(i))];
        float flare = m_orbFlash > 0.0f ? m_orbFlash / PLAYER_ORB_FLASH : 0.0f;
        int r = PLAYER_ORB_RADIUS - (far ? 2 : 0);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, static_cast<Uint8>(60 + 120 * flare));
        draw::FillCircle(m_screen, x, y, r + 5 + static_cast<int>(6 * flare));
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, far ? 190 : 255);
        draw::FillCircle(m_screen, x, y, r);
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, far ? 90 : 160);
        draw::FillCircle(m_screen, x - r / 3, y - r / 3, r / 3 + 1);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    };
    for (int i = 0; i < count; ++i)
        if (std::sin(t * PLAYER_ORBIT_SPEED + 6.2831853f * i / 3.0f) < 0.0f)
            drawOrb(i);

    if (m_playerRunSheet != NULL)  // the running Injoker (owner 2026-10-01)
    {
        int frame = static_cast<int>(t * PLAYER_RUN_FPS) % PLAYER_RUN_FRAMES;
        SDL_Rect src = { (frame % PLAYER_RUN_COLUMNS) * PLAYER_RUN_FRAME, (frame / PLAYER_RUN_COLUMNS) * PLAYER_RUN_FRAME,
            PLAYER_RUN_FRAME, PLAYER_RUN_FRAME };
        SDL_Rect dst = { PLAYER_DRAW_X - PLAYER_RUN_BODY_LEFT * PLAYER_RUN_DRAW / PLAYER_RUN_FRAME,
            ground - PLAYER_RUN_FEET_ROW * PLAYER_RUN_DRAW / PLAYER_RUN_FRAME - 2, PLAYER_RUN_DRAW, PLAYER_RUN_DRAW };
        SDL_RenderCopy(m_screen, m_playerRunSheet, &src, &dst);
    }
    else if (m_hasPlayer)
    {
        int w = m_player.ImageWidth() * PLAYER_DRAW_H / m_player.ImageHeight();
        m_player.RenderScaled(m_screen, { PLAYER_DRAW_X, ground - PLAYER_DRAW_H - 2 + bob, w, PLAYER_DRAW_H });
    }

    for (int i = 0; i < count; ++i)
        if (std::sin(t * PLAYER_ORBIT_SPEED + 6.2831853f * i / 3.0f) >= 0.0f)
            drawOrb(i);
}

// ------------------------------------------------------------------ tutorial (spec §24)

void GameManager::StartTutorial()
{
    ResetVisualEffects();
    m_showRecipes = false;
    unsigned seed = static_cast<unsigned>(std::time(NULL)) ^ (SDL_GetTicks() << 8);
    m_tutorial.Start(seed);
    m_tutorialActive = true;
    m_tutorialWrongFlash = m_tutorialSpawnFlash = 0.0f;
    audio::Play(audio::Sfx::Start);
    printf("[tutorial] started\n");
}

// Back to the Ready screen, or straight into Practice from the end card. Finishing is remembered.
void GameManager::ExitTutorial(bool startPractice)
{
    if (m_tutorial.IsDone() && !m_tutorialDone)
    {
        m_tutorialDone = true;
        SaveSettings();
    }
    m_tutorialActive = false;
    m_showRecipes = false;
    ResetVisualEffects();
    printf("[tutorial] left\n");
    if (startPractice)
        StartMode(practice::SessionMode::Play);
}

// Keys while the tutorial runs: NEXT (Enter / Space) on cards, the gameplay keys on key steps and the run, H for the
// recipe list during the run, Esc / Back to leave. On the end card Enter starts Practice.
void GameManager::HandleTutorialKey(SDL_Keycode sym, const SDL_Event& e)
{
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) { ExitTutorial(false); return; }
    if (sym == SDLK_m) { ToggleMute(); return; }
    if (sym == SDLK_h && m_tutorial.Step().kind == practice::TutorialStepKind::Run) { m_showRecipes = true; return; }
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
    {
        if (m_tutorial.IsDone())
            ExitTutorial(true);
        else if (m_tutorial.Next())
            audio::Play(audio::Sfx::Cast);  // a soft whoosh for turning the card
        return;
    }
    invoker::InputAction action;
    if (MainPlayer::TranslateKey(e, action))
        ProcessTutorialAction(action);
}

bool GameManager::HandleTutorialPointer(int x, int y)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    const practice::TutorialStep& step = m_tutorial.Step();
    if (hit(TOUCH_PLAYING_MENU_RECT))  // "ESC  EXIT"
    {
        ExitTutorial(false);
        return true;
    }
    if (step.kind == practice::TutorialStepKind::Done)
    {
        if (hit(TUTORIAL_PLAY_RECT)) { ExitTutorial(true); return true; }
        if (hit(TUTORIAL_MENU_RECT)) { ExitTutorial(false); return true; }
        return false;
    }
    if (step.kind == practice::TutorialStepKind::Card && hit(TUTORIAL_NEXT_RECT))
    {
        m_tutorial.Next();
        audio::Play(audio::Sfx::Cast);
        return true;
    }
    for (int i = 0; i < 6 && m_showTouchControls; ++i)
    {
        if (hit(kTouchButtons[i].rect))
        {
            ProcessTutorialAction(kTouchButtons[i].action);
            return true;
        }
    }
    return false;
}

void GameManager::ProcessTutorialAction(invoker::InputAction action)
{
    bool hadEnemy = m_tutorial.Enemy().active;
    practice::Bounds enemyBody = hadEnemy ? practice::EnemyBounds(m_tutorial.Enemy()) : practice::Bounds{ 0.0f, 0.0f, 0.0f, 0.0f };
    practice::TutorialInputResult r = m_tutorial.Input(action);
    if (r.wrongKey)
    {
        m_tutorialWrongFlash = TUTORIAL_WRONG_FLASH;
        audio::Play(audio::Sfx::CastWrong);
        return;
    }
    if (!r.accepted)
        return;
    PresentInvokerResult(action, r.invoker, r.cast, hadEnemy, enemyBody);
    if (r.stepAdvanced && m_tutorial.IsDone())
        audio::Play(audio::Sfx::Start);
}

// The TARGET hint's text position and the box around its text and icon (see RenderTargetHint).
SDL_Rect GameManager::TargetHintArea(invoker::SkillId target, int& textX) const
{
    char buf[64];
    snprintf(buf, sizeof(buf), "TARGET: %s", invoker::GetSkillDefinition(target).name);
    int width = pixeltext::Width(buf, 2);
    textX = SCREEN_WIDTH - width - 16;
    SDL_Rect area = { textX, 78, width, 100 + SKILL_HINT_SIZE + 4 - 78 };
    return area;
}

void GameManager::RenderHighlight(SDL_Rect rect)
{
    float t = SDL_GetTicks() / 1000.0f;
    Uint8 alpha = static_cast<Uint8>(150 + 105 * (0.5f + 0.5f * std::sin(t * 7.0f)));
    const int pad = 5;
    rect.x -= pad; rect.y -= pad; rect.w += pad * 2; rect.h += pad * 2;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 60, alpha);
    for (int i = 0; i < 3; ++i)
    {
        SDL_Rect r = { rect.x - i, rect.y - i, rect.w + i * 2, rect.h + i * 2 };
        SDL_RenderDrawRect(m_screen, &r);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
}

void GameManager::RenderButton(const SDL_Rect& rect, const char* label, bool pulse)
{
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 220);
    SDL_RenderFillRect(m_screen, &rect);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &rect);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    const SDL_Color gold = { 255, 210, 90, 255 };
    pixeltext::DrawShadowed(m_screen, label, rect.x + (rect.w - pixeltext::Width(label, 2)) / 2, rect.y + (rect.h - 14) / 2, 2, gold);
    if (pulse)
        RenderHighlight(rect);
}

// The tutorial layer: highlights around what the step is about, then the card panel at the top with the lesson,
// the text, the key sequence (key steps), the run's progress and hearts, and NEXT / PLAY / MENU.
void GameManager::RenderTutorial()
{
    const practice::TutorialStep& step = m_tutorial.Step();
    const practice::ActiveEnemy& enemy = m_tutorial.Enemy();
    bool run = step.kind == practice::TutorialStepKind::Run;

    // ---- highlights (in the run only for a moment when an enemy appears)
    int flags = step.highlight;
    if (run && m_tutorialSpawnFlash <= 0.0f)
        flags = practice::HIGHLIGHT_NONE;
    if (flags & practice::HIGHLIGHT_ORBS)
        RenderHighlight({ (elementPos[0].first + HudShiftX()) - 26, elementPos[0].second - 26, (elementPos[2].first + HudShiftX()) - (elementPos[0].first + HudShiftX()) + 52, 52 });
    if ((flags & practice::HIGHLIGHT_ENEMY) && enemy.active)
    {
        practice::Bounds b = practice::EnemyBounds(enemy);
        RenderHighlight({ static_cast<int>(b.x), static_cast<int>(b.y), static_cast<int>(b.w), static_cast<int>(b.h) });
    }
    if ((flags & practice::HIGHLIGHT_TARGET) && enemy.active)
    {
        int textX = 0;
        RenderHighlight(TargetHintArea(enemy.target, textX));
    }
    for (int i = 0; i < 2; ++i)
    {
        int bit = i == 0 ? practice::HIGHLIGHT_SLOT_D : practice::HIGHLIGHT_SLOT_F;
        if (flags & bit)
            RenderHighlight({ (skillPos[i].first + HudShiftX()) - 2, skillPos[i].second - 38, SKILL_SLOT_SIZE + 4, SKILL_SLOT_SIZE + 40 });
    }

    // ---- the panel
    const SDL_Rect& panel = TUTORIAL_PANEL_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 12, 14, 22, 225);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color grey = { 150, 155, 170, 255 };
    const SDL_Color red = { 255, 90, 90, 255 };
    const SDL_Color green = { 120, 220, 130, 255 };
    char buf[64];
    if (step.kind != practice::TutorialStepKind::Done)
    {
        snprintf(buf, sizeof(buf), "TUTORIAL  %d/%d", step.lesson, practice::TUTORIAL_LESSON_COUNT);
        pixeltext::Draw(m_screen, buf, panel.x + 12, panel.y + 6, 2, grey);  // scale 2: readable on a phone too
    }
    pixeltext::DrawShadowed(m_screen, step.line1, panel.x + 12, panel.y + 30, 2, step.kind == practice::TutorialStepKind::Done ? gold : white);
    if (step.line2[0] != '\0')
        pixeltext::DrawShadowed(m_screen, step.line2, panel.x + 12, panel.y + 52, 2, white);

    if (step.kind == practice::TutorialStepKind::Keys)
    {
        // the whole sequence, one box per key: done = green, now = gold (pulsing frame), later = grey
        int count = static_cast<int>(std::strlen(step.keys));
        for (int i = 0; i < count; ++i)
        {
            SDL_Rect box = { panel.x + 12 + i * 44, panel.y + 80, 36, 36 };
            bool done = i < m_tutorial.KeysDone();
            bool now = i == m_tutorial.KeysDone();
            SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 255);
            SDL_RenderFillRect(m_screen, &box);
            SDL_Color c = done ? green : now ? gold : grey;
            SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 255);
            SDL_RenderDrawRect(m_screen, &box);
            char key[2] = { step.keys[i], '\0' };
            pixeltext::DrawShadowed(m_screen, key, box.x + (box.w - pixeltext::Width(key, 3)) / 2, box.y + 8, 3, c);
            if (now)
                RenderHighlight(box);
        }
        if (m_tutorialWrongFlash > 0.0f)
        {
            char want[2] = { m_tutorial.ExpectedKey(), '\0' };
            snprintf(buf, sizeof(buf), "PRESS %s", want);
            pixeltext::DrawShadowed(m_screen, buf, panel.x + 12 + count * 44 + 16, panel.y + 90, 2, red);
        }
    }
    else if (step.kind == practice::TutorialStepKind::Card)
    {
        RenderButton(TUTORIAL_NEXT_RECT, "NEXT  >", true);
    }
    else if (run)
    {
        snprintf(buf, sizeof(buf), "DEFEATED %d/%d", m_tutorial.RunDefeated(), practice::TUTORIAL_RUN_ENEMIES);
        pixeltext::DrawShadowed(m_screen, buf, panel.x + 12, panel.y + 94, 2, gold);
        // hearts, like the HP squares of Practice (a lost one blinks)
        pixeltext::DrawShadowed(m_screen, "HP", panel.x + 300, panel.y + 94, 2, white);
        for (int i = 0; i < practice::START_HP; ++i)
        {
            SDL_Rect box = { panel.x + 340 + i * 24, panel.y + 92, 18, 18 };
            bool blinkOn = i == m_hpBlinkIndex && m_hpBlinkLeft > 0.0f && static_cast<int>(m_hpBlinkLeft * 16.0f) % 2 == 0;
            if (blinkOn)
                SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
            else if (i < m_tutorial.Hearts())
                SDL_SetRenderDrawColor(m_screen, 220, 50, 50, 255);
            else
                SDL_SetRenderDrawColor(m_screen, 40, 20, 24, 255);
            SDL_RenderFillRect(m_screen, &box);
            SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
            SDL_RenderDrawRect(m_screen, &box);
        }
    }
    else  // Done
    {
        pixeltext::DrawShadowed(m_screen, "NEED HELP WITH RECIPES? TURN ON RECIPE HINT (G).", panel.x + 12, panel.y + 74, 1, grey);
        RenderButton(TUTORIAL_PLAY_RECT, "PLAY", true);
        RenderButton(TUTORIAL_MENU_RECT, "MENU", false);
    }
}

// "RECIPES (H)" button on the Ready / Game Over screens.
void GameManager::RenderRecipesButton(const SDL_Rect& rect)
{
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 200);
    SDL_RenderFillRect(m_screen, &rect);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &rect);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    const SDL_Color gold = { 255, 210, 90, 255 };
    const char* label = "RECIPES  (H)";
    pixeltext::DrawShadowed(m_screen, label, rect.x + (rect.w - pixeltext::Width(label, 2)) / 2, rect.y + (rect.h - 14) / 2, 2, gold);
}

// A small orb for the recipe list: element colour, dark rim, key letter.
void GameManager::RenderSmallOrb(invoker::Orb orb, int centerX, int centerY)
{
    SDL_Color c = kOrbColors[static_cast<int>(orb)];
    SDL_SetRenderDrawColor(m_screen, 10, 12, 18, 255);
    draw::FillCircle(m_screen, centerX, centerY, 11);
    SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 255);
    draw::FillCircle(m_screen, centerX, centerY, 9);
    const char* letter = orb == invoker::Orb::Quas ? "Q" : orb == invoker::Orb::Wex ? "W" : "E";
    const SDL_Color white = { 255, 255, 255, 255 };
    pixeltext::DrawShadowed(m_screen, letter, centerX - pixeltext::Width(letter, 1) / 2, centerY - 3, 1, white);
}

// The recipe reference (spec §17: allowed outside play only): the 10 skills in catalog order, two columns of
// five, each with its icon, name and the three orbs it needs.
void GameManager::RenderRecipes()
{
    DimScreen(190);
    const SDL_Rect panel = { 64, 36, SCREEN_WIDTH - 128, SCREEN_HEIGHT - 72 };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 235);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color grey = { 170, 170, 185, 255 };
    pixeltext::DrawCentered(m_screen, "RECIPES", SCREEN_WIDTH, panel.y + 18, 3, gold);

    const int top = panel.y + 66;
    const int rowHeight = 74;
    const int columnX[2] = { panel.x + 48, panel.x + panel.w / 2 + 24 };
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
    {
        const invoker::SkillDefinition& def = invoker::GetSkillDefinition(static_cast<invoker::SkillId>(i));
        int x = columnX[i / 5];
        int y = top + (i % 5) * rowHeight;
        m_skillIcons[i].RenderAt(m_screen, x, y, SKILL_RECIPE_SIZE);
        pixeltext::DrawShadowed(m_screen, def.name, x + SKILL_RECIPE_SIZE + 14, y + 2, 2, white);

        // the orbs in a fixed Q, W, E order (the order never matters in play, only the counts do)
        int orbX = x + SKILL_RECIPE_SIZE + 14 + 11;
        const int counts[3] = { def.recipe.quas, def.recipe.wex, def.recipe.exort };
        for (int element = 0; element < 3; ++element)
        {
            for (int n = 0; n < counts[element]; ++n)
            {
                RenderSmallOrb(static_cast<invoker::Orb>(element), orbX, y + 32);
                orbX += 26;
            }
        }
    }
    pixeltext::DrawCentered(m_screen, "PRESS ANY KEY OR TAP TO CLOSE", SCREEN_WIDTH, panel.y + panel.h - 26, 2, grey);
}

// Starts (or restarts) the code-drawn effect of `skill`, if it has one (see SkillVfx.h). An effect placed on the
// enemy or aimed at it needs an enemy: with none, nothing is shown, same convention as casting into an empty
// encounter. The direction toward the enemy is captured once here (no homing).
void GameManager::StartSkillVfx(invoker::SkillId skill, bool hadEnemy, const practice::Bounds& enemyBody)
{
    int i = static_cast<int>(skill);
    if (!skillvfx::HasEffect(skill) || (skillvfx::NeedsEnemy(skill) && !hadEnemy))
        return;

    skillvfx::Effect& e = m_skillVfx[i];
    int enemyX = static_cast<int>(enemyBody.x + enemyBody.w * 0.5f);
    int enemyY = static_cast<int>(enemyBody.y + enemyBody.h * 0.5f);
    e.left = skillvfx::Duration(skill);
    e.x = skillvfx::StartsAtPlayer(skill) ? PLAYER_BODY_CENTER_X : enemyX;
    e.y = skillvfx::StartsAtPlayer(skill) ? PLAYER_BODY_CENTER_Y : enemyY;
    e.seed = ++m_skillVfxCount;
    e.dirX = 1.0f;
    e.dirY = 0.0f;
    e.tx = static_cast<int>(enemyBody.x);           // the Forge Spirit sprite walks up to the enemy's front
    e.ty = static_cast<int>(enemyBody.y + enemyBody.h);
    if (skill == invoker::SkillId::ChaosMeteor && m_meteorSheet != NULL)
        e.left = METEOR_FALL_TIME + METEOR_BLAST_TIME;
    if (skill == invoker::SkillId::ForgeSpirit && m_forgeSheet != NULL)
        e.left = FORGE_WALK_TIME + FORGE_ATTACK_TIME;
    if (hadEnemy)
    {
        float dx = static_cast<float>(enemyX - e.x);
        float dy = static_cast<float>(enemyY - e.y);
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f)
        {
            e.dirX = dx / len;
            e.dirY = dy / len;
        }
    }
}

bool GameManager::LoadGhostWalkSheet()
{
    SDL_Surface* surface = IMG_Load(GHOST_WALK_SHEET_PATH);
    if (surface == NULL)
    {
        printf("Failed to load Ghost Walk sheet %s: %s\n", GHOST_WALK_SHEET_PATH, IMG_GetError());
        return false;
    }
    const int size = GHOST_WALK_SHEET_COLUMNS * GHOST_WALK_FRAME_SIZE;
    if (surface->w == size && surface->h == size)
        m_ghostWalkSheet = SDL_CreateTextureFromSurface(m_screen, surface);
    else
        printf("Ghost Walk sheet %s is %dx%d, expected %dx%d: not used\n", GHOST_WALK_SHEET_PATH, surface->w, surface->h, size, size);
    SDL_FreeSurface(surface);
    if (m_ghostWalkSheet == NULL)
        return false;

    SDL_SetTextureBlendMode(m_ghostWalkSheet, SDL_BLENDMODE_BLEND);  // transparent background
    SDL_SetTextureAlphaMod(m_ghostWalkSheet, GHOST_WALK_ALPHA);
    return true;
}

// The Ghost Walk aura: one ring animation looping while m_ghostWalkLeft runs down. Nothing to draw otherwise.
void GameManager::RenderGhostWalk()
{
    if (m_ghostWalkSheet == NULL || m_ghostWalkLeft <= 0.0f)
        return;

    float age = GHOST_WALK_DURATION - m_ghostWalkLeft;
    int frame = GHOST_WALK_FIRST_FRAME + static_cast<int>(age * GHOST_WALK_FPS) % GHOST_WALK_FRAME_COUNT;
    SDL_Rect src = { (frame % GHOST_WALK_SHEET_COLUMNS) * GHOST_WALK_FRAME_SIZE, (frame / GHOST_WALK_SHEET_COLUMNS) * GHOST_WALK_FRAME_SIZE,
        GHOST_WALK_FRAME_SIZE, GHOST_WALK_FRAME_SIZE };
    SDL_Rect dst = { PLAYER_BODY_CENTER_X - GHOST_WALK_DRAW_SIZE / 2, PLAYER_BODY_CENTER_Y - GHOST_WALK_DRAW_SIZE / 2,
        GHOST_WALK_DRAW_SIZE, GHOST_WALK_DRAW_SIZE };
    SDL_RenderCopy(m_screen, m_ghostWalkSheet, &src, &dst);
}

// The code-drawn skill effects, each for as long as StartSkillVfx() set.
void GameManager::RenderSkillVfx()
{
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
    {
        invoker::SkillId skill = static_cast<invoker::SkillId>(i);
        const skillvfx::Effect& e = m_skillVfx[i];
        if (skill == invoker::SkillId::ChaosMeteor && m_meteorSheet != NULL)
        {
            if (e.left > 0.0f)
                RenderMeteorSprite(METEOR_FALL_TIME + METEOR_BLAST_TIME - e.left, e.x, e.y);
        }
        else if (skill == invoker::SkillId::ForgeSpirit && m_forgeSheet != NULL)
        {
            if (e.left > 0.0f)
                RenderForgeSprite(FORGE_WALK_TIME + FORGE_ATTACK_TIME - e.left, e);
        }
        else
            skillvfx::Render(m_screen, skill, e);
    }
}

SDL_Texture* GameManager::LoadSheet(const char* path, int size)
{
    SDL_Surface* surface = IMG_Load(path);
    if (surface == NULL)
    {
        printf("Failed to load %s: %s\n", path, IMG_GetError());
        return NULL;
    }
    SDL_Texture* texture = NULL;
    if (surface->w == size && surface->h == size)
    {
        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);  // palette + transparency -> RGBA
        if (rgba != NULL)
        {
            texture = SDL_CreateTextureFromSurface(m_screen, rgba);
            SDL_FreeSurface(rgba);
        }
    }
    else
        printf("%s is %dx%d, expected %dx%d: not used\n", path, surface->w, surface->h, size, size);
    SDL_FreeSurface(surface);
    if (texture != NULL)
    {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);  // drawn smaller than the 256 px frames: smoothed
    }
    return texture;
}

// Chaos Meteor: the burning rock comes down from the upper left onto (x, y), then the blast plays there.
void GameManager::RenderMeteorSprite(float age, int x, int y)
{
    if (age < 0.0f)
        return;
    int frame;
    int cx = x, cy = y;
    if (age < METEOR_FALL_TIME)
    {
        float f = age / METEOR_FALL_TIME;
        frame = static_cast<int>(f * METEOR_FALL_FRAMES);
        cx = x + static_cast<int>(METEOR_FROM_X * (1.0f - f));
        cy = y + static_cast<int>(METEOR_FROM_Y * (1.0f - f));
    }
    else
    {
        float b = (age - METEOR_FALL_TIME) / METEOR_BLAST_TIME;
        frame = METEOR_FALL_FRAMES + static_cast<int>(b * METEOR_BLAST_FRAMES);
    }
    const int last = METEOR_FALL_FRAMES + METEOR_BLAST_FRAMES - 1;
    frame = frame < 0 ? 0 : (frame > last ? last : frame);
    SDL_Rect src = { (frame % METEOR_COLUMNS) * METEOR_FRAME, (frame / METEOR_COLUMNS) * METEOR_FRAME, METEOR_FRAME, METEOR_FRAME };
    SDL_Rect dst = { cx - METEOR_ANCHOR_X * METEOR_DRAW / METEOR_FRAME, cy - METEOR_ANCHOR_Y * METEOR_DRAW / METEOR_FRAME,
        METEOR_DRAW, METEOR_DRAW };
    SDL_RenderCopy(m_screen, m_meteorSheet, &src, &dst);
}

// Forge Spirit: the fire spirit walks from the player to the enemy (e.tx, e.ty = the enemy's front, on the ground),
// then lunges and bursts into flames.
void GameManager::RenderForgeSprite(float age, const skillvfx::Effect& e)
{
    if (age < 0.0f)
        return;
    const int ground = static_cast<int>(practice::GROUND_LINE_Y);
    const int fromX = PLAYER_BODY_CENTER_X + 40;
    const int toX = e.tx - FORGE_DRAW / 3;
    int frame, footX;
    if (age < FORGE_WALK_TIME)
    {
        float f = age / FORGE_WALK_TIME;
        footX = fromX + static_cast<int>((toX - fromX) * f);
        frame = static_cast<int>(age * 16.0f) % FORGE_WALK_FRAMES;
    }
    else
    {
        float a = (age - FORGE_WALK_TIME) / FORGE_ATTACK_TIME;
        footX = toX;
        frame = FORGE_WALK_FRAMES + static_cast<int>(a * FORGE_ATTACK_FRAMES);
        if (frame > FORGE_WALK_FRAMES + FORGE_ATTACK_FRAMES - 1)
            frame = FORGE_WALK_FRAMES + FORGE_ATTACK_FRAMES - 1;
    }
    SDL_Rect src = { (frame % FORGE_COLUMNS) * FORGE_FRAME, (frame / FORGE_COLUMNS) * FORGE_FRAME, FORGE_FRAME, FORGE_FRAME };
    SDL_Rect dst = { footX - FORGE_DRAW / 2, ground - FORGE_FEET_ROW * FORGE_DRAW / FORGE_FRAME, FORGE_DRAW, FORGE_DRAW };
    SDL_RenderCopy(m_screen, m_forgeSheet, &src, &dst);
}

// Current Q/W/E orbs and the two invoked spells (D = newest, F = previous). Never shows recipes or targets.
// Only while Playing: centred, it would otherwise sit under the Ready / Game Over text.
void GameManager::RenderInvokerHud()
{
    if (!IsPlayView())
        return;

    const invoker::InvokerState& inv = ShownInvoker();

    // three orb sockets: empty ones are a faint ring, so the player sees how many orbs are loaded
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = inv.OrbCount(); i < 3; ++i)
    {
        SDL_SetRenderDrawColor(m_screen, 200, 200, 210, 90);
        draw::FillCircle(m_screen, (elementPos[i].first + HudShiftX()), elementPos[i].second, 20);
        SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 140);
        draw::FillCircle(m_screen, (elementPos[i].first + HudShiftX()), elementPos[i].second, 17);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    for (int i = 0; i < inv.OrbCount(); ++i)
        RenderOrb(inv.GetOrb(i), (elementPos[i].first + HudShiftX()), elementPos[i].second);

    // frames for the two slots, so an empty slot is visible too (the icons are cut-outs, the frame is their tile)
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 2; ++i)
    {
        SDL_Rect frame = { (skillPos[i].first + HudShiftX()) - 2, skillPos[i].second - 2, SKILL_SLOT_SIZE + 4, SKILL_SLOT_SIZE + 4 };
        SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 170);
        SDL_RenderFillRect(m_screen, &frame);
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 70);
        SDL_RenderDrawRect(m_screen, &frame);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    m_keyD.SetPos((skillPos[0].first + HudShiftX()) + 16, skillPos[0].second - 36);
    m_keyD.Render(m_screen);
    m_keyF.SetPos((skillPos[1].first + HudShiftX()) + 16, skillPos[1].second - 36);
    m_keyF.Render(m_screen);

    invoker::SkillId spellD = inv.GetSlot(invoker::Slot::D);
    invoker::SkillId spellF = inv.GetSlot(invoker::Slot::F);
    if (spellD != invoker::SkillId::None)
        m_skillIcons[static_cast<int>(spellD)].RenderAt(m_screen, (skillPos[0].first + HudShiftX()), skillPos[0].second, SKILL_SLOT_SIZE);
    if (spellF != invoker::SkillId::None)
        m_skillIcons[static_cast<int>(spellF)].RenderAt(m_screen, (skillPos[1].first + HudShiftX()), skillPos[1].second, SKILL_SLOT_SIZE);
}

// "83%" or "--" until the first judged cast, and "mm:ss": shared by the HUD and the Game Over screen
static void FormatAccuracy(char* out, size_t size, const practice::Stats& st)
{
    if (st.TotalCasts() > 0)
        snprintf(out, size, "%d%%", static_cast<int>(st.Accuracy() * 100.0 + 0.5));
    else
        snprintf(out, size, "--");
}

static void FormatTime(char* out, size_t size, float seconds)
{
    int total = static_cast<int>(seconds);
    snprintf(out, size, "%02d:%02d", total / 60, total % 60);
}

// HP, score, combo, best combo, accuracy, survival time.
void GameManager::RenderStatsHud()
{
    const practice::Stats& st = m_session.GetStats();
    const SDL_Color white = { 255, 255, 255, 255 };
    char buf[64];

    if (m_bossActive)
    {
        RenderBossHud();
        return;
    }
    if (!m_tutorialActive && m_session.State() == practice::GameState::Ready)
        return;  // the Ready screen shows the logo there; the numbers are all zero anyway
    if (m_tutorialActive)  // the tutorial has no score: its card panel takes the top of the screen
    {
        const SDL_Color grey = { 200, 200, 210, 255 };
        pixeltext::DrawShadowed(m_screen, "ESC  EXIT", SCREEN_WIDTH - pixeltext::Width("ESC  EXIT", 2) - 16, 42, 2, grey);
        return;
    }

    // row 1: HP squares, score, combo, best combo
    pixeltext::DrawShadowed(m_screen, "HP", 16, 12, 2, white);
    for (int i = 0; i < st.maxHp; ++i)
    {
        SDL_Rect box = { 56 + i * 24, 10, 18, 18 };
        // the square just lost by a leak blinks white for a moment (8 blinks per second)
        bool blinkOn = i == m_hpBlinkIndex && m_hpBlinkLeft > 0.0f && static_cast<int>(m_hpBlinkLeft * 16.0f) % 2 == 0;
        if (blinkOn)
            SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
        else if (i < st.hp)
            SDL_SetRenderDrawColor(m_screen, 220, 50, 50, 255);
        else
            SDL_SetRenderDrawColor(m_screen, 40, 20, 24, 255);
        SDL_RenderFillRect(m_screen, &box);
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
        SDL_RenderDrawRect(m_screen, &box);
    }
    snprintf(buf, sizeof(buf), "SCORE %d", st.score);
    pixeltext::DrawShadowed(m_screen, buf, 190, 12, 2, white);
    snprintf(buf, sizeof(buf), "COMBO %d", st.combo);
    pixeltext::DrawShadowed(m_screen, buf, 400, 12, 2, white);
    bool play = m_session.Mode() == practice::SessionMode::Play;
    const SDL_Color goldColor = { 255, 210, 90, 255 };
    if (play)  // PLAY: the gold of this run instead of the best combo (P3-5)
        snprintf(buf, sizeof(buf), "GOLD %d", st.gold);
    else
        snprintf(buf, sizeof(buf), "BEST %d", st.bestCombo);
    pixeltext::DrawShadowed(m_screen, buf, 590, 12, 2, play ? goldColor : white);

    // row 2: accuracy and survival time
    char value[16];
    FormatAccuracy(value, sizeof(value), st);
    snprintf(buf, sizeof(buf), "ACC %s", value);
    pixeltext::DrawShadowed(m_screen, buf, 16, 42, 2, white);
    FormatTime(value, sizeof(value), st.survivalTime);
    snprintf(buf, sizeof(buf), "TIME %s", value);
    pixeltext::DrawShadowed(m_screen, buf, 190, 42, 2, white);
    if (play)
    {
        snprintf(buf, sizeof(buf), "STAGE %d", st.bossesDefeated + 1);
        pixeltext::DrawShadowed(m_screen, buf, 400, 42, 2, white);
        // the runes still running (P3-4)
        char runes[64] = "";
        if (m_session.FrostLeft() > 0.0f)
            snprintf(runes + strlen(runes), sizeof(runes) - strlen(runes), "FROST %d  ", static_cast<int>(m_session.FrostLeft()) + 1);
        if (m_session.DoubleLeft() > 0.0f)
            snprintf(runes + strlen(runes), sizeof(runes) - strlen(runes), "X2 %d  ", static_cast<int>(m_session.DoubleLeft()) + 1);
        if (m_session.HasShield())
            snprintf(runes + strlen(runes), sizeof(runes) - strlen(runes), "SHIELD");
        const SDL_Color cyan = { 110, 210, 255, 255 };
        pixeltext::DrawShadowed(m_screen, runes, 190, 68, 2, cyan);
    }

    // reminder of the control that leaves the session (only while playing)
    if (m_session.State() == practice::GameState::Playing)
    {
        const SDL_Color grey = { 200, 200, 210, 255 };
        pixeltext::DrawShadowed(m_screen, "ESC  MENU", SCREEN_WIDTH - pixeltext::Width("ESC  MENU", 2) - 16, 42, 2, grey);
    }
}

void GameManager::DimScreen(Uint8 alpha)
{
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, alpha);
    SDL_RenderFillRect(m_screen, NULL);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
}

// The main menu screen: logo, name, the options, and the top 3 beside them (owner 2026-09-30: fewer things at
// once, one option per line).
void GameManager::RenderReadyScreen()
{
    const SDL_Color gold = { 255, 210, 90, 255 };
    DimScreen(150);
    // the logo (the app icon's art) and the name
    if (m_titleLogo.ImageHeight() > 0)
    {
        int w = m_titleLogo.ImageWidth() * TITLE_LOGO_H / m_titleLogo.ImageHeight();
        m_titleLogo.RenderScaled(m_screen, { (SCREEN_WIDTH - w) / 2, 14, w, TITLE_LOGO_H });
    }
    pixeltext::DrawCentered(m_screen, "INJOKER", SCREEN_WIDTH, 188, 5, gold);
    RenderMenu();
    RenderTop3Panel();
}

void GameManager::RenderGameOverScreen()
{
    if (m_session.Mode() == practice::SessionMode::Play)
    {
        RenderPlayGameOver();
        return;
    }
    const practice::Stats& st = m_session.GetStats();
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color red = { 235, 70, 70, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    char buf[64];

    const SDL_Color gold = { 255, 210, 90, 255 };
    DimScreen(170);
    pixeltext::DrawCentered(m_screen, "GAME OVER", SCREEN_WIDTH, 64, 8, red);

    // this session's numbers; a line that set a new record turns gold and says so
    snprintf(buf, sizeof(buf), m_lastBestUpdate.score ? "SCORE %d  NEW BEST!" : "SCORE %d", st.score);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 145, 3, m_lastBestUpdate.score ? gold : white);
    snprintf(buf, sizeof(buf), m_lastBestUpdate.combo ? "BEST COMBO %d  NEW BEST!" : "BEST COMBO %d", st.bestCombo);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 178, 3, m_lastBestUpdate.combo ? gold : white);
    char value[16];
    FormatAccuracy(value, sizeof(value), st);
    snprintf(buf, sizeof(buf), "ACCURACY %s", value);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 211, 3, white);
    FormatTime(value, sizeof(value), st.survivalTime);
    snprintf(buf, sizeof(buf), m_lastBestUpdate.survivalTime ? "SURVIVED %s  NEW BEST!" : "SURVIVED %s", value);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 244, 3, m_lastBestUpdate.survivalTime ? gold : white);

    FormatTime(value, sizeof(value), m_bests.survivalTime);
    snprintf(buf, sizeof(buf), "RECORDS  SCORE %d   COMBO %d   TIME %s", m_bests.score, m_bests.combo, value);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 282, 2, grey);

    if (st.assisted)
        pixeltext::DrawCentered(m_screen, "RECIPE HINT WAS ON - NOT RANKED", SCREEN_WIDTH, 312, 2, grey);
    else if (m_lastRank > 0)
    {
        snprintf(buf, sizeof(buf), "RANK #%d ON THIS DEVICE", m_lastRank);
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 312, 2, gold);
    }

    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO RESTART", SCREEN_WIDTH, 350, 3, white);
    pixeltext::DrawCentered(m_screen, "ESC  MENU", SCREEN_WIDTH, 395, 2, grey);
    RenderRecipesButton(RECIPES_BUTTON_GAMEOVER_RECT);
    RenderButton(LEADERBOARD_BUTTON_GAMEOVER_RECT, "LEADERBOARD  (L)", false);
}

// Shows which skill the active enemy requires. Owner decision (2026-09-23): always on for every player,
// not just --debug builds; see GAMEPLAY_SPEC.md E-8 and the "Target-skill hint" rule in §17.
void GameManager::RenderTargetHint()
{
    const practice::ActiveEnemy& e = ShownEnemy();
    if (!e.active)
        return;

    const SDL_Color yellow = { 255, 235, 60, 255 };
    char buf[64];
    snprintf(buf, sizeof(buf), "TARGET: %s", invoker::GetSkillDefinition(e.target).name);
    int textX = 0;
    SDL_Rect area = TargetHintArea(e.target, textX);
    pixeltext::DrawShadowed(m_screen, buf, textX, 78, 2, yellow);

    // Recipe hint (spec §17, off by default, not in the tutorial): the three orbs of the recipe between the name
    // and the icon, the same coloured orbs as the HUD and the recipe list; the icon moves down to make room.
    bool showRecipe = m_recipeHint && !m_tutorialActive;
    int tileY = showRecipe ? 126 : 100;
    if (showRecipe)
    {
        const invoker::Recipe& r = invoker::GetSkillDefinition(e.target).recipe;
        const int counts[3] = { r.quas, r.wex, r.exort };
        int x = area.x + area.w / 2 - 26;
        for (int element = 0; element < 3; ++element)
            for (int n = 0; n < counts[element]; ++n, x += 26)
                RenderSmallOrb(static_cast<invoker::Orb>(element), x, 111);
    }

    // the skill's icon below the text, centred under it, large enough to read at a glance (owner 2026-09-30)
    SDL_Rect tile = { area.x + (area.w - SKILL_HINT_SIZE - 4) / 2, tileY, SKILL_HINT_SIZE + 4, SKILL_HINT_SIZE + 4 };
    if (tile.x + tile.w > SCREEN_WIDTH - 16)  // short names: keep the tile inside the right margin
        tile.x = SCREEN_WIDTH - 16 - tile.w;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 170);
    SDL_RenderFillRect(m_screen, &tile);
    SDL_SetRenderDrawColor(m_screen, 255, 235, 60, 160);
    SDL_RenderDrawRect(m_screen, &tile);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    m_skillIcons[static_cast<int>(e.target)].RenderAt(m_screen, tile.x + 2, tile.y + 2, SKILL_HINT_SIZE);

    // PLAY elites and bosses (P3-3): the kind above, and the whole chain in small icons below (broken ones marked,
    // the current one highlighted)
    if (e.chainLength > 1)
    {
        const SDL_Color orange = { 255, 170, 70, 255 };
        const SDL_Color red = { 255, 90, 90, 255 };
        const char* kind = e.kind == practice::EnemyKind::Boss ? "BOSS" : "ELITE";
        const int size = 36, gap = 18;
        int x0 = SCREEN_WIDTH - 16 - (e.chainLength * size + (e.chainLength - 1) * gap);
        int y = tile.y + tile.h + 8;
        pixeltext::DrawShadowed(m_screen, kind, x0 - 12 - pixeltext::Width(kind, 2), y + 11, 2,
            e.kind == practice::EnemyKind::Boss ? red : orange);
        const SDL_Color grey = { 190, 190, 200, 255 };
        for (int i = 0; i < e.chainLength; ++i)
        {
            SDL_Rect cell = { x0 + i * (size + gap), y, size, size };
            SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 190);
            SDL_RenderFillRect(m_screen, &cell);
            SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
            m_skillIcons[static_cast<int>(e.chain[i])].RenderAt(m_screen, cell.x + 2, cell.y + 2, size - 4);
            bool done = i < e.chainStep;
            if (done)
            {
                SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 140);
                SDL_RenderFillRect(m_screen, &cell);
                SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(m_screen, 90, 230, 110, 255);
            }
            else
                SDL_SetRenderDrawColor(m_screen, 120, 125, 140, 255);
            SDL_RenderDrawRect(m_screen, &cell);
            if (i == e.chainStep)
                RenderHighlight(cell);
            if (i + 1 < e.chainLength)
                pixeltext::DrawShadowed(m_screen, ">", cell.x + size + gap / 2 - 5, cell.y + 11, 2, grey);
        }
    }
}

// ------------------------------------------------------------------ PLAY mode (spec §26)

// The points of a kill as floating text; only these values exist (1 / 3 / 10, doubled by Double Damage).
static const char* PointsText(int points)
{
    switch (points)
    {
    case 1:  return "+1";
    case 2:  return "+2";
    case 3:  return "+3";
    case 6:  return "+6";
    case 10: return "+10";
    case 20: return "+20";
    default: return "+";
    }
}

void GameManager::StartMode(practice::SessionMode mode)
{
    m_session.SetMode(mode);
    printf("[practice] mode: %s\n", mode == practice::SessionMode::Play ? "PLAY" : "SURVIVAL");
    PressEnterAction();
}

// An enemy was finished in PLAY: its gold goes to the bank at once (so it is never lost), and a boss announces the
// new stage and its rune (P3-4, P3-5, P3-6).
void GameManager::OnKill(const practice::KillReport& kill, const practice::Bounds& enemy)
{
    if (kill.gold > 0)
    {
        m_goldBank += kill.gold;
        SaveGold();
        snprintf(m_goldText, sizeof(m_goldText), "+%d GOLD", kill.gold);
        const SDL_Color gold = { 255, 200, 60, 255 };
        int x = static_cast<int>(enemy.x + enemy.w * 0.5f);
        x = x < 110 ? 110 : (x > SCREEN_WIDTH - 110 ? SCREEN_WIDTH - 110 : x);
        for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
        {
            if (m_floatTexts[i].left <= 0.0f)
            {
                m_floatTexts[i] = { FEEDBACK_TEXT_TIME * 1.5f, x, static_cast<int>(enemy.y) - 44, m_goldText, gold };
                break;
            }
        }
    }
    if (kill.kind == practice::EnemyKind::Boss && m_session.Mode() == practice::SessionMode::Play)
    {
        snprintf(m_stageText, sizeof(m_stageText), "BOSS DEFEATED - STAGE %d", m_session.GetStats().bossesDefeated + 1);
        switch (kill.rune)
        {
        case practice::Rune::Regeneration: m_announce = "RUNE: REGENERATION  +1 LIFE"; break;
        case practice::Rune::Frost:        m_announce = "RUNE: FROST  ENEMIES SLOWED"; break;
        case practice::Rune::DoubleDamage: m_announce = "RUNE: DOUBLE DAMAGE  SCORE X2"; break;
        case practice::Rune::Bounty:       m_announce = "RUNE: BOUNTY  +25 GOLD"; break;
        case practice::Rune::Shield:       m_announce = "RUNE: SHIELD  NEXT HIT BLOCKED"; break;
        default:                           m_announce = NULL; break;
        }
        m_announceLeft = ANNOUNCE_TIME;
        audio::Play(audio::Sfx::Start);
        printf("[play] boss defeated: stage %d, rune %d, gold bank %d\n", m_session.GetStats().bossesDefeated + 1,
            static_cast<int>(kill.rune), m_goldBank);
    }
}

void GameManager::RenderAnnouncement()
{
    if (m_announceLeft <= 0.0f || m_session.State() != practice::GameState::Playing)
        return;
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color cyan = { 110, 210, 255, 255 };
    pixeltext::DrawCentered(m_screen, m_stageText, SCREEN_WIDTH, 96, 3, gold);
    if (m_announce != NULL)
        pixeltext::DrawCentered(m_screen, m_announce, SCREEN_WIDTH, 128, 2, cyan);
}

// The PLAY top 10 (score, time, stage) and the gold bank: playruns.txt / gold.txt next to the exe (the app folder
// on Android), localStorage on the web, like the other records.
void GameManager::LoadPlayRuns()
{
    int raw[practice::TOP_RUNS * 3] = {};  // score, milliseconds, stage, ...
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var parts = String(localStorage.getItem('threeElements_playRuns')).split(',');
            for (var i = 0; i < $1; ++i) {
                var p = (parts[i] || '').split(':');
                for (var k = 0; k < 3; ++k)
                    HEAP32[($0 >> 2) + i * 3 + k] = Number(p[k]) | 0;
            }
        } catch (e) {}
    }, raw, practice::TOP_RUNS);
#else
    FILE* f = fopen(SavePath("playruns.txt").c_str(), "r");
    if (f != NULL)
    {
        for (int i = 0; i < practice::TOP_RUNS && fscanf(f, "%d %d %d", &raw[i * 3], &raw[i * 3 + 1], &raw[i * 3 + 2]) == 3; ++i) {}
        fclose(f);
    }
#endif
    for (int i = 0; i < practice::TOP_RUNS; ++i)
        m_playRuns[i] = { 0, 0.0f, 0 };
    for (int i = 0; i < practice::TOP_RUNS; ++i)  // re-inserted one by one, so a damaged file still comes out sorted
        if (raw[i * 3] > 0)
            practice::InsertPlayRun(m_playRuns, { raw[i * 3], raw[i * 3 + 1] / 1000.0f, raw[i * 3 + 2] > 0 ? raw[i * 3 + 2] : 1 });
}

void GameManager::SavePlayRuns()
{
    int raw[practice::TOP_RUNS * 3];
    for (int i = 0; i < practice::TOP_RUNS; ++i)
    {
        raw[i * 3] = m_playRuns[i].score;
        raw[i * 3 + 1] = static_cast<int>(m_playRuns[i].time * 1000.0f);
        raw[i * 3 + 2] = m_playRuns[i].stage;
    }
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var parts = [];
            for (var i = 0; i < $1; ++i)
                parts.push(HEAP32[($0 >> 2) + i * 3] + ':' + HEAP32[($0 >> 2) + i * 3 + 1] + ':' + HEAP32[($0 >> 2) + i * 3 + 2]);
            localStorage.setItem('threeElements_playRuns', parts.join(','));
        } catch (e) {}
    }, raw, practice::TOP_RUNS);
#else
    FILE* f = fopen(SavePath("playruns.txt").c_str(), "w");
    if (f != NULL)
    {
        for (int i = 0; i < practice::TOP_RUNS; ++i)
            fprintf(f, "%d %d %d\n", raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2]);
        fclose(f);
    }
#endif
}

void GameManager::LoadGold()
{
    int gold = 0;
#ifdef __EMSCRIPTEN__
    gold = EM_ASM_INT({
        try { return Number(localStorage.getItem('threeElements_gold')) | 0; } catch (e) { return 0; }
    });
#else
    FILE* f = fopen(SavePath("gold.txt").c_str(), "r");
    if (f != NULL)
    {
        if (fscanf(f, "%d", &gold) != 1)
            gold = 0;
        fclose(f);
    }
#endif
    m_goldBank = gold > 0 ? gold : 0;
}

void GameManager::SaveGold()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try { localStorage.setItem('threeElements_gold', String($0)); } catch (e) {}
    }, m_goldBank);
#else
    FILE* f = fopen(SavePath("gold.txt").c_str(), "w");
    if (f != NULL)
    {
        fprintf(f, "%d\n", m_goldBank);
        fclose(f);
    }
#endif
}

// PLAY's Game Over: score, stage and kills, the gold of the run and the bank, the time, and the rank.
void GameManager::RenderPlayGameOver()
{
    const practice::Stats& st = m_session.GetStats();
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color red = { 235, 70, 70, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    char buf[64], value[16];

    DimScreen(170);
    pixeltext::DrawCentered(m_screen, "GAME OVER", SCREEN_WIDTH, 64, 8, red);
    bool best = m_lastRank == 1 && !st.assisted;
    snprintf(buf, sizeof(buf), best ? "SCORE %d  NEW BEST!" : "SCORE %d", st.score);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 145, 3, best ? gold : white);
    snprintf(buf, sizeof(buf), "STAGE %d   KILLS %d", st.bossesDefeated + 1, st.kills);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 178, 3, white);
    snprintf(buf, sizeof(buf), "GOLD +%d   BANK %d", st.gold, m_goldBank);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 211, 3, gold);
    FormatTime(value, sizeof(value), st.survivalTime);
    snprintf(buf, sizeof(buf), "SURVIVED %s", value);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 244, 3, white);
    snprintf(buf, sizeof(buf), "BEST SCORE %d", m_playRuns[0].score);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 282, 2, grey);
    if (st.assisted)
        pixeltext::DrawCentered(m_screen, "RECIPE HINT WAS ON - NOT RANKED", SCREEN_WIDTH, 312, 2, grey);
    else if (m_lastRank > 0)
    {
        snprintf(buf, sizeof(buf), "RANK #%d ON THIS DEVICE", m_lastRank);
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 312, 2, gold);
    }
    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO RESTART", SCREEN_WIDTH, 350, 3, white);
    pixeltext::DrawCentered(m_screen, "ESC  MENU", SCREEN_WIDTH, 395, 2, grey);
    RenderRecipesButton(RECIPES_BUTTON_GAMEOVER_RECT);
    RenderButton(LEADERBOARD_BUTTON_GAMEOVER_RECT, "LEADERBOARD  (L)", false);
}

// ------------------------------------------------------------------ boss mode (spec §25)

void GameManager::StartBoss(int boss)
{
    ResetVisualEffects();
    m_showBossSelect = false;
    m_boss.Start(boss);
    m_bossActive = true;
    m_bossSelect = m_boss.BossIndex();
    m_bossNewBest = false;
    if (m_recipeHint)
        m_boss.MarkAssisted();  // fought with the recipe hint: no best time (B-12)
    audio::Play(audio::Sfx::Start);
    printf("[boss] fight against %s\n", m_boss.Def().name);
}

// Leaves the fight (or its result screen) for the boss list; Esc there goes on to the menu.
void GameManager::ExitBoss()
{
    m_bossActive = false;
    m_showBossSelect = true;
    ResetVisualEffects();
    printf("[boss] back to the boss list\n");
}

void GameManager::HandleBossSelectKey(SDL_Keycode sym)
{
    if (sym == SDLK_UP || sym == SDLK_DOWN)
        m_bossSelect = (m_bossSelect + (sym == SDLK_UP ? practice::BOSS_COUNT - 1 : 1)) % practice::BOSS_COUNT;
    else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
        StartBoss(m_bossSelect);
    else if (sym >= SDLK_1 && sym < SDLK_1 + practice::BOSS_COUNT)
        StartBoss(static_cast<int>(sym - SDLK_1));
    else if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK || sym == SDLK_b)
        m_showBossSelect = false;
    else if (sym == SDLK_m)
        ToggleMute();
    else if (sym == SDLK_g)
        ToggleRecipeHint();
}

// Keys in a fight: the gameplay keys, M / G, Esc / Back to the boss list. On the result screen Enter fights the
// same boss again.
void GameManager::HandleBossKey(SDL_Keycode sym, const SDL_Event& e)
{
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) { ExitBoss(); return; }
    if (sym == SDLK_m) { ToggleMute(); return; }
    if (sym == SDLK_g) { ToggleRecipeHint(); return; }
    if (m_boss.State() != practice::BossState::Fighting)
    {
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
            StartBoss(m_boss.BossIndex());
        return;
    }
    invoker::InputAction action;
    if (MainPlayer::TranslateKey(e, action))
        ProcessBossAction(action);
}

bool GameManager::HandleBossPointer(int x, int y)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    if (m_boss.State() != practice::BossState::Fighting)
    {
        if (hit(TOUCH_GAMEOVER_RESTART_RECT)) { StartBoss(m_boss.BossIndex()); return true; }
        if (hit(TOUCH_GAMEOVER_MENU_RECT)) { ExitBoss(); return true; }
        return false;
    }
    if (hit(TOUCH_PLAYING_MENU_RECT))  // "ESC  BACK"
    {
        ExitBoss();
        return true;
    }
    for (int i = 0; i < 6 && m_showTouchControls; ++i)
    {
        if (hit(kTouchButtons[i].rect))
        {
            ProcessBossAction(kTouchButtons[i].action);
            return true;
        }
    }
    return false;
}

void GameManager::ProcessBossAction(invoker::InputAction action)
{
    practice::Bounds body = BossDrawnBody();  // effects aimed at the boss go where it is drawn
    practice::BossInputResult r = m_boss.Input(action);
    if (!r.accepted)
        return;
    PresentInvokerResult(action, r.invoker, practice::CastOutcome::None, true, body);
    if (r.attemptStarted)
        printf("[boss] combo attempt started\n");
    if (r.fail != practice::ComboFail::None)
        OnBossFail(r.fail);
}

void GameManager::PresentBossUpdate(const practice::BossUpdateResult& r)
{
    const SDL_Color gold = { 255, 215, 80, 255 };
    if (r.lifted)
        audio::Play(audio::Sfx::Cast);
    bool missShown = false;
    for (int i = 0; i < r.impactCount; ++i)
    {
        const practice::BossImpact& impact = r.impacts[i];
        practice::Bounds body = m_boss.Body();
        if (practice::BossSpellDelay(impact.skill) > 0.0f)  // the delayed spells appear when they land, where they land
        {
            practice::Bounds at = body;
            at.x = impact.x - at.w * 0.5f;
            StartSkillVfx(impact.skill, true, at);
            if (impact.skill == invoker::SkillId::ChaosMeteor && m_meteorSheet != NULL)
                m_skillVfx[static_cast<int>(invoker::SkillId::ChaosMeteor)].left = METEOR_BLAST_TIME;  // it already fell
        }
        if (impact.grade == practice::HitGrade::Miss)  // a step missed: only its share is lost (B-7)
        {
            OnBossFail(impact.miss);
            missShown = true;
        }
        else if (impact.grade != practice::HitGrade::None)  // B-14: PERFECT! / GREAT / GOOD
        {
            const SDL_Color cyan = { 110, 210, 255, 255 };
            const SDL_Color white = { 235, 235, 240, 255 };
            bool perfect = impact.grade == practice::HitGrade::Perfect;
            BossText(perfect ? "PERFECT!" : impact.grade == practice::HitGrade::Great ? "GREAT" : "GOOD",
                perfect ? gold : impact.grade == practice::HitGrade::Great ? cyan : white);
            for (int k = 0; k < FEEDBACK_MAX_BURSTS; ++k)
            {
                if (m_bursts[k].left <= 0.0f)
                {
                    m_bursts[k] = { FEEDBACK_BURST_TIME, static_cast<int>(body.x + body.w * 0.5f), static_cast<int>(body.y + body.h * 0.5f) };
                    break;
                }
            }
            audio::Play(audio::Sfx::CastCorrect);
        }
    }
    if (r.fail != practice::ComboFail::None && !missShown)  // WRONG SPELL, a Tornado that missed, the window closing
        OnBossFail(r.fail);
    if (r.comboComplete)
    {
        snprintf(m_bossComboText, sizeof(m_bossComboText), "COMBO -%d%%", r.damage);
        BossText(m_bossComboText, gold, 40);
        m_bossDamageShown = r.damage;
        m_bossDamageLeft = BOSS_DAMAGE_SHOW;
        m_shakeLeft = FEEDBACK_SHAKE_TIME;
        printf("[boss] combo: -%d%%, boss HP %d%%\n", r.damage, m_boss.BossHp());
    }
    if (r.playerHit)
    {
        OnLeak(m_boss.PlayerHp());
        printf("[boss] the boss reached the player: HP %d\n", m_boss.PlayerHp());
    }
    if (r.won)
    {
        audio::Play(audio::Sfx::Start);
        int i = m_boss.BossIndex();
        float time = m_boss.Elapsed();
        if (!m_boss.Assisted() && (m_bossBest[i] <= 0.0f || time < m_bossBest[i]))
        {
            m_bossBest[i] = time;
            m_bossNewBest = true;
            SaveBossTimes();
        }
        printf("[boss] %s defeated in %.1f s%s\n", m_boss.Def().name, time, m_bossNewBest ? " (new best)" : "");
    }
    if (r.lost)
    {
        audio::Play(audio::Sfx::GameOver);
        printf("[boss] defeated by %s\n", m_boss.Def().name);
    }
}

void GameManager::OnBossFail(practice::ComboFail reason)
{
    const SDL_Color red = { 255, 90, 90, 255 };
    const char* text = reason == practice::ComboFail::WrongSpell ? "WRONG SPELL"
        : reason == practice::ComboFail::TooEarly ? "TOO EARLY"
        : reason == practice::ComboFail::TooLate ? "TOO LATE" : "MISSED";
    BossText(text, red);
    m_enemyFlashLeft = FEEDBACK_ENEMY_FLASH;
    audio::Play(audio::Sfx::CastWrong);
    printf("[boss] combo failed: %s\n", text);
}

void GameManager::BossText(const char* text, SDL_Color color, int raise)
{
    practice::Bounds b = BossDrawnBody();
    int x = static_cast<int>(b.x + b.w * 0.5f);
    int half = pixeltext::Width(text, 3) / 2 + 8;
    x = x < half ? half : (x > SCREEN_WIDTH - half ? SCREEN_WIDTH - half : x);
    int y = static_cast<int>(b.y) - 16 - raise;
    y = y < 150 ? 150 : y;  // below the HUD rows
    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
    {
        if (m_floatTexts[i].left <= 0.0f)
        {
            m_floatTexts[i] = { FEEDBACK_TEXT_TIME * 1.6f, x, y, text, color };
            break;
        }
    }
}

// Best fight time per boss, saved like the records: bosses.txt (milliseconds, one per boss) next to the exe /
// in the app folder, localStorage on the web. Missing or unreadable = not beaten yet.
void GameManager::LoadBossTimes()
{
    int raw[practice::BOSS_COUNT] = {};
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var values = String(localStorage.getItem('threeElements_bossTimes')).split(',').map(Number);
            for (var i = 0; i < $1; ++i)
                HEAP32[($0 >> 2) + i] = values[i] | 0;
        } catch (e) {}
    }, raw, practice::BOSS_COUNT);
#else
    FILE* f = fopen(SavePath("bosses.txt").c_str(), "r");
    if (f != NULL)
    {
        for (int i = 0; i < practice::BOSS_COUNT; ++i)
            if (fscanf(f, "%d", &raw[i]) != 1)
                break;
        fclose(f);
    }
#endif
    for (int i = 0; i < practice::BOSS_COUNT; ++i)
        m_bossBest[i] = raw[i] > 0 ? raw[i] / 1000.0f : 0.0f;
}

void GameManager::SaveBossTimes()
{
    int raw[practice::BOSS_COUNT];
    for (int i = 0; i < practice::BOSS_COUNT; ++i)
        raw[i] = static_cast<int>(m_bossBest[i] * 1000.0f);
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var values = [];
            for (var i = 0; i < $1; ++i)
                values.push(HEAP32[($0 >> 2) + i]);
            localStorage.setItem('threeElements_bossTimes', values.join(','));
        } catch (e) {}
    }, raw, practice::BOSS_COUNT);
#else
    FILE* f = fopen(SavePath("bosses.txt").c_str(), "w");
    if (f != NULL)
    {
        for (int i = 0; i < practice::BOSS_COUNT; ++i)
            fprintf(f, "%d ", raw[i]);
        fprintf(f, "\n");
        fclose(f);
    }
#endif
}

SDL_Rect GameManager::BossSelectRect(int index) const
{
    SDL_Rect r = { BOSS_SELECT_PANEL.x + 24, BOSS_SELECT_ROW_Y + index * BOSS_SELECT_ROW_STEP, BOSS_SELECT_PANEL.w - 48, BOSS_SELECT_ROW_H };
    return r;
}

practice::Bounds GameManager::BossDrawnBody() const
{
    practice::Bounds b = m_boss.Body();
    if (m_boss.Phase() == practice::BossPhase::Airborne)
        b.y -= practice::BossLiftHeight(m_boss.AirTime());
    return b;
}

// Filled ellipse (the boss's shadow) and an ellipse outline (impact rings), both flat on the ground.
static void FillEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry)
{
    for (int dy = -ry; dy <= ry; ++dy)
    {
        float k = 1.0f - static_cast<float>(dy * dy) / static_cast<float>(ry * ry);
        int half = static_cast<int>(rx * std::sqrt(k > 0.0f ? k : 0.0f));
        SDL_RenderDrawLine(r, cx - half, cy + dy, cx + half, cy + dy);
    }
}

static void DrawEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry)
{
    SDL_Point pts[41];
    for (int i = 0; i <= 40; ++i)
    {
        float a = 6.2831853f * i / 40.0f;
        pts[i] = { cx + static_cast<int>(rx * std::cos(a)), cy + static_cast<int>(ry * std::sin(a)) };
    }
    SDL_RenderDrawLines(r, pts, 41);
}

// The boss: its shadow on the ground (smaller the higher it floats), the Tornado under it while it is in the air,
// and the enemy sprite drawn larger and tinted. It flashes red when a combo fails.
void GameManager::RenderBoss()
{
    if (!m_bossActive)
        return;
    const practice::BossDefinition& def = m_boss.Def();
    const practice::EnemyDefinition& e = practice::GetEnemyDefinition(def.enemyDefinition);
    EnemyObject& sprite = m_enemySprites[def.enemyDefinition];
    bool airborne = m_boss.Phase() == practice::BossPhase::Airborne;
    float lift = airborne ? practice::BossLiftHeight(m_boss.AirTime()) : 0.0f;
    practice::Bounds body = m_boss.Body();
    const int ground = static_cast<int>(practice::GROUND_LINE_Y);
    int cx = static_cast<int>(body.x + body.w * 0.5f);

    float shrink = 1.0f - 0.5f * lift / practice::BOSS_LIFT_HEIGHT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 110);
    FillEllipse(m_screen, cx, ground, static_cast<int>(body.w * 0.55f * shrink), static_cast<int>(8 * shrink));
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    if (airborne && m_tornadoSheet != NULL)
    {
        int frame = practice::TornadoFrame(m_boss.AirTime());
        SDL_Rect src = { (frame % TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE, (frame / TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE,
            TORNADO_FRAME_SIZE, TORNADO_FRAME_SIZE };
        const int size = 112;
        SDL_Rect dst = { cx - size / 2, ground - static_cast<int>(lift) - size / 2 + 24, size, size };
        SDL_RenderCopy(m_screen, m_tornadoSheet, &src, &dst);
    }

    if (sprite.p_object_ == NULL)
        return;
    int x = static_cast<int>(body.x - e.bodyLeft * def.scale);
    int y = static_cast<int>(practice::GROUND_LINE_Y - e.feetRow * def.scale - lift);
    bool flash = m_enemyFlashLeft > 0.0f;
    if (flash)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 90, 90);
    else
        SDL_SetTextureColorMod(sprite.p_object_, def.tint[0], def.tint[1], def.tint[2]);
    sprite.RenderFrameScaled(m_screen, x, y, def.scale);
    SDL_SetTextureColorMod(sprite.p_object_, 255, 255, 255);
}

// Where each delayed spell will land: a faint circle of its reach and a ring closing in on the impact point.
void GameManager::RenderBossImpacts()
{
    if (!m_bossActive)
        return;
    const int ground = static_cast<int>(practice::GROUND_LINE_Y);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < m_boss.PendingCount(); ++i)
    {
        const practice::PendingSpell& p = m_boss.GetPending(i);
        SDL_Color c = p.skill == invoker::SkillId::SunStrike ? SDL_Color{ 255, 215, 80, 255 }
            : p.skill == invoker::SkillId::ChaosMeteor ? SDL_Color{ 255, 120, 30, 255 } : SDL_Color{ 190, 100, 255, 255 };
        float radius = practice::BossSpellRadius(p.skill);
        float progress = p.delay > 0.0f ? 1.0f - p.left / p.delay : 1.0f;  // 0 at the cast, 1 at the impact
        int cx = static_cast<int>(p.x);
        if (p.skill == invoker::SkillId::ChaosMeteor && m_meteorSheet != NULL && p.left < METEOR_FALL_TIME)
        {
            practice::Bounds body = m_boss.Body();  // the rock is seen falling onto its impact point
            RenderMeteorSprite(METEOR_FALL_TIME - p.left, cx, static_cast<int>(body.y + body.h * 0.5f));
            SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        }
        SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 70);
        DrawEllipse(m_screen, cx, ground, static_cast<int>(radius), static_cast<int>(radius * 0.22f));
        int rx = static_cast<int>(radius * (1.0f - progress)) + 4;
        SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, static_cast<Uint8>(120 + 120 * progress));
        for (int k = 0; k < 2; ++k)
            DrawEllipse(m_screen, cx, ground + k, rx, static_cast<int>(rx * 0.22f) + 1);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
}

// Row 1: HP squares, the boss's name and its HP bar. Row 2: the fight's time. ESC BACK on the right.
void GameManager::RenderBossHud()
{
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    char buf[64];

    pixeltext::DrawShadowed(m_screen, "HP", 16, 12, 2, white);
    for (int i = 0; i < practice::START_HP; ++i)
    {
        SDL_Rect box = { 56 + i * 24, 10, 18, 18 };
        bool blinkOn = i == m_hpBlinkIndex && m_hpBlinkLeft > 0.0f && static_cast<int>(m_hpBlinkLeft * 16.0f) % 2 == 0;
        if (blinkOn)
            SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
        else if (i < m_boss.PlayerHp())
            SDL_SetRenderDrawColor(m_screen, 220, 50, 50, 255);
        else
            SDL_SetRenderDrawColor(m_screen, 40, 20, 24, 255);
        SDL_RenderFillRect(m_screen, &box);
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
        SDL_RenderDrawRect(m_screen, &box);
    }

    const practice::BossDefinition& def = m_boss.Def();
    pixeltext::DrawShadowed(m_screen, def.name, 190, 12, 2, gold);
    // HP in % (B-4): the bar empties by each combo's damage, which shows beside it for a moment
    int barX = 190 + pixeltext::Width(def.name, 2) + 16;
    SDL_Rect bar = { barX, 9, BOSS_HP_BAR_W, 20 };
    SDL_Rect fill = { barX + 1, 10, (BOSS_HP_BAR_W - 2) * m_boss.BossHp() / practice::BOSS_FULL_HP, 18 };
    SDL_SetRenderDrawColor(m_screen, 30, 20, 40, 255);
    SDL_RenderFillRect(m_screen, &bar);
    SDL_SetRenderDrawColor(m_screen, 170, 60, 220, 255);
    SDL_RenderFillRect(m_screen, &fill);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
    SDL_RenderDrawRect(m_screen, &bar);
    snprintf(buf, sizeof(buf), "%d%%", m_boss.BossHp());
    pixeltext::DrawShadowed(m_screen, buf, barX + BOSS_HP_BAR_W + 10, 12, 2, white);
    if (m_bossDamageLeft > 0.0f)
    {
        snprintf(buf, sizeof(buf), "-%d%%", m_bossDamageShown);
        pixeltext::DrawShadowed(m_screen, buf, barX + BOSS_HP_BAR_W + 70, 12, 2, gold);
    }

    char value[16];
    FormatTime(value, sizeof(value), m_boss.Elapsed());
    snprintf(buf, sizeof(buf), "TIME %s", value);
    pixeltext::DrawShadowed(m_screen, buf, 190, 42, 2, white);
    if (m_boss.State() == practice::BossState::Fighting)
        pixeltext::DrawShadowed(m_screen, "ESC  BACK", SCREEN_WIDTH - pixeltext::Width("ESC  BACK", 2) - 16, 42, 2, grey);
}

// The combo at the top right: one icon tile per spell in order (done = green, cast and on its way = blue,
// next = pulsing gold), the next spell's name above, the recipe orbs under each tile with the RECIPE HINT on.
// In the middle: boss 1's lesson line and its CAST NOW cue, or why the last attempt failed.
void GameManager::RenderBossCombo()
{
    if (!m_bossActive || m_boss.State() != practice::BossState::Fighting)
        return;
    const practice::BossDefinition& def = m_boss.Def();
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 190, 190, 200, 255 };
    const SDL_Color red = { 255, 110, 110, 255 };
    char buf[64];

    int n = def.comboLength;
    int width = n * BOSS_COMBO_TILE + (n - 1) * BOSS_COMBO_GAP;
    int x0 = SCREEN_WIDTH - 16 - width;
    bool running = m_boss.AttemptRunning();
    int next = running ? m_boss.CastSteps() : 0;
    int nowStep = -1;  // hint: the follow-up whose timing bar says NOW

    if (running && next >= n)
        snprintf(buf, sizeof(buf), "COMBO CAST - WAIT FOR IT");
    else
        snprintf(buf, sizeof(buf), "NEXT: %s", invoker::GetSkillDefinition(def.combo[next]).name);
    pixeltext::DrawShadowed(m_screen, buf, SCREEN_WIDTH - 16 - pixeltext::Width(buf, 2), 64, 2, gold);

    for (int i = 0; i < n; ++i)
    {
        SDL_Rect tile = { x0 + i * (BOSS_COMBO_TILE + BOSS_COMBO_GAP), BOSS_COMBO_Y, BOSS_COMBO_TILE, BOSS_COMBO_TILE };
        bool done = running && m_boss.StepDone(i);
        bool cast = running && i < next;
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 190);
        SDL_RenderFillRect(m_screen, &tile);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        m_skillIcons[static_cast<int>(def.combo[i])].RenderAt(m_screen, tile.x + 2, tile.y + 2, BOSS_COMBO_TILE - 4);
        if (done)
            SDL_SetRenderDrawColor(m_screen, 90, 230, 110, 255);
        else if (cast)
            SDL_SetRenderDrawColor(m_screen, 90, 190, 255, 255);
        else
            SDL_SetRenderDrawColor(m_screen, 120, 125, 140, 255);
        for (int k = 0; k < (done || cast ? 3 : 1); ++k)
        {
            SDL_Rect frame = { tile.x - k, tile.y - k, tile.w + 2 * k, tile.h + 2 * k };
            SDL_RenderDrawRect(m_screen, &frame);
        }
        if (i == next && !(running && next >= n))
            RenderHighlight(tile);
        if (i + 1 < n)
            pixeltext::DrawShadowed(m_screen, ">", tile.x + BOSS_COMBO_TILE + BOSS_COMBO_GAP / 2 - 5, tile.y + 13, 2, grey);
        // B-15, hint only: the timing bar shrinks to empty at the ideal moment to cast this spell
        float until = 0.0f, span = 0.0f;
        if (m_recipeHint && m_boss.StepTiming(i, until, span))
        {
            SDL_Rect back = { tile.x, BOSS_COMBO_Y + BOSS_COMBO_TILE + 20, BOSS_COMBO_TILE, 4 };
            SDL_SetRenderDrawColor(m_screen, 30, 32, 44, 255);
            SDL_RenderFillRect(m_screen, &back);
            bool now = until <= BOSS_BAR_NOW_EARLY && until >= -BOSS_BAR_NOW_LATE;
            if (now)
            {
                SDL_SetRenderDrawColor(m_screen, 90, 240, 110, 255);
                SDL_RenderFillRect(m_screen, &back);
                RenderHighlight(tile);
                if (nowStep < 0)
                    nowStep = i;
            }
            else if (until > 0.0f)
            {
                SDL_Rect bar = back;
                float left = until / span;
                bar.w = static_cast<int>(BOSS_COMBO_TILE * (left > 1.0f ? 1.0f : left));
                SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 255);
                SDL_RenderFillRect(m_screen, &bar);
            }
            else  // past it: late now, the bar stays red until the spell is cast
            {
                SDL_SetRenderDrawColor(m_screen, 220, 60, 60, 255);
                SDL_RenderDrawRect(m_screen, &back);
            }
        }
        if (m_recipeHint)
        {
            const invoker::Recipe& r = invoker::GetSkillDefinition(def.combo[i]).recipe;
            const int counts[3] = { r.quas, r.wex, r.exort };
            int ox = tile.x + BOSS_COMBO_TILE / 2 - 15;  // three 14 px orbs, 15 px apart, centred under the tile
            for (int element = 0; element < 3; ++element)
            {
                for (int c = 0; c < counts[element]; ++c, ox += 15)
                {
                    int oy = BOSS_COMBO_Y + BOSS_COMBO_TILE + 11;
                    SDL_Color col = kOrbColors[element];
                    SDL_SetRenderDrawColor(m_screen, 10, 12, 18, 255);
                    draw::FillCircle(m_screen, ox, oy, 8);
                    SDL_SetRenderDrawColor(m_screen, col.r, col.g, col.b, 255);
                    draw::FillCircle(m_screen, ox, oy, 7);
                    const char* letter = element == 0 ? "Q" : element == 1 ? "W" : "E";
                    const SDL_Color white = { 255, 255, 255, 255 };
                    pixeltext::Draw(m_screen, letter, ox - 2, oy - 3, 1, white);
                }
            }
        }
    }

    if (def.guided)
        pixeltext::DrawCentered(m_screen, "TORNADO LIFTS IT. LAND SUN STRIKE AS IT COMES DOWN.", SCREEN_WIDTH, 100, 1, grey);
    if (nowStep > 0)
    {
        float t = SDL_GetTicks() / 1000.0f;
        SDL_Color pulse = { 90, static_cast<Uint8>(200 + 55 * (0.5f + 0.5f * std::sin(t * 12.0f))), 110, 255 };
        snprintf(buf, sizeof(buf), "CAST %s NOW!", invoker::GetSkillDefinition(def.combo[nowStep]).name);
        int scale = pixeltext::Width(buf, 3) <= 400 ? 3 : 2;  // long names: keep clear of the combo strip
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 116, scale, pulse);
    }
    else if (m_boss.CueNow())
    {
        float t = SDL_GetTicks() / 1000.0f;
        SDL_Color pulse = { 255, static_cast<Uint8>(200 + 55 * (0.5f + 0.5f * std::sin(t * 12.0f))), 80, 255 };
        pixeltext::DrawCentered(m_screen, "CAST NOW!", SCREEN_WIDTH, 116, 3, pulse);
    }
    else if (!running && m_boss.LastFail() != practice::ComboFail::None)
    {
        practice::ComboFail f = m_boss.LastFail();
        snprintf(buf, sizeof(buf), "LAST TRY: %s", f == practice::ComboFail::WrongSpell ? "WRONG SPELL"
            : f == practice::ComboFail::TooEarly ? "TOO EARLY" : f == practice::ComboFail::TooLate ? "TOO LATE" : "MISSED");
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 118, 2, red);
    }
}

// The three bosses, each with its combo and best time; arrows + Enter, 1-3, or a tap.
void GameManager::RenderBossSelect()
{
    DimScreen(190);
    const SDL_Rect& panel = BOSS_SELECT_PANEL;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 235);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 150, 155, 170, 255 };
    pixeltext::DrawCentered(m_screen, "BOSS FIGHTS", SCREEN_WIDTH, panel.y + 16, 3, gold);
    pixeltext::DrawCentered(m_screen, "ONLY A FULL COMBO HURTS A BOSS", SCREEN_WIDTH, panel.y + 48, 1, grey);

    char buf[96], time[16];
    for (int i = 0; i < practice::BOSS_COUNT; ++i)
    {
        const practice::BossDefinition& def = practice::GetBossDefinition(i);
        SDL_Rect r = BossSelectRect(i);
        bool selected = i == m_bossSelect;
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 26, 28, 40, selected ? 240 : 180);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, selected ? 255 : 110, selected ? 210 : 115, selected ? 90 : 130, 220);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

        snprintf(buf, sizeof(buf), "%d  %s", i + 1, def.name);
        pixeltext::DrawShadowed(m_screen, buf, r.x + 16, r.y + 12, 3, selected ? gold : white);
        buf[0] = '\0';
        for (int k = 0; k < def.comboLength; ++k)
        {
            if (k > 0)
                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " > ");
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", invoker::GetSkillDefinition(def.combo[k]).name);
        }
        pixeltext::DrawShadowed(m_screen, buf, r.x + 16, r.y + 50, 2, white);
        if (m_bossBest[i] > 0.0f)
        {
            FormatTime(time, sizeof(time), m_bossBest[i]);
            snprintf(buf, sizeof(buf), "BEST %s", time);
        }
        else
            snprintf(buf, sizeof(buf), "NOT BEATEN YET");
        pixeltext::DrawShadowed(m_screen, buf, r.x + 16, r.y + 80, 2, m_bossBest[i] > 0.0f ? gold : grey);
        if (selected)
            RenderHighlight(r);
    }
    const char* help = m_showTouchControls ? "TAP A BOSS TO FIGHT   TAP OUTSIDE TO GO BACK" : "1-3 / ENTER: FIGHT     ESC: BACK";
    pixeltext::DrawCentered(m_screen, help, SCREEN_WIDTH, panel.y + panel.h - 28, 2, grey);
}

// Victory or defeat: the time, the best time, and how to go on (the same tap zones as Practice's Game Over).
void GameManager::RenderBossResult()
{
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color red = { 235, 70, 70, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    char buf[64], value[16];
    bool won = m_boss.State() == practice::BossState::Won;
    int i = m_boss.BossIndex();

    DimScreen(170);
    pixeltext::DrawCentered(m_screen, won ? "BOSS DEFEATED!" : "DEFEATED", SCREEN_WIDTH, 70, 6, won ? gold : red);
    pixeltext::DrawCentered(m_screen, m_boss.Def().name, SCREEN_WIDTH, 150, 3, white);
    if (won)
    {
        FormatTime(value, sizeof(value), m_boss.Elapsed());
        snprintf(buf, sizeof(buf), m_bossNewBest ? "TIME %s  NEW BEST!" : "TIME %s", value);
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 200, 3, m_bossNewBest ? gold : white);
    }
    else
    {
        snprintf(buf, sizeof(buf), "BOSS HP LEFT %d%%", m_boss.BossHp());
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 200, 3, white);
    }
    if (m_bossBest[i] > 0.0f)
    {
        FormatTime(value, sizeof(value), m_bossBest[i]);
        snprintf(buf, sizeof(buf), "BEST TIME %s", value);
        pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 250, 2, grey);
    }
    if (m_boss.Assisted())
        pixeltext::DrawCentered(m_screen, "RECIPE HINT WAS ON - NO BEST TIME", SCREEN_WIDTH, 280, 2, grey);

    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO FIGHT AGAIN", SCREEN_WIDTH, 350, 3, white);
    pixeltext::DrawCentered(m_screen, "ESC  BOSS LIST", SCREEN_WIDTH, 395, 2, grey);
}

bool GameManager::loadBackgroundLayers() {
    const char* layerPaths[12] = {
       "assets/background/Layer_0011_0.png",
       "assets/background/Layer_0010_1.png",
        "assets/background/Layer_0009_2.png",
         "assets/background/Layer_0008_3.png",
         "assets/background/Layer_0007_Lights.png",
         "assets/background/Layer_0006_4.png",
         "assets/background/Layer_0005_5.png",
         "assets/background/Layer_0004_Lights.png",
        "assets/background/Layer_0003_6.png",
         "assets/background/Layer_0002_7.png",
         "assets/background/Layer_0001_8.png",
          "assets/background/Layer_0000_9.png"         
    };

    for (int i = 0; i < 12; ++i) {
        SDL_Surface* tempSurface = IMG_Load(layerPaths[i]);
        if (!tempSurface) {
            printf("Failed to load background layer %d: %s\n", i + 1, IMG_GetError());
            return false;
        }
        backgroundLayers[i] = SDL_CreateTextureFromSurface(m_screen, tempSurface);
        SDL_FreeSurface(tempSurface);
        if (!backgroundLayers[i]) {
            printf("Failed to create texture for background layer %d: %s\n", i + 1, SDL_GetError());
            return false;
        }
        backgroundPositions[i] = 0.0f;
        backgroundSpeeds[i] = BACKGROUND_LAYER_SPEED * (i + 1); // px/s, deeper layers slower
    }
    return true;
}

void GameManager::renderBackgroundLayers() {
    // PLAY: every boss defeated is a new stage with its own light (a placeholder for the stage backgrounds, P3-6)
    static const SDL_Color kStageTints[] = {
        { 255, 255, 255, 255 }, { 255, 205, 160, 255 }, { 165, 185, 255, 255 }, { 255, 150, 150, 255 }, { 205, 165, 255, 255 } };
    SDL_Color tint = kStageTints[0];
    if (!m_tutorialActive && !m_bossActive && m_session.Mode() == practice::SessionMode::Play
        && m_session.State() != practice::GameState::Ready)
        tint = kStageTints[m_session.GetStats().bossesDefeated % 5];
    for (int i = 0; i < 12; ++i) {
        SDL_SetTextureColorMod(backgroundLayers[i], tint.r, tint.g, tint.b);
        SDL_Rect srcRect = { 0, BACKGROUND_CROP_Y, SCREEN_WIDTH, SCREEN_HEIGHT };  // unscaled band, see GameManager.h
        SDL_Rect destRect = { static_cast<int>(backgroundPositions[i]), 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderCopy(m_screen, backgroundLayers[i], &srcRect, &destRect);

        // Render a second copy of the layer to create a seamless loop
        if (backgroundPositions[i] <= 0) {
            destRect.x = static_cast<int>(backgroundPositions[i]) + SCREEN_WIDTH;
            SDL_RenderCopy(m_screen, backgroundLayers[i], &srcRect, &destRect);
        }
        else {
            destRect.x = static_cast<int>(backgroundPositions[i]) - SCREEN_WIDTH;
            SDL_RenderCopy(m_screen, backgroundLayers[i], &srcRect, &destRect);
        }
    }
}

void GameManager::updateBackgroundLayers(float dt) {
    for (int i = 0; i < 12; ++i) {
        backgroundPositions[i] -= backgroundSpeeds[i] * dt;
        if (backgroundPositions[i] <= -SCREEN_WIDTH) {
            backgroundPositions[i] += SCREEN_WIDTH;
        }
    }
}
void GameManager::Close()
{
    if (m_tornadoSheet != NULL)
    {
        SDL_DestroyTexture(m_tornadoSheet);
        m_tornadoSheet = NULL;
    }
    if (m_ghostWalkSheet != NULL)
    {
        SDL_DestroyTexture(m_ghostWalkSheet);
        m_ghostWalkSheet = NULL;
    }
    SDL_Texture** sheets[3] = { &m_playerRunSheet, &m_meteorSheet, &m_forgeSheet };
    for (SDL_Texture** sheet : sheets)
    {
        if (*sheet != NULL)
        {
            SDL_DestroyTexture(*sheet);
            *sheet = NULL;
        }
    }
    audio::Shutdown();
    for (int i = 0; i < 12; ++i)
    {
        if (backgroundLayers[i] != NULL)
        {
            SDL_DestroyTexture(backgroundLayers[i]);
            backgroundLayers[i] = NULL;
        }
    }
    // sprites owned by BaseObject-derived members are freed by their destructors (BaseObject::free)

    SDL_DestroyRenderer(m_screen);
    m_screen = NULL;

    SDL_DestroyWindow(m_window);
    m_window = NULL;

    IMG_Quit();
    SDL_Quit();
}