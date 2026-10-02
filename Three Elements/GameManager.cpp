
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
#include "Online.h"
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
static const char* RuneName(practice::Rune rune);              // "FROST", defined with the item icons below
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
    m_playerCastSheet = LoadSheet(PLAYER_CAST_SHEET_PATH, PLAYER_RUN_COLUMNS * PLAYER_RUN_FRAME);
    m_meteorSheet = LoadSheet(METEOR_SHEET_PATH, METEOR_COLUMNS * METEOR_FRAME);
    m_iceWallSheet = LoadSheet(ICE_WALL_SHEET_PATH, ICE_WALL_COLUMNS * ICE_WALL_FRAME);
    for (int i = 0; i < IMMORTAL_ART_COUNT; ++i)  // missing art: that Immortal is drawn as a tinted enemy sprite
    {
        m_immortalRun[i] = LoadTexture(IMMORTAL_RUN_SHEETS[i]);
        m_immortalHit[i] = LoadTexture(IMMORTAL_HIT_SHEETS[i]);
    }
    m_forgeSheet = LoadSheet(FORGE_SHEET_PATH, FORGE_COLUMNS * FORGE_FRAME);
    LoadTopRuns();
    LoadBests();
    LoadBossTimes();
    LoadPlayRuns();
    LoadGold();
    LoadInventory();
    LoadItemIcons();
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
    if (m_forceTouch)
        m_showTouchControls = true;

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
            if (CanPause())
                m_paused = true;  // a call, the home button: the run waits for the player
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
        else if (m_showLayout && m_event.type == SDL_FINGERMOTION)  // the layout editor drags a block
        {
            int gameX = 0, gameY = 0;
            TouchToGame(m_event.tfinger.x, m_event.tfinger.y, gameX, gameY);
            LayoutPointerMove(gameX, gameY);
        }
        else if (m_showLayout && m_event.type == SDL_MOUSEMOTION && m_event.motion.which != SDL_TOUCH_MOUSEID)
        {
            LayoutPointerMove(m_event.motion.x, m_event.motion.y);
        }
        else if (m_showLayout && (m_event.type == SDL_FINGERUP
            || (m_event.type == SDL_MOUSEBUTTONUP && m_event.button.which != SDL_TOUCH_MOUSEID)))
        {
            LayoutPointerUp();
        }
    }

    // Practice rules: enemy movement, spawning, leaks, Game Over (input of this frame is already applied).
    // The enemy's position is read first: a Tornado hit removes it inside Update(), and the "+1" goes where it was.
    practice::Bounds enemyBefore = m_session.Enemy().active ? practice::EnemyBounds(m_session.Enemy())
                                                            : practice::Bounds{ 0.0f, 0.0f, 0.0f, 0.0f };
    practice::ActiveEnemy enemyShown = m_session.Enemy();
    if (ImmortalFight())
        enemyBefore = BossDrawnBody();  // the points of a beaten IMMORTAL rise from where it stood
    if (m_paused && !CanPause())
        m_paused = false;
    bool frozen = m_paused || TipShown();  // a first-time tip is being read: the run waits, like a pause
    if (frozen)
        dt = 0.0f;  // nothing moves or animates; the rules below are not even called
    practice::UpdateResult update = {};
    if (!frozen)
        update = m_session.Update(dt);
    LogUpdate(update);
    if (update.spawned && m_session.Mode() == practice::SessionMode::Play)  // spec §31: the first elite / boss
    {
        if (m_session.Enemy().kind == practice::EnemyKind::Elite)
            QueueTip(TIP_ELITE);
        else if (m_session.Enemy().kind == practice::EnemyKind::Overlord)
            QueueTip(TIP_OVERLORD);
    }
    if (update.immortalWarning)
        QueueTip(TIP_IMMORTAL);
    if (update.immortalWarning)  // spec §28 O-3: the banner (RenderImmortalWarning) and a horn call per tier
    {
        audio::Play(update.immortalTier >= 3 ? audio::Sfx::Immortal3
            : update.immortalTier == 2 ? audio::Sfx::Immortal2 : audio::Sfx::Immortal1);
        printf("[play] IMMORTAL incoming: %s (tier %d)\n", practice::GetBossDefinition(update.immortalBoss).name, update.immortalTier);
    }
    if (update.immortalFight)
        audio::Play(audio::Sfx::Start);
    if (update.immortalUpdated)
        PresentBossUpdate(update.boss, true);
    if (update.kill.immortal)  // above "GOOD" and "COMBO -n%", which are rising there already
    {
        const SDL_Color points = { 120, 235, 130, 255 };  // the same green as every other score text
        BossText(PointsText(update.kill.points), points, 72);
    }
    if (update.cast != practice::CastOutcome::None)
        OnCastJudged(update.cast, enemyBefore, update.cast != practice::CastOutcome::Correct ? NULL
            : update.kill.killed ? PointsText(update.kill.points) : "HIT");
    if (update.kill.killed)
        OnKill(update.kill, enemyBefore);
    if (update.kill.killed && !update.kill.immortal && enemyShown.active)  // a Tornado's kill: it fades out too
        BeginBeatenFade(enemyShown, 0.0f);
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
    if (m_bossActive && !frozen)
        PresentBossUpdate(m_boss.Update(dt));
    m_bossDamageLeft -= dt;
    m_bossAnimTime += dt;
    m_bossHitLeft -= dt;
    m_meterFlashLeft -= dt;
    m_announceLeft -= dt;  // the fight's clock stops by itself once it is won or lost
    m_tutorialWrongFlash -= dt;
    m_orbFlash -= dt;
    if (m_castAnim >= 0.0f)
    {
        m_castAnim += dt;
        if (m_castAnim * PLAYER_CAST_FPS >= PLAYER_CAST_FRAMES - PLAYER_CAST_START)
            m_castAnim = -1.0f;  // back to running
    }
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
    RenderDrops();      // the material / rune a boss left behind
    SDL_RenderSetViewport(m_screen, &sceneViewport);  // end of the (possibly shaken) scene: back to the letterbox

    RenderLeakFlash();
    RenderTouchControls();  // on top of the scene, only while Playing
    RenderInvokerHud();
    RenderStatsHud();
    RenderTargetHint();
    RenderBossCombo();
    RenderImmortalWarning();
    RenderImmortalBar();
    RenderImmortalMeter();
    RenderItemBar();

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
    if (m_showSettings)
        RenderSettings();
    if (m_showLayout)
        RenderLayoutEditor();
    if (m_showShop)
        RenderShop();
    if (m_showGuide)
        RenderGuide();
    RenderRuneChoice();
    RenderPause();
    RenderTip();
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
    if (TipShown())  // only these close a tip, so a player hammering Q / W / E does not skip it unread
    {
        if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE || sym == SDLK_ESCAPE || sym == SDLK_AC_BACK)
            DismissTip();
        return;
    }
    if (m_showGuide)
    {
        HandleGuideKey(sym);
        return;
    }
    if (m_showRecipes || m_showLeaderboard)  // an overlay is open: any key just closes it
    {
        m_showRecipes = m_showLeaderboard = false;
        return;
    }
    if (m_showLayout)  // the layout editor: any of these closes it (the layout is saved as it is)
    {
        if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK || sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
            CloseLayoutEditor();
        return;
    }
    if (m_showSettings)
    {
        HandleSettingsKey(sym);
        return;
    }
    if (m_showShop)
    {
        HandleShopKey(sym);
        return;
    }
    if (m_paused)
    {
        HandlePauseKey(sym, quit);
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
    if (m_session.State() == practice::GameState::Playing && m_session.Mode() == practice::SessionMode::Play)
    {
        if (m_session.RuneChoiceCount() > 0)  // Aghanim: 1 / 2 / 3 pick the rune; the run waits
        {
            if (sym >= SDLK_1 && sym <= SDLK_3)
            {
                ChooseRuneAction(static_cast<int>(sym - SDLK_1));
                return;
            }
            if (sym != SDLK_ESCAPE && sym != SDLK_AC_BACK && sym != SDLK_m && sym != SDLK_g)
                return;
        }
        // the items, 2 rows of 3 under the right hand (spec §27 I-3)
        const SDL_Keycode itemKeys[2][practice::ITEM_SLOTS] = {
            { SDLK_u, SDLK_i, SDLK_o, SDLK_j, SDLK_k, SDLK_l },
            { SDLK_KP_7, SDLK_KP_8, SDLK_KP_9, SDLK_KP_4, SDLK_KP_5, SDLK_KP_6 } };
        for (int row = 0; row < 2; ++row)
            for (int slot = 0; slot < practice::ITEM_SLOTS; ++slot)
                if (sym == itemKeys[row][slot])
                {
                    UseItemSlot(slot);
                    return;
                }
    }
    if (menu && sym == SDLK_p) { m_showShop = true; return; }
    if (menu && (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_LEFT || sym == SDLK_RIGHT))  // through the menu, wrapping
    {
        int n = MenuItemCount();
        m_menuIndex = (m_menuIndex + (sym == SDLK_UP || sym == SDLK_LEFT ? n - 1 : 1)) % n;
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
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK)  // AC_BACK: Android Back
    {
        if (m_session.State() == practice::GameState::Playing)
            m_paused = true;  // owner 2026-10-01: Esc in the middle of a run pauses; the pause offers the menu
        else
            PressEscapeAction(quit);
        return;
    }
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
    m_session.SetLoadout(m_inventory);  // PLAY items (spec §27); ignored by Survival
    if (m_session.PressEnter(seed))
    {
        ResetVisualEffects();
        m_lastBestUpdate = { false, false, false };
        m_paused = false;
        for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
            m_runMaterials[m] = 0;
        printf("[practice] session started (seed %u)\n", seed);
        if (m_recipeHint)
            m_session.MarkAssisted();  // started with the recipe hint (information only, spec §17)
        m_missStreak = 0;
        m_tipQueued = 0;
        if (m_session.Mode() == practice::SessionMode::Play)
        {
            for (int s = 0; s < practice::ITEM_SLOTS; ++s)
                if (m_inventory.slot[s] != practice::ITEM_NONE)
                {
                    QueueTip(TIP_ITEMS);  // the first run with something in a slot
                    break;
                }
        }
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
    if (TipShown())  // only its button closes a tip
    {
        if (hit(TIP_BUTTON_RECT))
            DismissTip();
        return;
    }
    if (m_showGuide)
    {
        HandleGuidePointer(x, y);
        return;
    }
    if (m_showLeaderboard && online::Available() && hit(GLOBAL_BOARDS_BUTTON_RECT))
    {
        OpenGlobalBoards();  // the list stays open underneath
        return;
    }
    if (m_showRecipes || m_showLeaderboard)  // an overlay is open: a tap anywhere closes it
    {
        m_showRecipes = m_showLeaderboard = false;
        return;
    }
    if (m_showLayout)
    {
        LayoutPointerDown(x, y);
        return;
    }
    if (m_showSettings)
    {
        HandleSettingsPointer(x, y);
        return;
    }
    if (m_showShop)
    {
        HandleShopPointer(x, y);
        return;
    }
    if (m_paused)
    {
        HandlePausePointer(x, y, quit);
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
        if (hit(TOUCH_PLAYING_MENU_RECT))  // "PAUSE"
        {
            m_paused = true;
            return;
        }
        if (m_session.Mode() == practice::SessionMode::Play)
        {
            for (int i = 0; i < m_session.RuneChoiceCount(); ++i)  // Aghanim's choice
            {
                SDL_Rect row = { RUNE_CHOICE_RECT.x + 20, RUNE_CHOICE_RECT.y + 60 + i * 56, RUNE_CHOICE_RECT.w - 40, 46 };
                if (hit(row))
                {
                    ChooseRuneAction(i);
                    return;
                }
            }
            if (m_session.RuneChoiceCount() > 0)
                return;
            for (int slot = 0; slot < practice::ITEM_SLOTS; ++slot)  // the item bar
            {
                if (hit(LoadoutSlotRect(slot, ItemBarX(), ItemBarY())))
                {
                    UseItemSlot(slot);
                    return;
                }
            }
        }
        for (int i = 0; i < 6 && m_showTouchControls; ++i)  // hidden buttons are not clickable either
        {
            if (hit(TouchButtonRect(i)))
            {
                ProcessAction(kTouchActions[i]);
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
    practice::ActiveEnemy enemyBefore = m_session.Enemy();
    practice::Bounds enemyBody = hadEnemy ? practice::EnemyBounds(m_session.Enemy()) : practice::Bounds{ 0.0f, 0.0f, 0.0f, 0.0f };
    if (ImmortalFight())
        enemyBody = BossDrawnBody();  // effects aimed at the IMMORTAL go where it is drawn

    practice::InputResult r = m_session.Input(action);
    if (!r.accepted)  // Ready / Game Over: the gameplay keys do nothing
        return;
    if (r.immortal)  // spec §28: the key went to the fight, which answers like a Boss Fights fight
    {
        PresentInvokerResult(action, r.invoker, practice::CastOutcome::None, true, enemyBody);
        PresentBossInput(r.boss);
        return;
    }

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
    {
        OnKill(r.kill, enemyBody);
        BeginBeatenFade(enemyBefore, KillImpactDelay(r.invoker.skill));  // the Forge Spirit / the meteor is on its way
    }

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

    if (result.event == invoker::InvokerEvent::Cast)
        m_castAnim = 0.0f;  // the Injoker casts (a new cast restarts the animation)
    if (result.event == invoker::InvokerEvent::Cast && result.skill == invoker::SkillId::GhostWalk)
        m_ghostWalkLeft = GHOST_WALK_DURATION;  // visual only; the cast is judged like any other spell
    // no-op for Tornado / Ghost Walk (own effects); in a boss fight Sun Strike, Chaos Meteor and EMP are drawn when
    // they land (PresentBossUpdate), not when they are cast
    if (result.event == invoker::InvokerEvent::Cast
        && !((m_bossActive || ImmortalFight()) && practice::BossSpellDelay(result.skill) > 0.0f))
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
    DrawTouchButtons();

    // tutorial: the button of the key to press now
    char expected = m_tutorialActive ? m_tutorial.ExpectedKey() : 0;
    const char letters[6] = { 'Q', 'W', 'E', 'R', 'D', 'F' };
    for (int i = 0; i < 6 && expected != 0; ++i)
        if (letters[i] == expected)
            RenderHighlight(TouchButtonRect(i));
}

void GameManager::DrawTouchButtons()
{
    Keyboard* icons[6] = { &m_keyQ, &m_keyW, &m_keyE, &m_keyR, &m_keyD, &m_keyF };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 6; ++i)
    {
        const SDL_Rect r = TouchButtonRect(i);
        if (i >= 4)  // D / F: the button is the slot itself - the skill's icon, tapped to cast it (spec §30 L-8)
        {
            int slot = i - 4;
            invoker::SkillId spell = ShownInvoker().GetSlot(slot == 0 ? invoker::Slot::D : invoker::Slot::F);
            if (spell != m_slotShown[slot])
            {
                if (spell != invoker::SkillId::None)
                    m_slotFlash[slot] = SLOT_FLASH_TIME;  // a new skill went in: glow, so a glance is enough
                m_slotShown[slot] = spell;
            }
            SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 190);
            SDL_RenderFillRect(m_screen, &r);
            if (spell != invoker::SkillId::None)
            {
                m_skillIcons[static_cast<int>(spell)].RenderAt(m_screen, r.x + 2, r.y + 2, r.w - 4);
                SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
            }
            SDL_SetRenderDrawColor(m_screen, 255, 255, 255, spell != invoker::SkillId::None ? 150 : 70);
            SDL_RenderDrawRect(m_screen, &r);
            // the key's letter in the corner (large and dim in an empty slot)
            const char* letter = slot == 0 ? "D" : "F";
            const SDL_Color white = { 255, 255, 255, 255 };
            const SDL_Color dim = { 120, 125, 140, 255 };
            if (spell != invoker::SkillId::None)
            {
                SDL_Rect tag = { r.x + 1, r.y + 1, 22, 22 };
                SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 200);
                SDL_RenderFillRect(m_screen, &tag);
                pixeltext::Draw(m_screen, letter, r.x + 7, r.y + 5, 2, white);
            }
            else
                pixeltext::Draw(m_screen, letter, r.x + (r.w - pixeltext::Width(letter, 4)) / 2, r.y + (r.h - 28) / 2, 4, dim);
            if (m_slotFlash[slot] > 0.0f)
            {
                SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(m_screen, 255, 215, 90, static_cast<Uint8>(255 * m_slotFlash[slot] / SLOT_FLASH_TIME));
                for (int k = 0; k < 4; ++k)
                {
                    SDL_Rect glow = { r.x - k, r.y - k, r.w + 2 * k, r.h + 2 * k };
                    SDL_RenderDrawRect(m_screen, &glow);
                }
            }
            SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
            continue;
        }
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
            const int pad = r.w / 9;
            SDL_Rect dst = { r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2 };
            SDL_RenderCopy(m_screen, tex, &src, &dst);
        }
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
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
        // the global boards (Android with Play Games, spec §29): PLAY by score, SURVIVAL by time in milliseconds
        if (m_session.Mode() == practice::SessionMode::Play)
            online::Submit(online::Board::Play, st.score);
        else
            online::Submit(online::Board::Survival, static_cast<long long>(st.survivalTime * 1000.0f));
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

static void FillEllipse(SDL_Renderer* r, int cx, int cy, int rx, int ry);  // defined with the boss drawing

// The single active enemy, drawn where the Practice session says it is.
void GameManager::RenderEnemy()
{
    // the enemy just beaten: still there until the spell that beat it arrives (see FORGE_HIT_TIME), then it fades
    // out drifting up
    if (!m_tutorialActive && !m_bossActive && m_session.State() == practice::GameState::Playing)
    {
        if (m_beatenLeft > 0.0f)
            DrawEnemy(m_beatenEnemy, false, BEATEN_ENEMY_ALPHA);
        else if (m_beatenFade > 0.0f)
        {
            float f = m_beatenFade / BEATEN_FADE_TIME;
            DrawEnemy(m_beatenEnemy, false, static_cast<Uint8>(m_beatenAlpha * f), static_cast<int>(BEATEN_FADE_RISE * (1.0f - f)));
        }
    }
    const practice::ActiveEnemy& e = ShownEnemy();
    if (e.active)
        DrawEnemy(e, m_enemyFlashLeft > 0.0f, 255);  // red tint after a wrong cast
}

void GameManager::BeginBeatenFade(const practice::ActiveEnemy& enemy, float wait)
{
    m_beatenEnemy = enemy;
    m_beatenLeft = wait;
    m_beatenFade = BEATEN_FADE_TIME;
    m_beatenAlpha = wait > 0.0f ? BEATEN_ENEMY_ALPHA : 255;
}

float GameManager::KillImpactDelay(invoker::SkillId skill) const
{
    if (skill == invoker::SkillId::ForgeSpirit && m_forgeSheet != NULL)
        return FORGE_HIT_TIME;
    if (skill == invoker::SkillId::ChaosMeteor && m_meteorSheet != NULL)
        return METEOR_FALL_TIME;
    return 0.0f;
}

void GameManager::DrawEnemy(const practice::ActiveEnemy& e, bool flash, Uint8 alpha, int raise)
{
    const practice::EnemyDefinition& def = practice::GetEnemyDefinition(e.definition);
    EnemyObject& sprite = m_enemySprites[e.definition];
    // e.x is the left edge of the visible body; the frame starts bodyLeft pixels earlier
    sprite.SetPos(static_cast<int>(e.x) - def.bodyLeft, static_cast<int>(practice::GROUND_LINE_Y) - def.feetRow - raise);
    if (sprite.p_object_ == NULL)
        return;
    // its shadow on the ground (it stays there under a flying enemy, and under one that fades away rising)
    practice::Bounds body = practice::EnemyBounds(e);
    int shadowRx = static_cast<int>(body.w * ENEMY_SHADOW_WIDTH);
    shadowRx = shadowRx > ENEMY_SHADOW_MAX_RX ? ENEMY_SHADOW_MAX_RX : shadowRx;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, static_cast<Uint8>(SHADOW_ALPHA * alpha / 255));
    FillEllipse(m_screen, static_cast<int>(body.x + body.w * 0.5f), static_cast<int>(practice::GROUND_LINE_Y) + 2,
        shadowRx, shadowRx / 5 > 3 ? shadowRx / 5 : 3);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    SDL_SetTextureAlphaMod(sprite.p_object_, alpha);
    if (flash)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 90, 90);
    else if (e.kind == practice::EnemyKind::Elite)  // PLAY: elites and bosses are larger and tinted (P3-2)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 200, 130);
    else if (e.kind == practice::EnemyKind::Overlord)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 130, 130);
    if (e.scale > 1.0f)
        sprite.RenderFrameScaled(m_screen, static_cast<int>(e.x - def.bodyLeft * e.scale),
            static_cast<int>(practice::GROUND_LINE_Y - def.feetRow * e.scale) - raise, e.scale);
    else
        sprite.Render(m_screen);
    SDL_SetTextureColorMod(sprite.p_object_, 255, 255, 255);
    SDL_SetTextureAlphaMod(sprite.p_object_, 255);
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
    const practice::BossSession& boss = BossView();
    bool fight = m_bossActive || ImmortalFight();
    for (int i = 0; fight && i < boss.ProjectileCount(); ++i)  // a Deafening Blast is drawn by SkillVfx
        if (boss.GetProjectile(i).skill == invoker::SkillId::Tornado)
            draw(boss.GetProjectile(i).motion);
}

// One shared reset for every temporary visual effect: called on Esc (back to Ready) and on Enter (new session),
// exactly where m_ghostWalkLeft used to be cleared by itself.
void GameManager::ResetVisualEffects()
{
    m_ghostWalkLeft = 0.0f;
    m_castAnim = -1.0f;
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
        m_skillVfx[i].left = 0.0f;
    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
        m_floatTexts[i].left = 0.0f;
    for (int i = 0; i < FEEDBACK_MAX_BURSTS; ++i)
        m_bursts[i].left = 0.0f;
    for (int i = 0; i < DROP_MAX; ++i)
        m_drops[i].active = false;
    m_enemyFlashLeft = m_leakFlashLeft = m_shakeLeft = m_hpBlinkLeft = 0.0f;
    m_beatenLeft = m_beatenFade = 0.0f;
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
    m_missStreak = correct ? 0 : m_missStreak + 1;
    if (m_missStreak >= TIP_MISS_STREAK && !m_recipeHint)
        QueueTip(TIP_HINT);  // spec §31: several wrong casts in a row - the recipe hint exists
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

    // points are green: gold is kept for the "+5 GOLD" text, so "+1" is not read as one gold (owner 2026-10-01)
    const SDL_Color points = { 120, 235, 130, 255 };
    const SDL_Color red = { 255, 90, 90, 255 };
    for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
    {
        if (m_floatTexts[i].left <= 0.0f)
        {
            m_floatTexts[i] = { FEEDBACK_TEXT_TIME, textX, static_cast<int>(enemy.y) - 10,
                label != NULL ? label : correct ? "+1" : "MISS", correct ? points : red };
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
    for (int i = 0; i < practice::ITEM_SLOTS; ++i)
        m_itemFlash[i] -= dt;
    m_slotFlash[0] -= dt;
    m_slotFlash[1] -= dt;
    for (int i = 0; i < FEEDBACK_MAX_BURSTS; ++i)
        m_bursts[i].left -= dt;
    for (int i = 0; i < DROP_MAX; ++i)
    {
        m_drops[i].age += dt;
        if (m_drops[i].age > DROP_SHOW_TIME)
            m_drops[i].active = false;
    }
    m_enemyFlashLeft -= dt;
    m_leakFlashLeft -= dt;
    m_shakeLeft -= dt;
    m_hpBlinkLeft -= dt;
    if (m_beatenLeft > 0.0f)
        m_beatenLeft -= dt;
    else
        m_beatenFade -= dt;
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
    // the touch layout (spec §30): split, size, then x y of the three blocks; missing or damaged = the default
    int layout[8] = { 0, touchlayout::DEFAULT_SIZE, -1, -1, -1, -1, -1, -1 };
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var text = localStorage.getItem('threeElements_layout');
            if (!text) return;
            var values = String(text).split(',').map(Number);
            for (var i = 0; i < 8 && i < values.length; ++i)
                HEAP32[($0 >> 2) + i] = values[i] | 0;
        } catch (e) {}
    }, layout);
    hint = EM_ASM_INT({
        try { return localStorage.getItem('threeElements_recipeHint') === '1' ? 1 : 0; } catch (e) { return 0; }
    });
    m_tipsSeen = EM_ASM_INT({
        try { return Number(localStorage.getItem('threeElements_tips')) | 0; } catch (e) { return 0; }
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
        if (fscanf(f, " layout %d %d %d %d %d %d %d %d", &layout[0], &layout[1], &layout[2], &layout[3], &layout[4],
            &layout[5], &layout[6], &layout[7]) != 8)
            layout[2] = -1;  // an older file: the default layout
        if (fscanf(f, " tips %d", &m_tipsSeen) != 1)
            m_tipsSeen = 0;
        fclose(f);
    }
#endif
    m_layout.split = layout[0] != 0;
    m_layout.size = layout[1];
    for (int b = 0; b < touchlayout::BLOCK_COUNT; ++b)
    {
        m_layout.x[b] = layout[2 + b * 2];
        m_layout.y[b] = layout[3 + b * 2];
    }
    touchlayout::Sanitize(m_layout);
    audio::SetMuted(muted != 0);
    m_tutorialDone = tutorialDone != 0;
    m_recipeHint = hint != 0;
}

void GameManager::SaveSettings()
{
    int muted = audio::IsMuted() ? 1 : 0;
    int tutorialDone = m_tutorialDone ? 1 : 0;
    int hint = m_recipeHint ? 1 : 0;
    int layout[8] = { m_layout.split ? 1 : 0, m_layout.size };
    for (int b = 0; b < touchlayout::BLOCK_COUNT; ++b)
    {
        layout[2 + b * 2] = m_layout.x[b];
        layout[3 + b * 2] = m_layout.y[b];
    }
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            localStorage.setItem('threeElements_muted', $0 ? '1' : '0');
            localStorage.setItem('threeElements_tutorialDone', $1 ? '1' : '0');
            localStorage.setItem('threeElements_recipeHint', $2 ? '1' : '0');
            var values = [];
            for (var i = 0; i < 8; ++i)
                values.push(HEAP32[($3 >> 2) + i]);
            localStorage.setItem('threeElements_layout', values.join(','));
            localStorage.setItem('threeElements_tips', String($4));
        } catch (e) {}
    }, muted, tutorialDone, hint, layout, m_tipsSeen);
#else
    FILE* f = fopen(SavePath("settings.txt").c_str(), "w");
    if (f != NULL)
    {
        fprintf(f, "muted %d tutorial %d hint %d layout %d %d %d %d %d %d %d %d tips %d\n", muted, tutorialDone, hint, layout[0],
            layout[1], layout[2], layout[3], layout[4], layout[5], layout[6], layout[7], m_tipsSeen);
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
        m_session.MarkAssisted();  // only while Playing (information only: the run is ranked all the same)
        if (m_bossActive)
            m_boss.MarkAssisted();
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
    if (index < MENU_MAIN_COUNT)  // the modes: a column of big buttons in the middle
    {
        SDL_Rect r = { (SCREEN_WIDTH - MENU_MAIN_W) / 2, MENU_MAIN_Y + index * MENU_MAIN_STEP, MENU_MAIN_W, MENU_MAIN_H };
        return r;
    }
    int small = MenuItemCount() - MENU_MAIN_COUNT;  // the rest: one centred row of small buttons
    int width = small * MENU_SMALL_W + (small - 1) * MENU_SMALL_GAP;
    int j = index - MENU_MAIN_COUNT;
    SDL_Rect r = { (SCREEN_WIDTH - width) / 2 + j * (MENU_SMALL_W + MENU_SMALL_GAP), MENU_SMALL_Y, MENU_SMALL_W, MENU_SMALL_H };
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
    case MENU_GUIDE:       m_showGuide = true; break;
    case MENU_LEADERBOARD: m_showLeaderboard = true; break;
    case MENU_SETTINGS:    m_showSettings = true; m_settingsIndex = 0; break;
    case MENU_SHOP:        m_showShop = true; break;
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
        bool main = i < MENU_MAIN_COUNT;
        const char* label = "";
        const char* key = "";
        switch (item)
        {
        case MENU_PLAY:        label = "PLAY";        key = "ENTER"; break;
        case MENU_SURVIVAL:    label = "SURVIVAL";    key = "S"; break;
        case MENU_BOSS:        label = "IMMORTALS"; key = "B"; break;
        case MENU_TUTORIAL:    label = "TUTORIAL";    key = "T"; break;
        case MENU_RECIPES:     label = "RECIPES";     key = "H"; break;
        case MENU_GUIDE:       label = "GUIDE";       key = ""; break;
        case MENU_LEADERBOARD: label = "LEADERBOARD"; key = "L"; break;
        case MENU_SETTINGS:    label = "SETTINGS";    key = ""; break;
        case MENU_SHOP:        label = "SHOP";        key = "P"; break;
        case MENU_QUIT:        label = "QUIT";        key = "ESC"; break;
        }
        // the modes stand out: brighter, larger, PLAY framed in gold; the rest is small and quiet
        bool accent = item == MENU_PLAY;
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        if (main)
            SDL_SetRenderDrawColor(m_screen, accent ? 70 : 28, accent ? 44 : 30, accent ? 16 : 46, selected ? 245 : 215);
        else
            SDL_SetRenderDrawColor(m_screen, 16, 18, 28, selected ? 230 : 160);
        SDL_RenderFillRect(m_screen, &r);
        bool goldFrame = selected || accent;
        SDL_SetRenderDrawColor(m_screen, goldFrame ? 255 : (main ? 150 : 100), goldFrame ? 210 : (main ? 155 : 105),
            goldFrame ? 90 : (main ? 175 : 120), main ? 235 : 180);
        for (int k = 0; k < (main && goldFrame ? 2 : 1); ++k)
        {
            SDL_Rect frame = { r.x - k, r.y - k, r.w + 2 * k, r.h + 2 * k };
            SDL_RenderDrawRect(m_screen, &frame);
        }
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

        int scale = main ? 3 : 2;
        int textH = 7 * scale;
        int textX = r.x + (r.w - pixeltext::Width(label, scale)) / 2;
        bool shop = item == MENU_SHOP;  // the shop's label is gold: the place the gold goes
        pixeltext::DrawShadowed(m_screen, label, textX, r.y + (r.h - textH) / 2, scale, selected || accent || shop ? gold : (main ? white : grey));
        if (main && selected)
            pixeltext::DrawShadowed(m_screen, ">", r.x + 14, r.y + (r.h - textH) / 2, scale, gold);
        if (main && !m_showTouchControls && key[0] != '\0')  // keyboard hints mean nothing on a touch screen
            pixeltext::DrawShadowed(m_screen, key, r.x + r.w - pixeltext::Width(key, 1) - 10, r.y + r.h - 12, 1, grey);
        if (item == MENU_TUTORIAL && !m_tutorialDone)
            RenderHighlight(r);
    }
}

// SETTINGS (owner 2026-10-01): sound and the recipe hint in one place; on a touch device also BUTTON LAYOUT (spec
// §30). A row per setting, tap or Enter to switch it (or to open the layout editor).
SDL_Rect GameManager::SettingsPanelRect() const
{
    int h = 160 + SettingsRowCount() * 70;
    SDL_Rect r = { SETTINGS_PANEL_X, (SCREEN_HEIGHT - h) / 2 - 12, SETTINGS_PANEL_W, h };
    return r;
}

SDL_Rect GameManager::SettingsRowRect(int row) const
{
    SDL_Rect p = SettingsPanelRect();
    SDL_Rect r = { p.x + 30, p.y + 70 + row * 70, p.w - 60, 50 };
    return r;
}

SDL_Rect GameManager::SettingsCloseRect() const
{
    SDL_Rect p = SettingsPanelRect();
    SDL_Rect r = { p.x + (p.w - 160) / 2, p.y + p.h - 50, 160, 34 };
    return r;
}

void GameManager::ActivateSettingsRow(int row)
{
    switch (SettingsRowAt(row))
    {
    case SETTING_SOUND:  ToggleMute(); break;
    case SETTING_HINT:   ToggleRecipeHint(); break;
    case SETTING_LAYOUT: OpenLayoutEditor(); break;
    case SETTING_TIPS:   // every first-time tip will be shown once more (spec §31)
        m_tipsSeen = 0;
        SaveSettings();
        audio::Play(audio::Sfx::CastCorrect);
        break;
    }
}

void GameManager::HandleSettingsKey(SDL_Keycode sym)
{
    int n = SettingsRowCount();
    if (sym == SDLK_UP || sym == SDLK_DOWN)
        m_settingsIndex = (m_settingsIndex + (sym == SDLK_UP ? n - 1 : 1)) % n;
    else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
        ActivateSettingsRow(m_settingsIndex);
    else if (sym == SDLK_m)
        ToggleMute();
    else if (sym == SDLK_g)
        ToggleRecipeHint();
    else if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK)
        m_showSettings = false;
}

void GameManager::HandleSettingsPointer(int x, int y)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    for (int row = 0; row < SettingsRowCount(); ++row)
    {
        if (hit(SettingsRowRect(row)))
        {
            m_settingsIndex = row;
            ActivateSettingsRow(row);
            return;
        }
    }
    if (hit(SettingsCloseRect()) || !hit(SettingsPanelRect()))
        m_showSettings = false;
}

void GameManager::RenderSettings()
{
    DimScreen(170);
    const SDL_Rect panel = SettingsPanelRect();
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 240);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 150, 155, 170, 255 };
    const SDL_Color on = { 120, 230, 130, 255 };
    const SDL_Color off = { 220, 110, 110, 255 };
    pixeltext::DrawCentered(m_screen, "SETTINGS", SCREEN_WIDTH, panel.y + 18, 3, gold);

    const char* names[4] = { "SOUND", "RECIPE HINT", "BUTTON LAYOUT", "TIPS" };
    const char* keys[4] = { "M", "G", "", "" };
    int n = SettingsRowCount();
    for (int i = 0; i < n; ++i)
    {
        SettingsRow kind = SettingsRowAt(i);
        const char* name = names[kind];
        bool value = kind == SETTING_SOUND ? !audio::IsMuted() : m_recipeHint;
        SDL_Rect r = SettingsRowRect(i);
        bool selected = i == m_settingsIndex;
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 28, 30, 46, selected ? 240 : 190);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, selected ? 255 : 110, selected ? 210 : 115, selected ? 90 : 130, 220);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        pixeltext::DrawShadowed(m_screen, name, r.x + 16, r.y + (r.h - 14) / 2, 2, selected ? gold : white);
        if (kind == SETTING_LAYOUT || kind == SETTING_TIPS)  // not a switch: it opens the editor / shows the tips again
        {
            const char* action = kind == SETTING_LAYOUT ? "EDIT" : (m_tipsSeen == 0 ? "ALL ON" : "SHOW AGAIN");
            int scale = kind == SETTING_LAYOUT ? 3 : 2;
            pixeltext::DrawShadowed(m_screen, action, r.x + r.w - pixeltext::Width(action, scale) - 16, r.y + (r.h - 7 * scale) / 2,
                scale, m_tipsSeen == 0 && kind == SETTING_TIPS ? on : gold);
            continue;
        }
        const char* text = value ? "ON" : "OFF";
        int valueX = r.x + r.w - pixeltext::Width(text, 3) - 16;
        pixeltext::DrawShadowed(m_screen, text, valueX, r.y + (r.h - 21) / 2, 3, value ? on : off);
        if (!m_showTouchControls)
            pixeltext::DrawShadowed(m_screen, keys[kind], valueX - 30, r.y + (r.h - 7) / 2, 1, grey);
    }
    pixeltext::DrawCentered(m_screen, "HINT SHOWS THE KEYS OF EACH SPELL, AND WHEN TO CAST AGAINST AN IMMORTAL.", SCREEN_WIDTH,
        SettingsRowRect(n - 1).y + 62, 1, grey);
    RenderButton(SettingsCloseRect(), "CLOSE", false);
}

// ------------------------------------------------------------------ BUTTON LAYOUT editor (spec §30)

void GameManager::OpenLayoutEditor()
{
    m_showSettings = false;
    m_showLayout = true;
    m_dragBlock = -1;
}

void GameManager::CloseLayoutEditor()
{
    LayoutPointerUp();  // a block still held goes where it may stay
    m_showLayout = false;
    SaveSettings();
}

// The four buttons along the top, or the block under the finger starts to move.
void GameManager::LayoutPointerDown(int x, int y)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    if (hit(LAYOUT_SPLIT_RECT))  // one cluster <-> Q W E and R / D F apart: each starts from its own default
        m_layout = touchlayout::Default(!m_layout.split, m_layout.size);
    else if (hit(LAYOUT_SIZE_RECT))
    {
        m_layout.size = (m_layout.size + 1) % touchlayout::SIZE_COUNT;
        for (int b = 0; b < touchlayout::BLOCK_COUNT; ++b)
            touchlayout::Clamp(m_layout, b);
        touchlayout::Sanitize(m_layout);  // larger buttons that no longer fit side by side: that mode's default
    }
    else if (hit(LAYOUT_RESET_RECT))
        m_layout = touchlayout::Default(false, touchlayout::DEFAULT_SIZE);
    else if (hit(LAYOUT_DONE_RECT))
    {
        CloseLayoutEditor();
        return;
    }
    else
    {
        for (int b = touchlayout::BLOCK_COUNT - 1; b >= 0; --b)
        {
            touchlayout::Box box = touchlayout::BlockBox(m_layout, b);
            if (box.w > 0 && x >= box.x && x < box.x + box.w && y >= box.y && y < box.y + box.h)
            {
                m_dragBlock = b;
                m_dragDX = x - box.x;
                m_dragDY = y - box.y;
                m_dragFromX = box.x;
                m_dragFromY = box.y;
                return;
            }
        }
        return;
    }
    audio::Play(audio::Sfx::Cast);
    SaveSettings();
}

void GameManager::LayoutPointerMove(int x, int y)
{
    if (m_dragBlock < 0)
        return;
    m_layout.x[m_dragBlock] = x - m_dragDX;
    m_layout.y[m_dragBlock] = y - m_dragDY;
    touchlayout::Clamp(m_layout, m_dragBlock);
}

void GameManager::LayoutPointerUp()
{
    if (m_dragBlock < 0)
        return;
    if (!touchlayout::Valid(m_layout))  // dropped on another block: back where it came from
    {
        m_layout.x[m_dragBlock] = m_dragFromX;
        m_layout.y[m_dragBlock] = m_dragFromY;
        audio::Play(audio::Sfx::CastWrong);
    }
    m_dragBlock = -1;
    SaveSettings();
}

// The buttons where they are now, each group in a frame the player drags; the orb row is drawn where it will be;
// the strip at the bottom stays free for the enemies.
void GameManager::RenderLayoutEditor()
{
    const SDL_Color grey = { 170, 175, 190, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    DimScreen(235);  // the menu behind must not distract: only the layout is shown

    // the area the groups may be in
    SDL_Rect area = { touchlayout::AREA_LEFT, touchlayout::AREA_TOP, touchlayout::AREA_RIGHT - touchlayout::AREA_LEFT,
        touchlayout::AREA_BOTTOM - touchlayout::AREA_TOP };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 40);
    SDL_RenderDrawRect(m_screen, &area);

    // the orb row, where it goes with this layout
    int shift = touchlayout::HudShift(m_layout);
    for (int i = 0; i < 3; ++i)
    {
        SDL_SetRenderDrawColor(m_screen, 200, 200, 210, 110);
        draw::FillCircle(m_screen, elementPos[i].first + shift, elementPos[i].second, 20);
        SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 160);
        draw::FillCircle(m_screen, elementPos[i].first + shift, elementPos[i].second, 17);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    // the item bar and the buttons
    touchlayout::Box items = touchlayout::BlockBox(m_layout, touchlayout::BLOCK_ITEMS);
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)
    {
        SDL_Rect r = LoadoutSlotRect(s, items.x, items.y);
        SDL_SetRenderDrawColor(m_screen, 30, 32, 46, 255);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, 110, 115, 135, 255);
        SDL_RenderDrawRect(m_screen, &r);
    }
    pixeltext::DrawShadowed(m_screen, "ITEMS", items.x + (items.w - pixeltext::Width("ITEMS", 2)) / 2, items.y + items.h / 2 - 7, 2, white);
    DrawTouchButtons();

    // a frame around each group: gold, red while it lies on another group
    bool valid = touchlayout::Valid(m_layout);
    for (int b = 0; b < touchlayout::BLOCK_COUNT; ++b)
    {
        touchlayout::Box box = touchlayout::BlockBox(m_layout, b);
        if (box.w <= 0)
            continue;
        bool held = b == m_dragBlock;
        if (held && !valid)
            SDL_SetRenderDrawColor(m_screen, 235, 70, 70, 255);
        else
            SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 255);
        for (int k = 3; k < (held ? 7 : 5); ++k)
        {
            SDL_Rect frame = { box.x - k, box.y - k, box.w + 2 * k, box.h + 2 * k };
            SDL_RenderDrawRect(m_screen, &frame);
        }
    }

    const char* sizes[touchlayout::SIZE_COUNT] = { "SIZE: SMALL", "SIZE: MEDIUM", "SIZE: LARGE" };
    RenderButton(LAYOUT_SPLIT_RECT, m_layout.split ? "GROUPS: 2" : "GROUPS: 1", false);
    RenderButton(LAYOUT_SIZE_RECT, sizes[m_layout.size], false);
    RenderButton(LAYOUT_RESET_RECT, "RESET", false);
    RenderButton(LAYOUT_DONE_RECT, "DONE", true);
    pixeltext::DrawCentered(m_screen, "DRAG A GROUP TO WHERE YOUR THUMBS REST.  GROUPS: 2 = Q W E AND R D F APART.", SCREEN_WIDTH, 76, 1, grey);
    pixeltext::DrawCentered(m_screen, "THE STRIP BELOW STAYS FREE FOR THE ENEMIES.", SCREEN_WIDTH, 92, 1, grey);
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
    if (online::Available())  // spec §29: the global boards are Google's screen, one tap away
        RenderButton(GLOBAL_BOARDS_BUTTON_RECT, online::SignedIn() ? "GLOBAL RANKING" : "SIGN IN FOR GLOBAL RANKING", true);
    snprintf(buf, sizeof(buf), "GOLD BANK %d", m_goldBank);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, panel.y + panel.h - 54, 2, gold);
    pixeltext::DrawCentered(m_screen, online::Available() ? "TAP OUTSIDE THE BUTTON TO CLOSE" : "PRESS ANY KEY OR TAP TO CLOSE",
        SCREEN_WIDTH, panel.y + panel.h - 28, 2, grey);
}

// A player who played as a guest (or offline) and signs in later still gets this device's best results on the
// global boards: they are sent again every time the boards are opened (a lower score never replaces a higher one).
void GameManager::OpenGlobalBoards()
{
    online::Submit(online::Board::Play, m_playRuns[0].score);
    online::Submit(online::Board::Survival, static_cast<long long>(m_topRuns[0].survivalTime * 1000.0f));
    online::ShowBoards();
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

    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);  // the shadow under the feet
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, SHADOW_ALPHA);
    FillEllipse(m_screen, cx, ground + 2, PLAYER_SHADOW_RX, PLAYER_SHADOW_RY);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

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

    if (m_playerRunSheet != NULL)  // the Injoker running, or casting just after D / F (owner 2026-10-01)
    {
        bool casting = m_castAnim >= 0.0f && m_playerCastSheet != NULL;
        int frame = casting ? PLAYER_CAST_START + static_cast<int>(m_castAnim * PLAYER_CAST_FPS)
                            : static_cast<int>(t * PLAYER_RUN_FPS) % PLAYER_RUN_FRAMES;
        if (frame > PLAYER_CAST_FRAMES - 1)
            frame = PLAYER_CAST_FRAMES - 1;
        SDL_Rect src = { (frame % PLAYER_RUN_COLUMNS) * PLAYER_RUN_FRAME, (frame / PLAYER_RUN_COLUMNS) * PLAYER_RUN_FRAME,
            PLAYER_RUN_FRAME, PLAYER_RUN_FRAME };
        SDL_Rect dst = { cx - PLAYER_RUN_BODY_CENTER * PLAYER_RUN_DRAW / PLAYER_RUN_FRAME,
            ground - PLAYER_RUN_FEET_ROW * PLAYER_RUN_DRAW / PLAYER_RUN_FRAME - 2, PLAYER_RUN_DRAW, PLAYER_RUN_DRAW };
        SDL_RenderCopy(m_screen, casting ? m_playerCastSheet : m_playerRunSheet, &src, &dst);
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
        if (hit(TouchButtonRect(i)))
        {
            ProcessTutorialAction(kTouchActions[i]);
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
        if (!(flags & bit))
            continue;
        if (m_showTouchControls)  // the D / F button is the slot there
            RenderHighlight(TouchButtonRect(4 + i));
        else
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
        else if (skill == invoker::SkillId::IceWall && m_iceWallSheet != NULL)
        {
            if (e.left > 0.0f)
                RenderIceWallSprite(skillvfx::Duration(skill) - e.left, e.left);
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

// A sheet of any size, kept at its own resolution: the Immortals' frames are drawn 1:1.
SDL_Texture* GameManager::LoadTexture(const char* path)
{
    SDL_Surface* surface = IMG_Load(path);
    if (surface == NULL)
    {
        printf("Failed to load %s: %s\n", path, IMG_GetError());
        return NULL;
    }
    SDL_Texture* texture = NULL;
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);  // palette + transparency -> RGBA
    if (rgba != NULL)
    {
        texture = SDL_CreateTextureFromSurface(m_screen, rgba);
        SDL_FreeSurface(rgba);
    }
    SDL_FreeSurface(surface);
    if (texture != NULL)
    {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    }
    return texture;
}

// Ice Wall: the wall of shards appears in front of the player, glints while it stands, and fades out at the end.
void GameManager::RenderIceWallSprite(float age, float left)
{
    if (age < 0.0f)
        return;
    int frame = static_cast<int>(age / ICE_WALL_GROW_TIME * ICE_WALL_FRAMES);
    if (frame > ICE_WALL_FRAMES - 1)  // standing: the last frames go back and forth, so the ice keeps glinting
    {
        const int span = ICE_WALL_FRAMES - 1 - ICE_WALL_IDLE_FIRST;
        int step = static_cast<int>((age - ICE_WALL_GROW_TIME) * ICE_WALL_IDLE_FPS) % (2 * span);
        frame = ICE_WALL_FRAMES - 1 - (step <= span ? step : 2 * span - step);
    }
    // the picture itself is never stretched: it only fades in and out
    float in = age < ICE_WALL_FADE_IN ? age / ICE_WALL_FADE_IN : 1.0f;
    float out = left < ICE_WALL_FADE_OUT ? left / ICE_WALL_FADE_OUT : 1.0f;
    SDL_SetTextureAlphaMod(m_iceWallSheet, static_cast<Uint8>(255.0f * (in < out ? in : out)));
    SDL_Rect src = { (frame % ICE_WALL_COLUMNS) * ICE_WALL_FRAME, (frame / ICE_WALL_COLUMNS) * ICE_WALL_FRAME, ICE_WALL_FRAME, ICE_WALL_FRAME };
    const int bottom = static_cast<int>(practice::GROUND_LINE_Y) + ICE_WALL_SINK;
    SDL_Rect dst = { PLAYER_BODY_CENTER_X + ICE_WALL_AHEAD - ICE_WALL_DRAW / 2, bottom - ICE_WALL_DRAW, ICE_WALL_DRAW, ICE_WALL_DRAW };
    SDL_RenderCopy(m_screen, m_iceWallSheet, &src, &dst);
    SDL_SetTextureAlphaMod(m_iceWallSheet, 255);
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

    // On a touch screen the D / F buttons show the skills themselves (DrawTouchButtons): no second copy under the
    // orbs (owner 2026-10-01). With a keyboard there are no such buttons, so the slots are drawn here.
    if (m_showTouchControls)
        return;

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
        if (m_boss.State() == practice::BossState::Fighting)  // afterwards the result panel has the numbers
            RenderBossHud();
        return;
    }
    if (!m_tutorialActive && m_session.State() != practice::GameState::Playing)
        return;  // Ready: the menu is there; Game Over: the result panel shows every number
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
            snprintf(runes + strlen(runes), sizeof(runes) - strlen(runes), "SHIELD  ");
        if (m_session.BkbLeft() > 0.0f)
            snprintf(runes + strlen(runes), sizeof(runes) - strlen(runes), "BKB %d  ", static_cast<int>(m_session.BkbLeft()) + 1);
        if (m_session.SmokeLeft() > 0.0f)
            snprintf(runes + strlen(runes), sizeof(runes) - strlen(runes), "SMOKE %d", static_cast<int>(m_session.SmokeLeft()) + 1);
        const SDL_Color cyan = { 110, 210, 255, 255 };
        pixeltext::DrawShadowed(m_screen, runes, 190, 68, 2, cyan);
    }

    // reminder of the control that leaves the session (only while playing)
    if (m_session.State() == practice::GameState::Playing)
    {
        const SDL_Color grey = { 200, 200, 210, 255 };
        const char* pause = m_showTouchControls ? "PAUSE" : "ESC  PAUSE";
        pixeltext::DrawShadowed(m_screen, pause, SCREEN_WIDTH - pixeltext::Width(pause, 2) - 16, 42, 2, grey);
    }
}

void GameManager::DimScreen(Uint8 alpha)
{
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, alpha);
    SDL_RenderFillRect(m_screen, NULL);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
}

// Home: the name, the menu (modes big, the rest small) and the top 3 beside it (owner 2026-10-01).
void GameManager::RenderReadyScreen()
{
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 170, 175, 190, 255 };
    DimScreen(150);
    // the name only: the logo picture is gone from Home (owner 2026-10-01)
    pixeltext::DrawCentered(m_screen, "INJOKER", SCREEN_WIDTH, 30, 7, gold);
    pixeltext::DrawCentered(m_screen, "INVOKE THE ELEMENTS", SCREEN_WIDTH, 88, 2, grey);
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
    // Survival: this run, then where it stands (rank by survival time, the three records)
    const practice::Stats& st = m_session.GetStats();
    const SDL_Color red = { 235, 70, 70, 255 };
    const SDL_Color none = { 0, 0, 0, 0 };
    char value[32];

    ResultBegin("GAME OVER", red, NULL, none, 7, 1, 0);
    snprintf(value, sizeof(value), "%d", st.score);
    ResultRow("SCORE", value, m_lastBestUpdate.score ? "NEW BEST!" : NULL, m_lastBestUpdate.score ? 1 : 0);
    FormatAccuracy(value, sizeof(value), st);
    ResultRow("ACCURACY", value);
    FormatTime(value, sizeof(value), st.survivalTime);
    ResultRow("TIME", value, m_lastBestUpdate.survivalTime ? "NEW BEST!" : NULL, m_lastBestUpdate.survivalTime ? 1 : 0);
    ResultSeparator();
    if (m_lastRank > 0)
    {
        snprintf(value, sizeof(value), "#%d", m_lastRank);
        ResultRow("RANK", value, "ON THIS DEVICE", 1);
    }
    else
        ResultRow("RANK", "NOT IN THE TOP 10", NULL, 2);
    snprintf(value, sizeof(value), "%d", m_bests.score);
    ResultRow("RECORD SCORE", value, NULL, 2);
    snprintf(value, sizeof(value), "%d", m_bests.combo);  // the combo record lives across runs (spec §13)
    ResultRow("RECORD COMBO", value, m_lastBestUpdate.combo ? "NEW BEST!" : NULL, m_lastBestUpdate.combo ? 1 : 2);
    FormatTime(value, sizeof(value), m_bests.survivalTime);
    ResultRow("RECORD TIME", value, NULL, 2);
    ResultButtons("PLAY AGAIN", "MENU", true);
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
        const char* kind = e.kind == practice::EnemyKind::Overlord ? "OVERLORD" : "ELITE";
        const int size = 36, gap = 18;
        int x0 = SCREEN_WIDTH - 16 - (e.chainLength * size + (e.chainLength - 1) * gap);
        int y = tile.y + tile.h + 8;
        pixeltext::DrawShadowed(m_screen, kind, x0 - 12 - pixeltext::Width(kind, 2), y + 11, 2,
            e.kind == practice::EnemyKind::Overlord ? red : orange);
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
    case 50: return "+50";    // an IMMORTAL (§28 O-8)
    case 100: return "+100";
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
                m_floatTexts[i] = { FEEDBACK_TEXT_TIME * 1.5f, x, static_cast<int>(enemy.y) - (kill.immortal ? 118 : 44), m_goldText, gold };
                break;
            }
        }
    }
    float dropX = enemy.x + enemy.w * 0.5f, dropY = enemy.y + enemy.h * 0.5f;
    bool hasRune = kill.rune != practice::Rune::None;
    for (int d = 0; d < kill.materialCount; ++d)  // §32 M-7: kept for good, like the gold
    {
        int m = static_cast<int>(kill.materials[d]);
        m_inventory.material[m] = m_session.Loadout().material[m];
        ++m_runMaterials[m];
        SaveInventory();
        char label[32];
        snprintf(label, sizeof(label), "+1 %s", practice::MaterialName(kill.materials[d]));
        AddDrop(m_materialIcons[m], label, dropX, dropY, d == 0 ? (hasRune || kill.materialCount > 1 ? -80 : 0) : (hasRune ? 0 : 80));
    }
    if (kill.immortalChance > 0)  // §32 M-3: the chance grew (and was rolled): the bar lights up
    {
        m_meterFlashLeft = IMMORTAL_METER_FLASH;
        snprintf(m_meterText, sizeof(m_meterText), "+%d%%", kill.kind == practice::EnemyKind::Overlord
            ? practice::PLAY_IMMORTAL_CHANCE_OVERLORD : practice::PLAY_IMMORTAL_CHANCE_ELITE);
        const SDL_Color violet = { 200, 150, 255, 255 };
        for (int i = 0; i < FEEDBACK_MAX_TEXTS; ++i)
        {
            if (m_floatTexts[i].left <= 0.0f)
            {
                m_floatTexts[i] = { FEEDBACK_TEXT_TIME * 1.5f, SCREEN_WIDTH / 2 + 40, IMMORTAL_BAR_Y - 36, m_meterText, violet };
                break;
            }
        }
        printf("[play] immortal chance %d%%%s\n", kill.immortalChance, kill.immortalComing ? " - it is coming" : "");
    }
    if (hasRune || kill.runeChoice)
    {
        m_tipRune = kill.rune;
        QueueTip(TIP_RUNE);
    }
    if (kill.gold > 0)
        QueueTip(TIP_GOLD);
    if (kill.materialCount > 0)
    {
        m_tipMaterial = kill.materials[0];
        QueueTip(TIP_MATERIAL);
    }
    if (hasRune)  // the player sees what fell, not only its name (owner 2026-10-01)
        AddDrop(m_runeIcons[static_cast<int>(kill.rune)], RuneName(kill.rune), dropX, dropY, kill.materialCount > 0 ? 80 : 0);
    if (kill.kind == practice::EnemyKind::Overlord && m_session.Mode() == practice::SessionMode::Play)
    {
        if (kill.immortal)
            snprintf(m_stageText, sizeof(m_stageText), "%s DEFEATED - STAGE %d",
                practice::GetBossDefinition(m_session.ImmortalBoss()).name, m_session.GetStats().bossesDefeated + 1);
        else
            snprintf(m_stageText, sizeof(m_stageText), "OVERLORD DEFEATED - STAGE %d", m_session.GetStats().bossesDefeated + 1);
        switch (kill.rune)
        {
        case practice::Rune::None:         m_announce = kill.runeChoice ? "CHOOSE YOUR RUNE" : NULL; break;
        case practice::Rune::Regeneration: m_announce = "RUNE: REGENERATION  +1 LIFE"; break;
        case practice::Rune::Frost:        m_announce = "RUNE: FROST  ENEMIES SLOWED"; break;
        case practice::Rune::DoubleDamage: m_announce = "RUNE: DOUBLE DAMAGE  SCORE X2"; break;
        case practice::Rune::Bounty:       m_announce = "RUNE: BOUNTY  +25 GOLD"; break;
        case practice::Rune::Shield:       m_announce = "RUNE: SHIELD  NEXT HIT BLOCKED"; break;
        default:                           m_announce = NULL; break;
        }
        if (kill.materialCount == 1)  // the drop and the rune share the second line
        {
            snprintf(m_announceText, sizeof(m_announceText), "+1 %s   %s", practice::MaterialName(kill.materials[0]),
                m_announce != NULL ? m_announce : "");
            m_announce = m_announceText;
        }
        else if (kill.materialCount > 1)  // two drops: their names are on the ground with the icons
        {
            snprintf(m_announceText, sizeof(m_announceText), "+%d MATERIALS   %s", kill.materialCount,
                m_announce != NULL ? m_announce : "");
            m_announce = m_announceText;
        }
        m_announceLeft = kill.immortal ? ANNOUNCE_TIME * 1.5f : ANNOUNCE_TIME;
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
    const SDL_Color red = { 235, 70, 70, 255 };
    const SDL_Color orange = { 255, 170, 70, 255 };
    const SDL_Color grey = { 165, 172, 192, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    char value[48], note[48];

    bool anyMaterial = false;
    for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
        anyMaterial = anyMaterial || m_runMaterials[m] > 0;
    int defeatedBy = m_session.DefeatedBy();  // §28 O-9: the IMMORTAL that ended the run
    char subtitle[64] = "";
    if (defeatedBy >= 0)
        snprintf(subtitle, sizeof(subtitle), "DEFEATED BY %s", practice::GetBossDefinition(defeatedBy).name);

    ResultBegin("GAME OVER", red, defeatedBy >= 0 ? subtitle : NULL, orange, anyMaterial ? 9 : 8, 1, defeatedBy >= 0 ? 16 : 0);
    bool best = m_lastRank == 1;
    snprintf(value, sizeof(value), "%d", st.score);
    ResultRow("SCORE", value, best ? "NEW BEST!" : NULL, best ? 1 : 0);
    snprintf(value, sizeof(value), "%d", st.bossesDefeated + 1);
    ResultRow("STAGE", value);
    snprintf(value, sizeof(value), "%d", st.kills);
    ResultRow("ENEMIES DEFEATED", value);
    FormatTime(value, sizeof(value), st.survivalTime);
    ResultRow("TIME", value);
    FormatAccuracy(value, sizeof(value), st);
    ResultRow("ACCURACY", value);
    snprintf(value, sizeof(value), "+%d", st.gold);
    snprintf(note, sizeof(note), "BANK %d", m_goldBank);
    ResultRow("GOLD EARNED", value, note, 1);
    if (anyMaterial)  // §28 O-10: what this run brought home, as icons
    {
        int y = m_resultY, x = RESULT_VALUE_X;
        ResultRow("MATERIALS", NULL);
        for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
        {
            if (m_runMaterials[m] <= 0)
                continue;
            RenderIconScaled(m_materialIcons[m], x, y - 5, 24);
            snprintf(value, sizeof(value), "X%d", m_runMaterials[m]);
            pixeltext::DrawShadowed(m_screen, value, x + 28, y, 2, white);
            x += 28 + pixeltext::Width(value, 2) + 18;
        }
    }
    ResultSeparator();
    if (m_lastRank > 0)
    {
        snprintf(value, sizeof(value), "#%d", m_lastRank);
        ResultRow("RANK", value, "ON THIS DEVICE", 1);
    }
    else
        ResultRow("RANK", "NOT IN THE TOP 10", NULL, 2);
    snprintf(value, sizeof(value), "%d", m_playRuns[0].score);
    ResultRow("BEST SCORE", value, NULL, 2);
    if (defeatedBy >= 0)
        pixeltext::DrawShadowed(m_screen, "TIP: PRACTISE ITS COMBO IN IMMORTALS (MENU).", RESULT_PANEL_X + 24, m_resultY + 2, 1, grey);
    ResultButtons("PLAY AGAIN", "MENU", true);
}

// ------------------------------------------------------------------ shop and items (spec §27)

static const char* const kItemFiles[practice::ITEM_COUNT] = {
    "blink", "refresher", "euls", "bkb", "midas", "octarine", "aghanim", "salve", "cheese", "smoke", "greatersmoke" };

void GameManager::LoadItemIcons()
{
    char path[96];
    for (int i = 0; i < practice::ITEM_COUNT; ++i)
    {
        for (int l = 0; l < practice::GetItemDefinition(i).levels; ++l)
        {
            snprintf(path, sizeof(path), "assets/items/%s%d.png", kItemFiles[i], l + 1);
            m_itemIcons[i][l] = LoadSheet(path, ITEM_ICON);  // 48 x 48, drawn 1:1
        }
    }
    const char* const materials[practice::MATERIAL_COUNT] = { "pointbooster", "mysticstaff", "sacredrelic" };
    for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
    {
        snprintf(path, sizeof(path), "assets/items/mat_%s.png", materials[m]);
        m_materialIcons[m] = LoadSheet(path, ITEM_ICON);
    }
    const char* const runes[6] = { "", "regen", "frost", "double", "bounty", "shield" };  // practice::Rune order
    for (int r = 1; r < 6; ++r)
    {
        snprintf(path, sizeof(path), "assets/items/rune_%s.png", runes[r]);
        m_runeIcons[r] = LoadSheet(path, ITEM_ICON);
    }
}

static const char* RuneName(practice::Rune rune)
{
    switch (rune)
    {
    case practice::Rune::Regeneration: return "REGENERATION";
    case practice::Rune::Frost:        return "FROST";
    case practice::Rune::DoubleDamage: return "DOUBLE DAMAGE";
    case practice::Rune::Bounty:       return "BOUNTY";
    case practice::Rune::Shield:       return "SHIELD";
    default:                           return "";
    }
}

void GameManager::RenderIconScaled(SDL_Texture* icon, int x, int y, int size)
{
    if (icon == NULL)
        return;
    SDL_Rect dst = { x, y, size, size };
    SDL_RenderCopy(m_screen, icon, NULL, &dst);
}

// A reward appears where the boss fell: `offsetX` spreads two rewards apart.
void GameManager::AddDrop(SDL_Texture* icon, const char* label, float x, float y, int offsetX)
{
    for (int i = 0; i < DROP_MAX; ++i)
    {
        Drop& d = m_drops[i];
        if (d.active)
            continue;
        d.active = true;
        d.age = 0.0f;
        d.x0 = x;
        d.y0 = y;
        float rest = x + static_cast<float>(offsetX);
        d.x1 = rest < 180.0f ? 180.0f : (rest > SCREEN_WIDTH - 110.0f ? SCREEN_WIDTH - 110.0f : rest);
        d.icon = icon;
        snprintf(d.label, sizeof(d.label), "%s", label);
        return;
    }
}

void GameManager::RenderDrops()
{
    const SDL_Color white = { 255, 255, 255, 255 };
    for (int i = 0; i < DROP_MAX; ++i)
    {
        const Drop& d = m_drops[i];
        if (!d.active)
            continue;
        float t = d.age < DROP_HOP_TIME ? d.age / DROP_HOP_TIME : 1.0f;
        float x = d.x0 + (d.x1 - d.x0) * t;
        float y = d.y0 + (DROP_REST_Y - d.y0) * t - 90.0f * 4.0f * t * (1.0f - t);  // a hop out of the boss
        if (t >= 1.0f)
            y += 3.0f * std::sin((d.age - DROP_HOP_TIME) * 5.0f);                    // then it bobs gently
        float left = DROP_SHOW_TIME - d.age;
        float alpha = left < DROP_FADE_TIME ? (left > 0.0f ? left / DROP_FADE_TIME : 0.0f) : 1.0f;
        int cx = static_cast<int>(x), cy = static_cast<int>(y);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 255, 215, 90, static_cast<Uint8>(70 * alpha));
        draw::FillCircle(m_screen, cx, cy, 36);
        SDL_SetRenderDrawColor(m_screen, 255, 235, 150, static_cast<Uint8>(90 * alpha));
        draw::FillCircle(m_screen, cx, cy, 30);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        if (d.icon != NULL)
        {
            SDL_SetTextureAlphaMod(d.icon, static_cast<Uint8>(255 * alpha));
            RenderIconScaled(d.icon, cx - ITEM_ICON / 2, cy - ITEM_ICON / 2, ITEM_ICON);
            SDL_SetTextureAlphaMod(d.icon, 255);
        }
        if (t >= 1.0f && alpha >= 1.0f)
            pixeltext::DrawShadowed(m_screen, d.label, cx - pixeltext::Width(d.label, 1) / 2, cy + ITEM_ICON / 2 + 6, 1, white);
    }
}

// The inventory: items.txt next to the gold (11 levels, 11 counts, 6 slots, and since 1.6 the 3 materials),
// localStorage on the web. Anything damaged or missing counts as nothing owned (a file from before 1.6 has no
// materials); slots only keep items that are owned, each once.
void GameManager::LoadInventory()
{
    const int slots = practice::ITEM_COUNT * 2, materials = slots + practice::ITEM_SLOTS;
    const int n = materials + practice::MATERIAL_COUNT;
    int raw[practice::ITEM_COUNT * 2 + practice::ITEM_SLOTS + practice::MATERIAL_COUNT];
    for (int i = 0; i < n; ++i)
        raw[i] = i >= slots && i < materials ? practice::ITEM_NONE : 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var text = localStorage.getItem('threeElements_items');
            if (!text) return;
            var values = String(text).split(',').map(Number);
            for (var i = 0; i < $1 && i < values.length; ++i)
                HEAP32[($0 >> 2) + i] = values[i] | 0;
        } catch (e) {}
    }, raw, n);
#else
    FILE* f = fopen(SavePath("items.txt").c_str(), "r");
    if (f != NULL)
    {
        for (int i = 0; i < n; ++i)
            if (fscanf(f, "%d", &raw[i]) != 1)
                break;
        fclose(f);
    }
#endif
    m_inventory = practice::EmptyInventory();
    for (int i = 0; i < practice::ITEM_COUNT; ++i)
    {
        int maxLevel = practice::GetItemDefinition(i).levels;
        bool consumable = practice::GetItemDefinition(i).kind == practice::ItemKind::Consumable;
        int level = raw[i], count = raw[practice::ITEM_COUNT + i];
        m_inventory.level[i] = consumable ? 0 : (level < 0 ? 0 : (level > maxLevel ? maxLevel : level));
        m_inventory.count[i] = consumable ? (count < 0 ? 0 : (count > practice::ITEM_MAX_STACK ? practice::ITEM_MAX_STACK : count)) : 0;
    }
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)
    {
        int id = raw[practice::ITEM_COUNT * 2 + s];
        if (id >= 0 && id < practice::ITEM_COUNT && !practice::IsEquipped(m_inventory, static_cast<practice::ItemId>(id))
            && practice::Owns(m_inventory, static_cast<practice::ItemId>(id)))
            m_inventory.slot[s] = id;
    }
    for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
    {
        int count = raw[materials + m];
        m_inventory.material[m] = count < 0 ? 0 : (count > practice::MATERIAL_MAX ? practice::MATERIAL_MAX : count);
    }
}

void GameManager::SaveInventory()
{
    const int n = practice::ITEM_COUNT * 2 + practice::ITEM_SLOTS + practice::MATERIAL_COUNT;
    int raw[practice::ITEM_COUNT * 2 + practice::ITEM_SLOTS + practice::MATERIAL_COUNT];
    for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
        raw[practice::ITEM_COUNT * 2 + practice::ITEM_SLOTS + m] = m_inventory.material[m];
    for (int i = 0; i < practice::ITEM_COUNT; ++i)
    {
        raw[i] = m_inventory.level[i];
        raw[practice::ITEM_COUNT + i] = m_inventory.count[i];
    }
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)
        raw[practice::ITEM_COUNT * 2 + s] = m_inventory.slot[s];
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var values = [];
            for (var i = 0; i < $1; ++i)
                values.push(HEAP32[($0 >> 2) + i]);
            localStorage.setItem('threeElements_items', values.join(','));
        } catch (e) {}
    }, raw, n);
#else
    FILE* f = fopen(SavePath("items.txt").c_str(), "w");
    if (f != NULL)
    {
        for (int i = 0; i < n; ++i)
            fprintf(f, "%d ", raw[i]);
        fprintf(f, "\n");
        fclose(f);
    }
#endif
}

void GameManager::RenderItemIcon(practice::ItemId id, int level, int x, int y)
{
    int i = static_cast<int>(id);
    int l = level >= 2 ? 1 : 0;
    SDL_Texture* icon = m_itemIcons[i][l] != NULL ? m_itemIcons[i][l] : m_itemIcons[i][0];
    if (icon == NULL)
        return;
    SDL_Rect dst = { x, y, ITEM_ICON, ITEM_ICON };
    SDL_RenderCopy(m_screen, icon, NULL, &dst);
}

SDL_Rect GameManager::ShopCardRect(int index) const
{
    SDL_Rect r = { SHOP_GRID_X + (index % SHOP_COLUMNS) * (SHOP_CARD_W + SHOP_CARD_GAP),
        SHOP_GRID_Y + (index / SHOP_COLUMNS) * (SHOP_CARD_H + SHOP_CARD_GAP), SHOP_CARD_W, SHOP_CARD_H };
    return r;
}

SDL_Rect GameManager::LoadoutSlotRect(int slot, int x0, int y0) const
{
    SDL_Rect r = { x0 + (slot % 3) * (ITEM_SLOT + ITEM_SLOT_GAP), y0 + (slot / 3) * (ITEM_SLOT + ITEM_SLOT_GAP), ITEM_SLOT, ITEM_SLOT };
    return r;
}

void GameManager::ShopBuy()
{
    practice::ItemId id = static_cast<practice::ItemId>(m_shopSelect);
    if (practice::Buy(m_inventory, id, m_goldBank))
    {
        SaveGold();
        SaveInventory();
        audio::Play(audio::Sfx::CastCorrect);
        printf("[shop] bought %s (gold left %d)\n", practice::GetItemDefinition(id).level[0].name, m_goldBank);
    }
    else
        audio::Play(audio::Sfx::CastWrong);
}

void GameManager::ShopToggleEquip()
{
    practice::ItemId id = static_cast<practice::ItemId>(m_shopSelect);
    if (practice::IsEquipped(m_inventory, id))
        practice::Unequip(m_inventory, id);
    else if (!practice::Equip(m_inventory, id))
    {
        audio::Play(audio::Sfx::CastWrong);  // not owned, or all six slots taken
        return;
    }
    SaveInventory();
    audio::Play(audio::Sfx::Cast);
}

// Arrows move through the grid, Enter / B buys, E equips or unequips, Esc closes.
void GameManager::HandleShopKey(SDL_Keycode sym)
{
    int n = practice::ITEM_COUNT;
    if (sym == SDLK_LEFT)
        m_shopSelect = (m_shopSelect + n - 1) % n;
    else if (sym == SDLK_RIGHT)
        m_shopSelect = (m_shopSelect + 1) % n;
    else if (sym == SDLK_UP)
        m_shopSelect = m_shopSelect >= SHOP_COLUMNS ? m_shopSelect - SHOP_COLUMNS : m_shopSelect;
    else if (sym == SDLK_DOWN)
        m_shopSelect = m_shopSelect + SHOP_COLUMNS < n ? m_shopSelect + SHOP_COLUMNS : m_shopSelect;
    else if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE || sym == SDLK_b)
        ShopBuy();
    else if (sym == SDLK_e)
        ShopToggleEquip();
    else if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK || sym == SDLK_p)
        m_showShop = false;
    else if (sym == SDLK_m)
        ToggleMute();
}

void GameManager::HandleShopPointer(int x, int y)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    for (int i = 0; i < practice::ITEM_COUNT; ++i)
    {
        if (hit(ShopCardRect(i)))
        {
            m_shopSelect = i;
            return;
        }
    }
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)  // a tap on a loadout slot takes its item off
    {
        if (hit(LoadoutSlotRect(s, SHOP_LOADOUT_X, SHOP_LOADOUT_Y)) && m_inventory.slot[s] != practice::ITEM_NONE)
        {
            m_shopSelect = m_inventory.slot[s];
            m_inventory.slot[s] = practice::ITEM_NONE;
            SaveInventory();
            audio::Play(audio::Sfx::Cast);
            return;
        }
    }
    if (hit(SHOP_BUY_RECT))
        ShopBuy();
    else if (hit(SHOP_EQUIP_RECT))
        ShopToggleEquip();
    else if (hit(SHOP_CLOSE_RECT) || !hit(SHOP_PANEL_RECT))
        m_showShop = false;
}

void GameManager::RenderShop()
{
    DimScreen(190);
    const SDL_Rect& panel = SHOP_PANEL_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 245);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 150, 155, 170, 255 };
    const SDL_Color green = { 120, 230, 130, 255 };
    char buf[96];
    pixeltext::DrawShadowed(m_screen, "SHOP", panel.x + 20, panel.y + 18, 3, gold);
    pixeltext::DrawShadowed(m_screen, "ITEMS WORK IN PLAY", panel.x + 110, panel.y + 26, 1, grey);
    snprintf(buf, sizeof(buf), "GOLD %d", m_goldBank);
    pixeltext::DrawShadowed(m_screen, buf, panel.x + panel.w - pixeltext::Width(buf, 3) - 20, panel.y + 18, 3, gold);
    // the materials owned (§28 O-11): IMMORTAL drops, needed for Aghanim's Scepter and every upgrade
    const SDL_Color cyan = { 110, 210, 255, 255 };
    int mx = panel.x + 110;
    pixeltext::DrawShadowed(m_screen, "MATERIALS:", mx, panel.y + 40, 1, cyan);
    mx += pixeltext::Width("MATERIALS:", 1) + 10;
    for (int m = 0; m < practice::MATERIAL_COUNT; ++m)
    {
        RenderIconScaled(m_materialIcons[m], mx, panel.y + 34, 18);
        snprintf(buf, sizeof(buf), "%s %d", practice::MaterialName(static_cast<practice::Material>(m)), m_inventory.material[m]);
        pixeltext::DrawShadowed(m_screen, buf, mx + 22, panel.y + 40, 1, cyan);
        mx += 22 + pixeltext::Width(buf, 1) + 16;
    }

    // ---- the item cards
    for (int i = 0; i < practice::ITEM_COUNT; ++i)
    {
        practice::ItemId id = static_cast<practice::ItemId>(i);
        const practice::ItemDefinition& def = practice::GetItemDefinition(id);
        SDL_Rect r = ShopCardRect(i);
        bool selected = i == m_shopSelect;
        int level = practice::CurrentLevel(m_inventory, id);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 28, 30, 46, selected ? 250 : 200);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, selected ? 255 : 90, selected ? 210 : 95, selected ? 90 : 115, 230);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        RenderItemIcon(id, level > 0 ? level : 1, r.x + (r.w - ITEM_ICON) / 2, r.y + 6);
        const char* name = def.level[level >= 2 ? 1 : 0].name;
        pixeltext::DrawShadowed(m_screen, name, r.x + (r.w - pixeltext::Width(name, 1)) / 2, r.y + 60, 1, selected ? gold : white);
        int price = practice::NextPrice(m_inventory, id);
        SDL_Color status = grey;
        if (def.kind == practice::ItemKind::Consumable)
            snprintf(buf, sizeof(buf), "OWNED %d", m_inventory.count[i]);
        else if (level >= def.levels)
        {
            snprintf(buf, sizeof(buf), "MAX");
            status = green;
        }
        else if (level > 0)
            snprintf(buf, sizeof(buf), "LV %d  UP %d", level, price);
        else
        {
            snprintf(buf, sizeof(buf), "%d GOLD", price);
            status = price <= m_goldBank ? gold : grey;
        }
        pixeltext::DrawShadowed(m_screen, buf, r.x + (r.w - pixeltext::Width(buf, 1)) / 2, r.y + 76, 1, status);
        if (practice::IsEquipped(m_inventory, id))
            pixeltext::DrawShadowed(m_screen, "E", r.x + r.w - 12, r.y + 5, 1, green);  // equipped
        if (selected)
            RenderHighlight(r);
    }

    // ---- the selected item's details
    practice::ItemId sel = static_cast<practice::ItemId>(m_shopSelect);
    const practice::ItemDefinition& def = practice::GetItemDefinition(sel);
    int level = practice::CurrentLevel(m_inventory, sel);
    const SDL_Rect& d = SHOP_DETAIL_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 24, 26, 40, 240);
    SDL_RenderFillRect(m_screen, &d);
    SDL_SetRenderDrawColor(m_screen, 110, 115, 135, 220);
    SDL_RenderDrawRect(m_screen, &d);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    RenderItemIcon(sel, level > 0 ? level : 1, d.x + 12, d.y + 12);
    const char* kind = def.kind == practice::ItemKind::Active ? "ACTIVE" : def.kind == practice::ItemKind::Passive ? "PASSIVE" : "CONSUMABLE";
    pixeltext::DrawShadowed(m_screen, def.level[level >= 2 ? 1 : 0].name, d.x + 70, d.y + 16, 2, gold);
    pixeltext::DrawShadowed(m_screen, kind, d.x + 70, d.y + 40, 1, grey);
    int y = d.y + 76;
    for (int l = 0; l < def.levels; ++l)
    {
        const practice::ItemLevel& lv = def.level[l];
        bool owned = def.kind == practice::ItemKind::Consumable ? level > 0 : level > l;
        if (def.levels > 1)
            snprintf(buf, sizeof(buf), "LV %d  %s", l + 1, lv.name);
        else
            snprintf(buf, sizeof(buf), "%s", lv.name);
        pixeltext::DrawShadowed(m_screen, buf, d.x + 12, y, 1, owned ? green : white);
        pixeltext::DrawShadowed(m_screen, lv.effect, d.x + 12, y + 14, 1, grey);
        if (lv.cooldown > 0.0f)
            snprintf(buf, sizeof(buf), "COOLDOWN %d S   PRICE %d", static_cast<int>(lv.cooldown), lv.price);
        else
            snprintf(buf, sizeof(buf), def.kind == practice::ItemKind::Consumable ? "PRICE %d EACH" : "PRICE %d", lv.price);
        int needs = practice::MaterialForLevel(sel, l + 1);
        if (needs != practice::MATERIAL_NONE)
            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " + %s", practice::MaterialName(static_cast<practice::Material>(needs)));
        pixeltext::DrawShadowed(m_screen, buf, d.x + 12, y + 28, 1, needs != practice::MATERIAL_NONE && !owned ? cyan : grey);
        y += 56;
    }
    if (def.kind == practice::ItemKind::Consumable)
    {
        snprintf(buf, sizeof(buf), "OWNED %d / %d", m_inventory.count[m_shopSelect], practice::ITEM_MAX_STACK);
        pixeltext::DrawShadowed(m_screen, buf, d.x + 12, y, 1, white);
    }

    // the price is written on the button itself (a label under it was crossed by the button's pulsing frame)
    int price = practice::NextPrice(m_inventory, sel);
    if (price <= 0)
        snprintf(buf, sizeof(buf), def.kind == practice::ItemKind::Consumable ? "FULL" : "MAXED");
    else
        snprintf(buf, sizeof(buf), "%s %d", level > 0 && def.kind != practice::ItemKind::Consumable ? "UPGRADE" : "BUY", price);
    RenderButton(SHOP_BUY_RECT, buf, practice::CanBuy(m_inventory, sel, m_goldBank));
    bool owned = practice::Owns(m_inventory, sel);
    RenderButton(SHOP_EQUIP_RECT, !owned ? "-" : practice::IsEquipped(m_inventory, sel) ? "UNEQUIP" : "EQUIP", false);
    if (price > m_goldBank)
    {
        const SDL_Color red = { 235, 110, 110, 255 };
        snprintf(buf, sizeof(buf), "NOT ENOUGH GOLD: %d MORE NEEDED", price - m_goldBank);
        pixeltext::DrawShadowed(m_screen, buf, d.x + 12, SHOP_BUY_RECT.y - 16, 1, red);
    }
    int missing = practice::RequiredMaterial(m_inventory, sel);
    if (missing != practice::MATERIAL_NONE && m_inventory.material[missing] <= 0)
    {
        const SDL_Color red = { 235, 110, 110, 255 };
        snprintf(buf, sizeof(buf), "NEEDS %s (IMMORTAL DROP)", practice::MaterialName(static_cast<practice::Material>(missing)));
        pixeltext::DrawShadowed(m_screen, buf, d.x + 12, SHOP_BUY_RECT.y - 30, 1, red);
    }

    // ---- the loadout (2 x 3) with its keys
    pixeltext::DrawShadowed(m_screen, "LOADOUT", SHOP_LOADOUT_X, SHOP_LOADOUT_Y - 18, 2, gold);
    const char* keys[practice::ITEM_SLOTS] = { "U", "I", "O", "J", "K", "L" };
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)
    {
        SDL_Rect r = LoadoutSlotRect(s, SHOP_LOADOUT_X, SHOP_LOADOUT_Y);
        SDL_SetRenderDrawColor(m_screen, 30, 32, 46, 255);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, 110, 115, 135, 255);
        SDL_RenderDrawRect(m_screen, &r);
        int id = m_inventory.slot[s];
        if (id != practice::ITEM_NONE)
            RenderItemIcon(static_cast<practice::ItemId>(id), practice::CurrentLevel(m_inventory, static_cast<practice::ItemId>(id)), r.x + 2, r.y + 2);
        if (!m_showTouchControls)
            pixeltext::DrawShadowed(m_screen, keys[s], r.x + 3, r.y + 3, 1, white);
    }
    const char* help1 = m_showTouchControls ? "TAP AN ITEM, THEN BUY OR EQUIP." : "ARROWS: CHOOSE   ENTER: BUY   E: EQUIP";
    const char* help2 = m_showTouchControls ? "TAP A SLOT TO TAKE ITS ITEM OFF." : "IN PLAY: U I O / J K L USE THE SLOTS";
    pixeltext::DrawShadowed(m_screen, help1, SHOP_LOADOUT_X + 190, SHOP_LOADOUT_Y + 10, 1, grey);
    pixeltext::DrawShadowed(m_screen, help2, SHOP_LOADOUT_X + 190, SHOP_LOADOUT_Y + 28, 1, grey);
    pixeltext::DrawShadowed(m_screen, "NO REAL MONEY: GOLD COMES FROM ELITES AND OVERLORDS IN PLAY.", SHOP_LOADOUT_X + 190,
        SHOP_LOADOUT_Y + 60, 1, grey);
    pixeltext::DrawShadowed(m_screen, "MATERIALS DROP FROM IMMORTALS: BEAT ELITES AND OVERLORDS TO MEET ONE.", SHOP_LOADOUT_X + 190,
        SHOP_LOADOUT_Y + 76, 1, grey);
    RenderButton(SHOP_CLOSE_RECT, "CLOSE", false);
}

// While PLAY runs: the six slots on the right, each with its icon, its key, a cooldown shade with the seconds left,
// the units left of a consumable, and a glow when it was just used.
void GameManager::RenderItemBar()
{
    if (m_tutorialActive || m_bossActive || m_session.Mode() != practice::SessionMode::Play
        || m_session.State() != practice::GameState::Playing)
        return;
    const practice::Inventory& inv = m_session.Loadout();
    bool any = false;
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)
        any = any || inv.slot[s] != practice::ITEM_NONE;
    if (!any)
        return;
    const SDL_Color white = { 235, 235, 240, 255 };
    const char* keys[practice::ITEM_SLOTS] = { "U", "I", "O", "J", "K", "L" };
    char buf[16];
    for (int s = 0; s < practice::ITEM_SLOTS; ++s)
    {
        SDL_Rect r = LoadoutSlotRect(s, ItemBarX(), ItemBarY());
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 190);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, 150, 155, 175, 200);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        int index = inv.slot[s];
        if (index == practice::ITEM_NONE)
            continue;
        practice::ItemId id = static_cast<practice::ItemId>(index);
        const practice::ItemDefinition& def = practice::GetItemDefinition(id);
        RenderItemIcon(id, practice::CurrentLevel(inv, id) > 0 ? practice::CurrentLevel(inv, id) : 1, r.x + 2, r.y + 2);
        bool empty = def.kind == practice::ItemKind::Consumable && inv.count[index] <= 0;
        float cd = m_session.ItemCooldown(id), total = m_session.ItemCooldownTotal(id);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        if (empty)
        {
            SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 170);
            SDL_RenderFillRect(m_screen, &r);
        }
        else if (cd > 0.0f && total > 0.0f)  // the cooldown shade shrinks from the top
        {
            int h = static_cast<int>(r.h * cd / total);
            SDL_Rect shade = { r.x, r.y, r.w, h };
            SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 170);
            SDL_RenderFillRect(m_screen, &shade);
        }
        if (m_itemFlash[s] > 0.0f)
        {
            SDL_SetRenderDrawColor(m_screen, 255, 215, 90, static_cast<Uint8>(255 * m_itemFlash[s] / ITEM_FLASH_TIME));
            for (int k = 0; k < 3; ++k)
            {
                SDL_Rect f = { r.x - k, r.y - k, r.w + 2 * k, r.h + 2 * k };
                SDL_RenderDrawRect(m_screen, &f);
            }
        }
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        if (cd > 0.0f)
        {
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(cd) + 1);
            pixeltext::DrawShadowed(m_screen, buf, r.x + (r.w - pixeltext::Width(buf, 2)) / 2, r.y + (r.h - 14) / 2, 2, white);
        }
        if (def.kind == practice::ItemKind::Consumable)
        {
            snprintf(buf, sizeof(buf), "%d", inv.count[index]);
            pixeltext::DrawShadowed(m_screen, buf, r.x + r.w - pixeltext::Width(buf, 1) - 3, r.y + r.h - 10, 1, white);
        }
        if (!m_showTouchControls)
            pixeltext::DrawShadowed(m_screen, keys[s], r.x + 3, r.y + 3, 1, white);
    }
}

void GameManager::UseItemSlot(int slot)
{
    practice::ItemUseResult r = m_session.UseItem(slot);
    if (!r.used)
    {
        if (m_session.Loadout().slot[slot] != practice::ITEM_NONE)
            audio::Play(audio::Sfx::CastWrong);  // not ready, or nothing to do
        return;
    }
    m_itemFlash[slot] = ITEM_FLASH_TIME;
    audio::Play(audio::Sfx::Invoke);
    if (practice::GetItemDefinition(r.item).kind == practice::ItemKind::Consumable)
    {
        int i = static_cast<int>(r.item);  // a used unit is gone for good (§27 I-4)
        m_inventory.count[i] = m_session.Loadout().count[i];
        SaveInventory();
    }
    printf("[play] item used: %s\n", practice::GetItemDefinition(r.item).level[0].name);
}

void GameManager::ChooseRuneAction(int index)
{
    int goldBefore = m_session.GetStats().gold;
    practice::Rune rune = m_session.ChooseRune(index);
    if (rune == practice::Rune::None)
        return;
    int gained = m_session.GetStats().gold - goldBefore;  // Bounty (Midas included)
    if (gained > 0)
    {
        m_goldBank += gained;
        SaveGold();
    }
    switch (rune)
    {
    case practice::Rune::Regeneration: m_announce = "RUNE: REGENERATION  +1 LIFE"; break;
    case practice::Rune::Frost:        m_announce = "RUNE: FROST  ENEMIES SLOWED"; break;
    case practice::Rune::DoubleDamage: m_announce = "RUNE: DOUBLE DAMAGE  SCORE X2"; break;
    case practice::Rune::Bounty:       m_announce = "RUNE: BOUNTY  GOLD"; break;
    case practice::Rune::Shield:       m_announce = "RUNE: SHIELD  NEXT HIT BLOCKED"; break;
    default: break;
    }
    m_announceLeft = ANNOUNCE_TIME;
    AddDrop(m_runeIcons[static_cast<int>(rune)], RuneName(rune), SCREEN_WIDTH * 0.5f, 300.0f, 0);
    audio::Play(audio::Sfx::CastCorrect);
}

// Aghanim's choice (§27 I-5): the run waits; one row per rune, 1 / 2 / 3 or a tap.
void GameManager::RenderRuneChoice()
{
    int n = m_session.RuneChoiceCount();
    if (n <= 0 || m_session.State() != practice::GameState::Playing)
        return;
    DimScreen(150);
    const SDL_Rect& panel = RUNE_CHOICE_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 245);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 120, 170, 255, 220);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    const SDL_Color blue = { 150, 200, 255, 255 };
    pixeltext::DrawCentered(m_screen, "AGHANIM: CHOOSE A RUNE", SCREEN_WIDTH, panel.y + 18, 2, blue);
    char buf[64];
    for (int i = 0; i < n; ++i)
    {
        SDL_Rect row = { panel.x + 20, panel.y + 60 + i * 56, panel.w - 40, 46 };
        const char* text = "";
        switch (m_session.RuneChoice(i))
        {
        case practice::Rune::Regeneration: text = "REGENERATION  +1 LIFE"; break;
        case practice::Rune::Frost:        text = "FROST  ENEMIES SLOWED"; break;
        case practice::Rune::DoubleDamage: text = "DOUBLE DAMAGE  SCORE X2"; break;
        case practice::Rune::Bounty:       text = "BOUNTY  +GOLD"; break;
        case practice::Rune::Shield:       text = "SHIELD  NEXT HIT BLOCKED"; break;
        default: break;
        }
        if (m_showTouchControls)
            snprintf(buf, sizeof(buf), "%s", text);
        else
            snprintf(buf, sizeof(buf), "%d  %s", i + 1, text);
        RenderButton(row, buf, false);
        RenderIconScaled(m_runeIcons[static_cast<int>(m_session.RuneChoice(i))], row.x + 4, row.y + 3, 40);
    }
}

// ------------------------------------------------------------------ first-time tips (spec §31)

// Queues a tip unless it was shown before. Only inside a PLAY / SURVIVAL run: the Tutorial and Boss Fights explain
// themselves. The run waits while a tip is on screen (RunFrame), and only GOT IT / Enter closes it.
void GameManager::QueueTip(TipId tip)
{
    if (m_tutorialActive || m_bossActive || m_session.State() != practice::GameState::Playing)
        return;
    if ((m_tipsSeen & (1 << tip)) != 0 || m_tipQueued >= TIP_COUNT)
        return;
    for (int i = 0; i < m_tipQueued; ++i)
        if (m_tipQueue[i] == tip)
            return;
    m_tipQueue[m_tipQueued++] = tip;
}

void GameManager::DismissTip()
{
    if (m_tipQueued <= 0)
        return;
    m_tipsSeen |= 1 << m_tipQueue[0];
    for (int i = 1; i < m_tipQueued; ++i)
        m_tipQueue[i - 1] = m_tipQueue[i];
    --m_tipQueued;
    m_missStreak = 0;
    SaveSettings();
    audio::Play(audio::Sfx::Cast);
}

void GameManager::RenderTip()
{
    if (!TipShown())
        return;
    if (m_session.State() != practice::GameState::Playing)  // the run ended under it: nothing left to explain
    {
        m_tipQueued = 0;
        return;
    }
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    bool touch = m_showTouchControls;
    const char* title = "";
    const char* lines[4] = { "", "", "", "" };
    SDL_Texture* icon = NULL;
    char runeLine[48] = "";
    switch (m_tipQueue[0])
    {
    case TIP_ELITE:
        title = "ELITE";
        lines[0] = "IT NEEDS 2 SPELLS, IN ORDER.";
        lines[1] = "IF IT REACHES YOU: -1 LIFE PER";
        lines[2] = "SPELL LEFT. BEAT IT: 3 POINTS, 5 GOLD.";
        lines[3] = "BEATING IT RAISES THE IMMORTAL %.";
        break;
    case TIP_OVERLORD:
        title = "OVERLORD";
        lines[0] = "IT NEEDS 3 SPELLS, IN ORDER.";
        lines[1] = "IF IT REACHES YOU: -1 LIFE PER";
        lines[2] = "SPELL LEFT. BEAT IT: RUNE, NEW STAGE.";
        lines[3] = "BEATING IT RAISES THE IMMORTAL %.";
        break;
    case TIP_RUNE:
        title = "RUNE";
        icon = m_runeIcons[static_cast<int>(m_tipRune)];
        lines[0] = "EVERY OVERLORD LEAVES A RUNE:";
        lines[1] = "A SHORT BONUS FOR THIS RUN.";
        switch (m_tipRune)
        {
        case practice::Rune::Regeneration: lines[2] = "REGENERATION: +1 LIFE."; break;
        case practice::Rune::Frost:        lines[2] = "FROST: ENEMIES ARE SLOWER."; break;
        case practice::Rune::DoubleDamage: lines[2] = "DOUBLE DAMAGE: POINTS X2."; break;
        case practice::Rune::Bounty:       lines[2] = "BOUNTY: EXTRA GOLD."; break;
        case practice::Rune::Shield:       lines[2] = "SHIELD: BLOCKS THE NEXT HIT."; break;
        default:                           lines[2] = "YOU CHOOSE WHICH ONE."; break;
        }
        break;
    case TIP_GOLD:
        title = "GOLD";
        lines[0] = "ELITES AND OVERLORDS GIVE GOLD.";
        lines[1] = "IT IS KEPT AFTER THE RUN.";
        lines[2] = "SPEND IT IN THE SHOP (MENU).";
        break;
    case TIP_ITEMS:
        title = "ITEMS";
        for (int s = 0; s < practice::ITEM_SLOTS && icon == NULL; ++s)
            if (m_inventory.slot[s] != practice::ITEM_NONE)
                icon = m_itemIcons[m_inventory.slot[s]][0];
        lines[0] = "YOUR ITEMS ARE IN THE SLOTS.";
        lines[1] = touch ? "TAP A SLOT TO USE ITS ITEM." : "KEYS U I O / J K L USE THEM.";
        lines[2] = "A NUMBER ON A SLOT: SECONDS TO WAIT.";
        break;
    case TIP_IMMORTAL:
        title = "IMMORTAL";
        lines[0] = "IT HAS A LIFE BAR. ONLY ITS COMBO";
        lines[1] = "HURTS IT: CAST THE SPELLS AT THE";
        lines[2] = "TOP RIGHT IN ORDER, WELL TIMED.";
        lines[3] = "PRACTISE IN IMMORTALS (MENU).";
        break;
    case TIP_MATERIAL:
        title = "MATERIAL";
        icon = m_materialIcons[static_cast<int>(m_tipMaterial)];
        snprintf(runeLine, sizeof(runeLine), "YOU FOUND A %s.", practice::MaterialName(m_tipMaterial));
        lines[0] = runeLine;
        lines[1] = "ONLY IMMORTALS DROP MATERIALS.";
        lines[2] = "THE SHOP NEEDS ONE FOR AGHANIM'S";
        lines[3] = "SCEPTER AND FOR ITEM UPGRADES.";
        break;
    default:
        title = "NEED HELP?";
        lines[0] = "RECIPE HINT SHOWS THE KEYS OF THE";
        lines[1] = "TARGET SPELL. TURN IT ON WITH THE";
        lines[2] = touch ? "HINT BUTTON, TOP LEFT." : "HINT BUTTON, TOP LEFT (OR KEY G).";
        break;
    }

    DimScreen(150);
    const SDL_Rect& panel = TIP_PANEL_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 245);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 220);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    int textX = panel.x + 28;
    if (icon != NULL)
    {
        RenderIconScaled(icon, panel.x + 24, panel.y + 22, 64);
        textX = panel.x + 106;
    }
    pixeltext::DrawShadowed(m_screen, title, textX, panel.y + 20, 3, gold);
    for (int i = 0; i < 4; ++i)
        pixeltext::DrawShadowed(m_screen, lines[i], textX, panel.y + 58 + i * 24, 2, white);
    RenderButton(TIP_BUTTON_RECT, touch ? "GOT IT" : "ENTER  GOT IT", true);
}

// ------------------------------------------------------------------ GUIDE (spec §31)

SDL_Rect GameManager::GuideTabRect(int tab) const
{
    SDL_Rect r = { GUIDE_PANEL_RECT.x + 24 + tab * 168, GUIDE_PANEL_RECT.y + 50, 160, 32 };
    return r;
}

void GameManager::HandleGuideKey(SDL_Keycode sym)
{
    if (sym == SDLK_LEFT || sym == SDLK_UP)
        m_guideTab = (m_guideTab + GUIDE_TABS - 1) % GUIDE_TABS;
    else if (sym == SDLK_RIGHT || sym == SDLK_DOWN || sym == SDLK_TAB || sym == SDLK_SPACE)
        m_guideTab = (m_guideTab + 1) % GUIDE_TABS;
    else if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK || sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
        m_showGuide = false;
    else if (sym == SDLK_m)
        ToggleMute();
}

void GameManager::HandleGuidePointer(int x, int y)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    for (int t = 0; t < GUIDE_TABS; ++t)
    {
        if (hit(GuideTabRect(t)))
        {
            m_guideTab = t;
            return;
        }
    }
    if (hit(GUIDE_CLOSE_RECT) || !hit(GUIDE_PANEL_RECT))
        m_showGuide = false;
}

// What the Tutorial does not teach, one tab per subject, every line short. The numbers come from the rules.
void GameManager::RenderGuide()
{
    DimScreen(200);
    const SDL_Rect& panel = GUIDE_PANEL_RECT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 245);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 200);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    const SDL_Color grey = { 150, 155, 170, 255 };
    const SDL_Color orange = { 255, 170, 70, 255 };
    const SDL_Color red = { 255, 110, 110, 255 };
    char buf[128];
    pixeltext::DrawShadowed(m_screen, "GUIDE", panel.x + 24, panel.y + 14, 3, gold);
    const char* tabs[GUIDE_TABS] = { "BASICS", "ENEMIES", "RUNES", "ITEMS", "IMMORTALS" };
    for (int t = 0; t < GUIDE_TABS; ++t)
    {
        SDL_Rect r = GuideTabRect(t);
        bool selected = t == m_guideTab;
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, selected ? 70 : 28, selected ? 44 : 30, selected ? 16 : 46, 240);
        SDL_RenderFillRect(m_screen, &r);
        SDL_SetRenderDrawColor(m_screen, selected ? 255 : 110, selected ? 210 : 115, selected ? 90 : 135, 230);
        SDL_RenderDrawRect(m_screen, &r);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        pixeltext::DrawShadowed(m_screen, tabs[t], r.x + (r.w - pixeltext::Width(tabs[t], 2)) / 2, r.y + 9, 2, selected ? gold : white);
    }
    const int left = panel.x + 24;
    const int top = panel.y + 100;

    if (m_guideTab == 0)
    {
        const char* lines[] = {
        "Q, W, E LOAD ORBS. THE THREE ORBS YOU HOLD MAKE A SPELL.",
        "ONLY THE MIX COUNTS, NOT THE ORDER: Q Q W IS THE SAME AS W Q Q.",
        "R INVOKES THAT SPELL INTO D. THE SPELL THAT WAS IN D MOVES TO F.",
        "D AND F CAST. A SPELL STAYS IN ITS SLOT: CAST IT AS OFTEN AS YOU LIKE.",
        "EVERY ENEMY NEEDS ONE SPELL. ITS NAME IS AT THE TOP RIGHT.",
        "A WRONG SPELL DOES NOTHING. AN ENEMY THAT REACHES YOU COSTS A LIFE.",
        "RECIPES (MENU) LISTS THE TEN SPELLS. RECIPE HINT SHOWS THE KEYS.",
        "PLAY IS THE MAIN GAME. SURVIVAL: ONE SPELL PER ENEMY, WITHOUT END."
        };
        for (int i = 0; i < static_cast<int>(sizeof(lines) / sizeof(lines[0])); ++i)
            pixeltext::DrawShadowed(m_screen, lines[i], left, top + 8 + i * 40, 2, white);
    }
    else if (m_guideTab == 1)
    {
        // one row per kind of enemy; the columns are filled from the PLAY rules
        const int columns[5] = { left, left + 240, left + 420, left + 650, left + 760 };
        const char* heads[5] = { "ENEMY", "SPELLS", "IF IT REACHES YOU", "POINTS", "GOLD" };
        for (int c = 0; c < 5; ++c)
            pixeltext::DrawShadowed(m_screen, heads[c], columns[c], top, 1, grey);
        struct Row { const char* name; SDL_Color color; const char* spells; int lives; int points; int gold; const char* note; };
        const Row rows[4] = {
            { "NORMAL", white, "1", practice::PLAY_LEAK_NORMAL, practice::PLAY_POINTS_NORMAL, 0,
                "MOST ENEMIES." },
            { "ELITE", orange, "2 IN ORDER", practice::PLAY_LEAK_ELITE, practice::PLAY_POINTS_ELITE, practice::PLAY_GOLD_ELITE,
                "THE 10TH, 15TH, 25TH, 35TH... ENEMY. LARGER AND A LITTLE SLOWER." },
            { "OVERLORD", red, "3 IN ORDER", practice::PLAY_LEAK_OVERLORD, practice::PLAY_POINTS_OVERLORD, practice::PLAY_GOLD_OVERLORD,
                "THE 20TH, 30TH, 40TH... ENEMY. LEAVES A RUNE, THEN THE NEXT STAGE BEGINS." },
            { "IMMORTAL", gold, "ITS COMBO", 1, practice::PLAY_IMMORTAL_POINTS, practice::PLAY_IMMORTAL_GOLD,
                "COMES BY CHANCE: +10 % PER ELITE BEATEN, +20 % PER OVERLORD. A LIFE BAR, A COMBO OF 4 TO 8. DROPS MATERIALS." },
        };
        for (int i = 0; i < 4; ++i)
        {
            int y = top + 22 + i * 52;
            pixeltext::DrawShadowed(m_screen, rows[i].name, columns[0], y, 2, rows[i].color);
            pixeltext::DrawShadowed(m_screen, rows[i].spells, columns[1], y, 2, white);
            snprintf(buf, sizeof(buf), rows[i].lives == 1 ? "-%d LIFE" : "-%d LIVES", rows[i].lives);
            pixeltext::DrawShadowed(m_screen, buf, columns[2], y, 2, white);
            snprintf(buf, sizeof(buf), "%d", rows[i].points);
            pixeltext::DrawShadowed(m_screen, buf, columns[3], y, 2, white);
            if (rows[i].gold > 0)
                snprintf(buf, sizeof(buf), "%d", rows[i].gold);
            else
                snprintf(buf, sizeof(buf), "-");
            pixeltext::DrawShadowed(m_screen, buf, columns[4], y, 2, gold);
            pixeltext::DrawShadowed(m_screen, rows[i].note, columns[0], y + 20, 1, grey);
        }
        // at this size a line holds 69 characters inside the panel (1.9.5 had one of 76 that ran over the frame)
        const char* notes[4] = {
            "A WRONG SPELL NEVER HURTS YOU: TRY AGAIN. A CHAIN KEEPS ITS PROGRESS.",
            "A CHAIN ENEMY THAT REACHES YOU COSTS 1 LIFE PER SPELL STILL ON IT.",
            "GOLD: FROM ELITES, OVERLORDS, IMMORTALS AND THE BOUNTY RUNE.",
            "IN SURVIVAL EVERY ENEMY IS A NORMAL ONE AND COSTS 1 LIFE.",
        };
        for (int i = 0; i < 4; ++i)
            pixeltext::DrawShadowed(m_screen, notes[i], left, top + 244 + i * 28, 2, white);
    }
    else if (m_guideTab == 2)
    {
        const practice::Rune runes[5] = { practice::Rune::Regeneration, practice::Rune::Frost, practice::Rune::DoubleDamage,
            practice::Rune::Bounty, practice::Rune::Shield };
        for (int i = 0; i < 5; ++i)
        {
            int y = top + i * 56;
            RenderIconScaled(m_runeIcons[static_cast<int>(runes[i])], left, y, 44);
            pixeltext::DrawShadowed(m_screen, RuneName(runes[i]), left + 60, y + 15, 2, gold);
            switch (runes[i])
            {
            case practice::Rune::Regeneration: snprintf(buf, sizeof(buf), "+1 LIFE (%d AT MOST)", practice::PLAY_MAX_HP); break;
            case practice::Rune::Frost:
                snprintf(buf, sizeof(buf), "ENEMIES AT %d %% SPEED FOR %d S", static_cast<int>(practice::PLAY_FROST_SPEED * 100.0f + 0.5f),
                    static_cast<int>(practice::PLAY_FROST_TIME));
                break;
            case practice::Rune::DoubleDamage: snprintf(buf, sizeof(buf), "POINTS X2 FOR %d S", static_cast<int>(practice::PLAY_DOUBLE_TIME)); break;
            case practice::Rune::Bounty:       snprintf(buf, sizeof(buf), "+%d GOLD", practice::PLAY_BOUNTY_GOLD); break;
            default:                           snprintf(buf, sizeof(buf), "THE NEXT HIT COSTS NO LIFE"); break;
            }
            pixeltext::DrawShadowed(m_screen, buf, left + 290, y + 15, 2, white);
        }
        pixeltext::DrawShadowed(m_screen, "AN OVERLORD OR AN IMMORTAL LEAVES ONE AT RANDOM.", left, top + 296, 2, white);
        pixeltext::DrawShadowed(m_screen, "WITH AGHANIM'S SCEPTER YOU CHOOSE 1 OF 2 (BLESSING: 1 OF 3).", left, top + 328, 2, white);
    }
    else if (m_guideTab == 3)
    {
        // two columns; the texts are the shop's own (Practice/Items)
        for (int i = 0; i < practice::ITEM_COUNT; ++i)
        {
            const practice::ItemDefinition& def = practice::GetItemDefinition(i);
            int x = left + (i / 6) * 424;
            int y = top - 4 + (i % 6) * 60;
            RenderIconScaled(m_itemIcons[i][0], x, y, 40);
            pixeltext::DrawShadowed(m_screen, def.level[0].name, x + 50, y, 2, gold);
            if (def.level[0].cooldown > 0.0f)
                snprintf(buf, sizeof(buf), "%s  -  COOLDOWN %d S", def.level[0].effect, static_cast<int>(def.level[0].cooldown));
            else
                snprintf(buf, sizeof(buf), "%s%s", def.level[0].effect,
                    def.kind == practice::ItemKind::Consumable ? "  -  USED UP" : "  -  ALWAYS ON");
            pixeltext::DrawShadowed(m_screen, buf, x + 50, y + 19, 1, white);
            if (def.levels > 1)
            {
                snprintf(buf, sizeof(buf), "LV 2 %s: %s", def.level[1].name, def.level[1].effect);
                pixeltext::DrawShadowed(m_screen, buf, x + 50, y + 31, 1, grey);
            }
        }
        pixeltext::DrawShadowed(m_screen, "ITEMS WORK IN PLAY. BUY THEM IN THE SHOP WITH GOLD. UPGRADES ALSO NEED A MATERIAL.", left, top + 352, 1, grey);
        pixeltext::DrawShadowed(m_screen, "AGAINST AN IMMORTAL, REFRESHER ORB DOUBLES THE DAMAGE OF YOUR NEXT COMBO INSTEAD.", left, top + 364, 1, grey);
    }
    else
    {
        char lift[96], delays[96];
        snprintf(lift, sizeof(lift), "TORNADO LIFTS IT FOR %.1f S. LAND THE NEXT SPELLS AS IT COMES DOWN:", practice::BOSS_LIFT_TIME);
        snprintf(delays, sizeof(delays), "SUN STRIKE HITS %.1f S AFTER THE CAST, CHAOS METEOR %.1f S, EMP %.1f S.",
            practice::BOSS_SUN_STRIKE_DELAY, practice::BOSS_METEOR_DELAY, practice::BOSS_EMP_DELAY);
        const char* lines[] = {
        "AN IMMORTAL IS HURT ONLY BY ITS COMBO: THE SPELLS AT THE TOP RIGHT.",
        "",
        "",
        "GRADES: PERFECT 100 %, GREAT 60 %, GOOD 35 % OF ITS LIFE, AVERAGED.",
        "EVERY OTHER STEP IS A QUICK ONE: CAST IT RIGHT AFTER THE LAST SPELL.",
        "COLD SNAP FREEZES IT, ICE WALL SLOWS IT, GHOST WALK MAKES IT LOSE YOU.",
        "WITH RECIPE HINT ON, A BAR UNDER EACH SPELL SHOWS WHEN TO CAST IT.",
        "FIVE IMMORTALS, COMBOS OF 4 TO 8 SPELLS. PRACTISE THEM IN THE MENU."
        };
        for (int i = 0; i < static_cast<int>(sizeof(lines) / sizeof(lines[0])); ++i)
        {
            const char* text = i == 1 ? lift : i == 2 ? delays : lines[i];
            pixeltext::DrawShadowed(m_screen, text, left, top + 8 + i * 40, 2, white);
        }
    }

    const char* help = m_showTouchControls ? "TAP A TAB." : "ARROWS: NEXT TAB    ESC: CLOSE";
    pixeltext::DrawShadowed(m_screen, help, left, GUIDE_CLOSE_RECT.y + 14, 1, grey);
    RenderButton(GUIDE_CLOSE_RECT, "CLOSE", false);
}

// ------------------------------------------------------------------ pause (owner 2026-10-01)

// Esc / Back / Enter resumes; Q leaves the run for the menu (a boss fight for the boss list).
void GameManager::HandlePauseKey(SDL_Keycode sym, bool& quit)
{
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK || sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_SPACE)
        m_paused = false;
    else if (sym == SDLK_q)
        QuitFromPause(quit);
    else if (sym == SDLK_m)
        ToggleMute();
    else if (sym == SDLK_g)
        ToggleRecipeHint();
}

void GameManager::HandlePausePointer(int x, int y, bool& quit)
{
    auto hit = [x, y](const SDL_Rect& r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    if (hit(PAUSE_RESUME_RECT))
        m_paused = false;
    else if (hit(PAUSE_QUIT_RECT))
        QuitFromPause(quit);
    else if (hit(SOUND_BUTTON_RECT))
        ToggleMute();
    else if (hit(HINT_BUTTON_RECT))
        ToggleRecipeHint();
}

void GameManager::QuitFromPause(bool& quit)
{
    m_paused = false;
    if (m_bossActive)
        ExitBoss();
    else
        PressEscapeAction(quit);  // Playing -> Ready, with the records of the run so far
}

void GameManager::RenderPause()
{
    if (!m_paused)
        return;
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 190, 195, 210, 255 };
    bool touch = m_showTouchControls;
    DimScreen(175);
    pixeltext::DrawCentered(m_screen, "PAUSED", SCREEN_WIDTH, 150, 6, gold);
    RenderButton(PAUSE_RESUME_RECT, touch ? "RESUME" : "ESC  RESUME", true);
    RenderButton(PAUSE_QUIT_RECT, m_bossActive ? (touch ? "IMMORTALS" : "Q  IMMORTALS") : (touch ? "QUIT TO MENU" : "Q  QUIT TO MENU"), false);
    pixeltext::DrawCentered(m_screen, m_bossActive ? "THE FIGHT WAITS FOR YOU." : "THE RUN WAITS FOR YOU.", SCREEN_WIDTH, 352, 1, grey);
}

// ------------------------------------------------------------------ result screens

// The title, an optional line under it, and the empty panel; ResultRow / ResultSeparator fill it from the top.
void GameManager::ResultBegin(const char* title, SDL_Color titleColor, const char* subtitle, SDL_Color subtitleColor,
    int rows, int separators, int extraHeight)
{
    DimScreen(185);
    pixeltext::DrawCentered(m_screen, title, SCREEN_WIDTH, 22, 6, titleColor);
    if (subtitle != NULL)
        pixeltext::DrawCentered(m_screen, subtitle, SCREEN_WIDTH, 73, 2, subtitleColor);
    SDL_Rect panel = { RESULT_PANEL_X, RESULT_PANEL_Y, RESULT_PANEL_W, 24 + rows * RESULT_ROW_H + separators * 12 + extraHeight };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 16, 18, 28, 235);
    SDL_RenderFillRect(m_screen, &panel);
    SDL_SetRenderDrawColor(m_screen, 110, 115, 135, 220);
    SDL_RenderDrawRect(m_screen, &panel);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    m_resultY = panel.y + 16;
}

// One line of the panel: the label on the left, its value in the value column, an optional gold note after it.
void GameManager::ResultRow(const char* label, const char* value, const char* note, int style)
{
    const SDL_Color labelColor = { 165, 172, 192, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    pixeltext::DrawShadowed(m_screen, label, RESULT_PANEL_X + 24, m_resultY, 2, labelColor);
    int x = RESULT_VALUE_X;
    if (value != NULL)
    {
        pixeltext::DrawShadowed(m_screen, value, x, m_resultY, 2, style == 1 ? gold : style == 2 ? grey : white);
        x += pixeltext::Width(value, 2) + 18;
    }
    if (note != NULL)
        pixeltext::DrawShadowed(m_screen, note, x, m_resultY, 2, gold);
    m_resultY += RESULT_ROW_H;
}

void GameManager::ResultSeparator()
{
    SDL_SetRenderDrawColor(m_screen, 70, 75, 95, 255);
    SDL_RenderDrawLine(m_screen, RESULT_PANEL_X + 24, m_resultY, RESULT_PANEL_X + RESULT_PANEL_W - 24, m_resultY);
    m_resultY += 12;
}

// The commands, always as buttons: the main one (pulsing) and the way back; under them the two look-ups.
void GameManager::ResultButtons(const char* again, const char* back, bool extras)
{
    char buf[48];
    bool touch = m_showTouchControls;
    snprintf(buf, sizeof(buf), touch ? "%s" : "ENTER  %s", again);
    RenderButton(TOUCH_GAMEOVER_RESTART_RECT, buf, true);
    snprintf(buf, sizeof(buf), touch ? "%s" : "ESC  %s", back);
    RenderButton(TOUCH_GAMEOVER_MENU_RECT, buf, false);
    if (extras)
    {
        RenderButton(RECIPES_BUTTON_GAMEOVER_RECT, touch ? "RECIPES" : "H  RECIPES", false);
        RenderButton(LEADERBOARD_BUTTON_GAMEOVER_RECT, touch ? "LEADERBOARD" : "L  LEADERBOARD", false);
    }
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
    m_paused = false;
    if (m_recipeHint)
        m_boss.MarkAssisted();
    audio::Play(audio::Sfx::Start);
    printf("[boss] fight against %s\n", m_boss.Def().name);
}

// Leaves the fight (or its result screen) for the boss list; Esc there goes on to the menu.
void GameManager::ExitBoss()
{
    m_bossActive = false;
    m_paused = false;
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
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK)
    {
        if (m_boss.State() == practice::BossState::Fighting)
            m_paused = true;  // the pause offers the boss list
        else
            ExitBoss();
        return;
    }
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
    if (hit(TOUCH_PLAYING_MENU_RECT))  // "PAUSE"
    {
        m_paused = true;
        return true;
    }
    for (int i = 0; i < 6 && m_showTouchControls; ++i)
    {
        if (hit(TouchButtonRect(i)))
        {
            ProcessBossAction(kTouchActions[i]);
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
    PresentBossInput(r);
}

void GameManager::PresentBossInput(const practice::BossInputResult& r)
{
    if (r.attemptStarted)
        printf("[boss] combo attempt started\n");
    if (r.grade == practice::HitGrade::Miss)  // a quick step cast too late (B-16)
        OnBossFail(practice::ComboFail::TooLate);
    else if (r.grade != practice::HitGrade::None)
        OnBossGrade(r.grade);
    if (r.fail != practice::ComboFail::None)
        OnBossFail(r.fail);
}

void GameManager::PresentBossUpdate(const practice::BossUpdateResult& r, bool immortal)
{
    const SDL_Color gold = { 255, 215, 80, 255 };
    if (r.lifted)
        audio::Play(audio::Sfx::Cast);
    bool missShown = false;
    for (int i = 0; i < r.impactCount; ++i)
    {
        const practice::BossImpact& impact = r.impacts[i];
        practice::Bounds body = BossView().Body();
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
            OnBossGrade(impact.grade);
    }
    if (r.quickMissed)  // a quick step ran out of time: only its share is lost (B-16)
    {
        OnBossFail(practice::ComboFail::TooLate);
        missShown = true;
    }
    if (r.phaseChanged)  // B-19
    {
        BossText("PHASE 2!", gold, 84);
        audio::Play(audio::Sfx::Start);
        printf("[boss] phase 2\n");
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
        printf("[boss] combo: -%d%%, boss HP %d%%\n", r.damage, BossView().BossHp());
    }
    if (immortal)  // the run handles the contact (lives, Shield), the reward and the Game Over itself
        return;
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
        if (m_bossBest[i] <= 0.0f || time < m_bossBest[i])
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

void GameManager::OnBossGrade(practice::HitGrade grade)
{
    m_bossHitLeft = IMMORTAL_HIT_TIME;  // it flinches (the Immortals with a "got hit" sheet)
    const SDL_Color gold = { 255, 215, 80, 255 };
    const SDL_Color cyan = { 110, 210, 255, 255 };
    const SDL_Color white = { 235, 235, 240, 255 };
    bool perfect = grade == practice::HitGrade::Perfect;
    BossText(perfect ? "PERFECT!" : grade == practice::HitGrade::Great ? "GREAT" : "GOOD",
        perfect ? gold : grade == practice::HitGrade::Great ? cyan : white);
    practice::Bounds body = BossView().Body();
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

// Best fight time per Immortal, saved like the records: immortals.txt (milliseconds, one per Immortal) next to the
// exe / in the app folder, localStorage on the web. Missing or unreadable = not beaten yet. (bosses.txt held the
// times of the eight bosses of 1.2 - 1.8, which are gone: it is not read any more.)
void GameManager::LoadBossTimes()
{
    int raw[practice::BOSS_COUNT] = {};
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            var values = String(localStorage.getItem('threeElements_immortalTimes')).split(',').map(Number);
            for (var i = 0; i < $1; ++i)
                HEAP32[($0 >> 2) + i] = values[i] | 0;
        } catch (e) {}
    }, raw, practice::BOSS_COUNT);
#else
    FILE* f = fopen(SavePath("immortals.txt").c_str(), "r");
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
            localStorage.setItem('threeElements_immortalTimes', values.join(','));
        } catch (e) {}
    }, raw, practice::BOSS_COUNT);
#else
    FILE* f = fopen(SavePath("immortals.txt").c_str(), "w");
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
    const practice::BossSession& boss = BossView();
    practice::Bounds b = boss.Body();
    if (boss.Phase() == practice::BossPhase::Airborne)
        b.y -= practice::BossLiftHeight(boss.AirTime());
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
    if (!BossShown())
        return;
    const practice::BossSession& boss = BossView();
    const practice::BossDefinition& def = boss.Def();
    const practice::EnemyDefinition& e = practice::GetEnemyDefinition(def.enemyDefinition);
    EnemyObject& sprite = m_enemySprites[def.enemyDefinition];
    bool airborne = boss.Phase() == practice::BossPhase::Airborne;
    float lift = airborne ? practice::BossLiftHeight(boss.AirTime()) : 0.0f;
    practice::Bounds body = boss.Body();
    const int ground = static_cast<int>(practice::GROUND_LINE_Y);
    int cx = static_cast<int>(body.x + body.w * 0.5f);

    float shrink = 1.0f - 0.5f * lift / practice::BOSS_LIFT_HEIGHT;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 110);
    FillEllipse(m_screen, cx, ground, static_cast<int>(body.w * 0.55f * shrink), static_cast<int>(8 * shrink));
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    if (airborne && m_tornadoSheet != NULL)
    {
        int frame = practice::TornadoFrame(boss.AirTime());
        SDL_Rect src = { (frame % TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE, (frame / TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE,
            TORNADO_FRAME_SIZE, TORNADO_FRAME_SIZE };
        const int size = 112;
        SDL_Rect dst = { cx - size / 2, ground - static_cast<int>(lift) - size / 2 + 24, size, size };
        SDL_RenderCopy(m_screen, m_tornadoSheet, &src, &dst);
    }

    bool flash = m_enemyFlashLeft > 0.0f;
    if (!RenderImmortalArt(boss, cx, lift, flash))
    {
        if (sprite.p_object_ == NULL)
            return;
        int x = static_cast<int>(body.x - e.bodyLeft * def.scale);
        int y = static_cast<int>(practice::GROUND_LINE_Y - e.feetRow * def.scale - lift);
        // B-17 holds: frozen = ice blue, slowed by the Ice Wall = pale blue, confused by Ghost Walk = a "?" above it
        if (flash)
            SDL_SetTextureColorMod(sprite.p_object_, 255, 90, 90);
        else if (boss.FreezeLeft() > 0.0f)
            SDL_SetTextureColorMod(sprite.p_object_, 120, 190, 255);
        else if (boss.SlowLeft() > 0.0f)
            SDL_SetTextureColorMod(sprite.p_object_, 190, 220, 255);
        else
            SDL_SetTextureColorMod(sprite.p_object_, def.tint[0], def.tint[1], def.tint[2]);
        sprite.RenderFrameScaled(m_screen, x, y, def.scale);
        SDL_SetTextureColorMod(sprite.p_object_, 255, 255, 255);
    }
    const char* hold = boss.FreezeLeft() > 0.0f ? "FROZEN" : boss.ConfuseLeft() > 0.0f ? "?" : boss.SlowLeft() > 0.0f ? "SLOWED" : NULL;
    if (hold != NULL && boss.State() == practice::BossState::Fighting)
    {
        const SDL_Color ice = { 170, 225, 255, 255 };
        int scale = hold[0] == '?' ? 4 : 2;
        pixeltext::DrawShadowed(m_screen, hold, cx - pixeltext::Width(hold, scale) / 2,
            static_cast<int>(body.y - lift) - 12 - 7 * scale, scale, ice);
    }
}

// An Immortal with its own art (§32 M-6): it runs while it walks; it stands (the first frame of its "got hit"
// sheet) while it is in the air, frozen, confused, pushed back or waiting for the fight; and it flinches (that
// sheet, once) when a spell of the combo scores. The frames are drawn 1:1, centred on the body, feet on the ground.
bool GameManager::RenderImmortalArt(const practice::BossSession& boss, int cx, float lift, bool flash)
{
    int art = boss.BossIndex();
    if (art < 0 || art >= IMMORTAL_ART_COUNT || m_immortalRun[art] == NULL)
        return false;
    SDL_Texture* run = m_immortalRun[art];
    SDL_Texture* hit = m_immortalHit[art];
    bool fighting = boss.State() == practice::BossState::Fighting && (m_bossActive || m_session.ImmortalActive());
    bool standing = !fighting || boss.Phase() != practice::BossPhase::Walking || boss.FreezeLeft() > 0.0f
        || boss.ConfuseLeft() > 0.0f;
    SDL_Texture* sheet = run;
    int frame = static_cast<int>(m_bossAnimTime * IMMORTAL_RUN_FPS[art]) % IMMORTAL_FRAMES;
    if (hit != NULL && m_bossHitLeft > 0.0f)
    {
        sheet = hit;
        frame = static_cast<int>((1.0f - m_bossHitLeft / IMMORTAL_HIT_TIME) * IMMORTAL_FRAMES);
        frame = frame < 0 ? 0 : (frame > IMMORTAL_FRAMES - 1 ? IMMORTAL_FRAMES - 1 : frame);
    }
    else if (hit != NULL && standing)
    {
        sheet = hit;
        frame = 0;
    }
    int w = 0, h = 0;
    SDL_QueryTexture(sheet, NULL, NULL, &w, &h);
    int cellW = w / 4, cellH = h / 4;
    SDL_Rect src = { (frame % 4) * cellW, (frame / 4) * cellH, cellW, cellH };
    SDL_Rect dst = { cx - cellW / 2, static_cast<int>(practice::GROUND_LINE_Y - lift) - cellH + IMMORTAL_FEET[art], cellW, cellH };
    if (flash)
        SDL_SetTextureColorMod(sheet, 255, 90, 90);
    else if (boss.FreezeLeft() > 0.0f)
        SDL_SetTextureColorMod(sheet, 120, 190, 255);
    else if (boss.SlowLeft() > 0.0f)
        SDL_SetTextureColorMod(sheet, 190, 220, 255);
    SDL_RenderCopy(m_screen, sheet, &src, &dst);
    SDL_SetTextureColorMod(sheet, 255, 255, 255);
    return true;
}

// Where each delayed spell will land: a faint circle of its reach and a ring closing in on the impact point.
void GameManager::RenderBossImpacts()
{
    if (!(m_bossActive || ImmortalFight()))
        return;
    const practice::BossSession& boss = BossView();
    const int ground = static_cast<int>(practice::GROUND_LINE_Y);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < boss.PendingCount(); ++i)
    {
        const practice::PendingSpell& p = boss.GetPending(i);
        SDL_Color c = p.skill == invoker::SkillId::SunStrike ? SDL_Color{ 255, 215, 80, 255 }
            : p.skill == invoker::SkillId::ChaosMeteor ? SDL_Color{ 255, 120, 30, 255 } : SDL_Color{ 190, 100, 255, 255 };
        float radius = practice::BossSpellRadius(p.skill);
        float progress = p.delay > 0.0f ? 1.0f - p.left / p.delay : 1.0f;  // 0 at the cast, 1 at the impact
        int cx = static_cast<int>(p.x);
        if (p.skill == invoker::SkillId::ChaosMeteor && m_meteorSheet != NULL && p.left < METEOR_FALL_TIME)
        {
            practice::Bounds body = boss.Body();  // the rock is seen falling onto its impact point
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
    {
        const char* pause = m_showTouchControls ? "PAUSE" : "ESC  PAUSE";
        pixeltext::DrawShadowed(m_screen, pause, SCREEN_WIDTH - pixeltext::Width(pause, 2) - 16, 42, 2, grey);
    }
}

// The combo at the top right: one icon tile per spell in order (done = green, cast and on its way = blue,
// next = pulsing gold), the next spell's name above, the recipe orbs under each tile with the RECIPE HINT on.
// In the middle: boss 1's lesson line and its CAST NOW cue, or why the last attempt failed.
void GameManager::RenderBossCombo()
{
    if (!BossShown() || BossView().State() != practice::BossState::Fighting)
        return;
    const practice::BossSession& boss = BossView();
    bool warning = ImmortalShown() && !m_session.ImmortalActive();  // the banner has the middle of the screen
    const practice::BossDefinition& def = boss.Def();
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 190, 190, 200, 255 };
    const SDL_Color red = { 255, 110, 110, 255 };
    char buf[64];

    const invoker::SkillId* combo = boss.Combo();  // the second phase's once the boss is at 50 % (B-19)
    int n = boss.ComboLength();
    // five spells fit with the large tiles; six to eight get smaller ones without the ">" (§32 M-6)
    const bool small = n > 5;
    const int TILE = small ? BOSS_COMBO_TILE_SMALL : BOSS_COMBO_TILE;
    const int GAP = small ? BOSS_COMBO_GAP_SMALL : BOSS_COMBO_GAP;
    int width = n * TILE + (n - 1) * GAP;
    int x0 = SCREEN_WIDTH - 16 - width;
    // the lines in the middle of the screen stay clear of a long strip
    auto drawMiddle = [this, x0](const char* text, int y, int scale, SDL_Color color)
    {
        int w = pixeltext::Width(text, scale);
        int x = (SCREEN_WIDTH - w) / 2;
        if (x + w > x0 - 14)
            x = x0 - 14 - w;
        pixeltext::DrawShadowed(m_screen, text, x < 8 ? 8 : x, y, scale, color);
    };
    bool running = boss.AttemptRunning();
    int next = running ? boss.CastSteps() : 0;
    int nowStep = -1;  // hint: the follow-up whose timing bar says NOW

    if (running && next >= n)
        snprintf(buf, sizeof(buf), "COMBO CAST - WAIT FOR IT");
    else
        snprintf(buf, sizeof(buf), "NEXT: %s", invoker::GetSkillDefinition(combo[next]).name);
    pixeltext::DrawShadowed(m_screen, buf, SCREEN_WIDTH - 16 - pixeltext::Width(buf, 2), 64, 2, gold);

    for (int i = 0; i < n; ++i)
    {
        SDL_Rect tile = { x0 + i * (TILE + GAP), BOSS_COMBO_Y, TILE, TILE };
        bool done = running && boss.StepDone(i);
        bool cast = running && i < next;
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 190);
        SDL_RenderFillRect(m_screen, &tile);
        SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
        m_skillIcons[static_cast<int>(combo[i])].RenderAt(m_screen, tile.x + 2, tile.y + 2, TILE - 4);
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
        if (i + 1 < n && !small)
            pixeltext::DrawShadowed(m_screen, ">", tile.x + TILE + GAP / 2 - 5, tile.y + 13, 2, grey);
        // B-15, hint only: the timing bar shrinks to empty at the ideal moment to cast this spell
        float until = 0.0f, span = 0.0f;
        if (m_recipeHint && boss.StepTiming(i, until, span))
        {
            SDL_Rect back = { tile.x, BOSS_COMBO_Y + TILE + 20, TILE, 4 };
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
                bar.w = static_cast<int>(TILE * (left > 1.0f ? 1.0f : left));
                SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 255);
                SDL_RenderFillRect(m_screen, &bar);
            }
            else  // past it: late now, the bar stays red until the spell is cast
            {
                SDL_SetRenderDrawColor(m_screen, 220, 60, 60, 255);
                SDL_RenderDrawRect(m_screen, &back);
            }
        }
        // B-16: the next quick step shows how long is left to cast it (always, with or without the hint)
        float quickLeft = 0.0f, quickTotal = 0.0f;
        if (boss.QuickTiming(i, quickLeft, quickTotal))
        {
            SDL_Rect back = { tile.x, BOSS_COMBO_Y + TILE + 20, TILE, 4 };
            SDL_SetRenderDrawColor(m_screen, 30, 32, 44, 255);
            SDL_RenderFillRect(m_screen, &back);
            float f = quickLeft / quickTotal;
            SDL_Rect bar = back;
            bar.w = static_cast<int>(TILE * (f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f)));
            bool perfect = quickTotal - quickLeft <= practice::BOSS_QUICK_PERFECT;
            bool great = quickTotal - quickLeft <= practice::BOSS_QUICK_GREAT;
            SDL_SetRenderDrawColor(m_screen, perfect ? 255 : (great ? 110 : 235), perfect ? 215 : (great ? 210 : 235), perfect ? 80 : (great ? 255 : 240), 255);
            SDL_RenderFillRect(m_screen, &bar);
        }
        if (m_recipeHint)
        {
            const invoker::Recipe& r = invoker::GetSkillDefinition(combo[i]).recipe;
            const int counts[3] = { r.quas, r.wex, r.exort };
            const int pitch = small ? 13 : 15;           // three orbs centred under the tile (smaller under a small one)
            int ox = tile.x + TILE / 2 - pitch;
            for (int element = 0; element < 3; ++element)
            {
                for (int c = 0; c < counts[element]; ++c, ox += pitch)
                {
                    int oy = BOSS_COMBO_Y + TILE + 11;
                    SDL_Color col = kOrbColors[element];
                    SDL_SetRenderDrawColor(m_screen, 10, 12, 18, 255);
                    draw::FillCircle(m_screen, ox, oy, small ? 7 : 8);
                    SDL_SetRenderDrawColor(m_screen, col.r, col.g, col.b, 255);
                    draw::FillCircle(m_screen, ox, oy, small ? 6 : 7);
                    const char* letter = element == 0 ? "Q" : element == 1 ? "W" : "E";
                    const SDL_Color white = { 255, 255, 255, 255 };
                    pixeltext::Draw(m_screen, letter, ox - 2, oy - 3, 1, white);
                }
            }
        }
    }

    if (warning)
        return;
    if (def.guided)
        drawMiddle("TORNADO LIFTS IT. LAND SUN STRIKE AS IT COMES DOWN.", 100, 1, grey);
    else if (n > 1 && boss.StepKind(1) == practice::BossStepKind::Quick)
        drawMiddle("QUICK COMBO: CAST EACH SPELL RIGHT AFTER THE LAST ONE.", 100, 1, grey);
    if (nowStep > 0)
    {
        float t = SDL_GetTicks() / 1000.0f;
        SDL_Color pulse = { 90, static_cast<Uint8>(200 + 55 * (0.5f + 0.5f * std::sin(t * 12.0f))), 110, 255 };
        snprintf(buf, sizeof(buf), "CAST %s NOW!", invoker::GetSkillDefinition(combo[nowStep]).name);
        int scale = !small && pixeltext::Width(buf, 3) <= 400 ? 3 : 2;  // long names: keep clear of the combo strip
        drawMiddle(buf, 116, scale, pulse);
    }
    else if (boss.CueNow())
    {
        float t = SDL_GetTicks() / 1000.0f;
        SDL_Color pulse = { 255, static_cast<Uint8>(200 + 55 * (0.5f + 0.5f * std::sin(t * 12.0f))), 80, 255 };
        drawMiddle("CAST NOW!", 116, 3, pulse);
    }
    else if (!running && boss.LastFail() != practice::ComboFail::None)
    {
        practice::ComboFail f = boss.LastFail();
        snprintf(buf, sizeof(buf), "LAST TRY: %s", f == practice::ComboFail::WrongSpell ? "WRONG SPELL"
            : f == practice::ComboFail::TooEarly ? "TOO EARLY" : f == practice::ComboFail::TooLate ? "TOO LATE" : "MISSED");
        drawMiddle(buf, 118, 2, red);
    }
}

// The five Immortals, each with its combo and best time; arrows + Enter, 1-5, or a tap.
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
    pixeltext::DrawShadowed(m_screen, "IMMORTALS", panel.x + 24, panel.y + 14, 3, gold);
    pixeltext::DrawShadowed(m_screen, "ONLY ITS COMBO HURTS AN IMMORTAL. THEY COME BY CHANCE IN PLAY.", panel.x + 210, panel.y + 22, 1, grey);

    // one compact row per boss (B-20): number and name, best time, and the combo as small icons (both phases)
    char buf[64], time[16];
    const int icon = 28, step = 33;
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

        snprintf(buf, sizeof(buf), "%d %s", i + 1, def.name);
        pixeltext::DrawShadowed(m_screen, buf, r.x + 10, r.y + 12, 2, selected ? gold : white);
        if (m_bossBest[i] > 0.0f)
        {
            FormatTime(time, sizeof(time), m_bossBest[i]);
            snprintf(buf, sizeof(buf), "BEST %s", time);
        }
        else
            snprintf(buf, sizeof(buf), "NOT BEATEN YET");
        pixeltext::DrawShadowed(m_screen, buf, r.x + 10, r.y + 36, 1, m_bossBest[i] > 0.0f ? gold : grey);

        int x = r.x + 232;
        for (int k = 0; k < def.comboLength; ++k, x += step)
            m_skillIcons[static_cast<int>(def.combo[k])].RenderAt(m_screen, x, r.y + 16, icon);
        if (def.combo2Length > 0)
        {
            pixeltext::DrawShadowed(m_screen, "+", x + 2, r.y + 15, 2, grey);
            x += 20;
            for (int k = 0; k < def.combo2Length; ++k, x += step)
                m_skillIcons[static_cast<int>(def.combo2[k])].RenderAt(m_screen, x, r.y + 8, icon);
        }
    }
    const char* help = m_showTouchControls ? "TAP ONE TO FIGHT IT   -   TAP OUTSIDE TO GO BACK" : "1-5 OR ARROWS + ENTER: FIGHT   -   ESC: BACK";
    pixeltext::DrawCentered(m_screen, help, SCREEN_WIDTH, panel.y + panel.h - 20, 1, grey);
}

// spec §28 O-3: "IMMORTAL INCOMING" and its name, pulsing, while the warning counts down (the boss stands at the
// edge of the field and its combo strip is up already, so the player can prepare the first spells).
void GameManager::RenderImmortalWarning()
{
    if (!ImmortalShown() || m_session.ImmortalActive())
        return;
    float t = SDL_GetTicks() / 1000.0f;
    float pulse = 0.5f + 0.5f * std::sin(t * 11.0f);
    SDL_Color red = { 255, static_cast<Uint8>(60 + 90 * pulse), static_cast<Uint8>(60 + 40 * pulse), 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 200, 30, 30, static_cast<Uint8>(50 + 60 * pulse));
    for (int k = 0; k < 6; ++k)  // a red frame around the screen, like an alarm
    {
        SDL_Rect frame = { k, k, SCREEN_WIDTH - 2 * k, SCREEN_HEIGHT - 2 * k };
        SDL_RenderDrawRect(m_screen, &frame);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    // centred, but clear of the combo strip at the top right (eight tiles reach the middle of the screen)
    const practice::BossDefinition& def = practice::GetBossDefinition(m_session.ImmortalBoss());
    int n = def.comboLength;
    int strip = n > 5 ? n * BOSS_COMBO_TILE_SMALL + (n - 1) * BOSS_COMBO_GAP_SMALL : n * BOSS_COMBO_TILE + (n - 1) * BOSS_COMBO_GAP;
    int limit = SCREEN_WIDTH - 16 - strip - 14;
    const char* title = "IMMORTAL INCOMING";
    int w = pixeltext::Width(title, 3);
    int x = (SCREEN_WIDTH - w) / 2;
    if (x + w > limit)
        x = limit - w;
    pixeltext::DrawShadowed(m_screen, title, x, 92, 3, red);
    pixeltext::DrawShadowed(m_screen, def.name, x + (w - pixeltext::Width(def.name, 2)) / 2, 122, 2, gold);
}

// §32 M-3: the chance that the next thing after an elite or an overlord is an Immortal. Bottom centre, where the
// Immortal's own bar is during its fight. It lights up when the chance grows.
void GameManager::RenderImmortalMeter()
{
    if (m_bossActive || m_tutorialActive || m_session.State() != practice::GameState::Playing
        || m_session.Mode() != practice::SessionMode::Play || ImmortalShown())
        return;
    const SDL_Color violet = { 200, 150, 255, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    int chance = m_session.ImmortalChance();
    bool coming = m_session.ImmortalDue();
    bool lit = m_meterFlashLeft > 0.0f && static_cast<int>(m_meterFlashLeft * 12.0f) % 2 == 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", chance);
    const char* label = "IMMORTAL";
    int labelW = pixeltext::Width(label, 2);
    int x = (SCREEN_WIDTH - (labelW + 12 + IMMORTAL_METER_W + 10 + pixeltext::Width("100%", 2))) / 2;
    int y = IMMORTAL_BAR_Y;
    pixeltext::DrawShadowed(m_screen, label, x, y + 3, 2, coming || lit ? gold : violet);
    int barX = x + labelW + 12;
    SDL_Rect bar = { barX, y + 2, IMMORTAL_METER_W, 16 };
    SDL_Rect fill = { barX + 1, y + 3, (IMMORTAL_METER_W - 2) * chance / 100, 14 };
    SDL_SetRenderDrawColor(m_screen, 30, 20, 40, 255);
    SDL_RenderFillRect(m_screen, &bar);
    if (coming || lit)
        SDL_SetRenderDrawColor(m_screen, 255, 210, 90, 255);
    else
        SDL_SetRenderDrawColor(m_screen, 150, 90, 230, 255);
    SDL_RenderFillRect(m_screen, &fill);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
    SDL_RenderDrawRect(m_screen, &bar);
    pixeltext::DrawShadowed(m_screen, buf, barX + IMMORTAL_METER_W + 10, y + 3, 2, coming ? gold : white);
}

// The IMMORTAL's name and HP bar (in %), bottom centre; the last combo's damage beside it, and "X2" while a
// Refresher Orb is armed (O-6).
void GameManager::RenderImmortalBar()
{
    if (!ImmortalFight())
        return;
    const practice::BossSession& boss = m_session.Immortal();
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    char buf[32];
    const char* name = boss.Def().name;
    int nameW = pixeltext::Width(name, 2);
    int x = (SCREEN_WIDTH - (nameW + 16 + BOSS_HP_BAR_W + 10 + pixeltext::Width("100%", 2))) / 2;
    int y = IMMORTAL_BAR_Y;
    pixeltext::DrawShadowed(m_screen, name, x, y + 3, 2, gold);
    int barX = x + nameW + 16;
    SDL_Rect bar = { barX, y, BOSS_HP_BAR_W, 20 };
    SDL_Rect fill = { barX + 1, y + 1, (BOSS_HP_BAR_W - 2) * boss.BossHp() / practice::BOSS_FULL_HP, 18 };
    SDL_SetRenderDrawColor(m_screen, 30, 20, 40, 255);
    SDL_RenderFillRect(m_screen, &bar);
    SDL_SetRenderDrawColor(m_screen, 200, 50, 70, 255);
    SDL_RenderFillRect(m_screen, &fill);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 255);
    SDL_RenderDrawRect(m_screen, &bar);
    snprintf(buf, sizeof(buf), "%d%%", boss.BossHp());
    pixeltext::DrawShadowed(m_screen, buf, barX + BOSS_HP_BAR_W + 10, y + 3, 2, white);
    if (m_bossDamageLeft > 0.0f)
    {
        snprintf(buf, sizeof(buf), "-%d%%", m_bossDamageShown);
        pixeltext::DrawShadowed(m_screen, buf, barX + BOSS_HP_BAR_W + 70, y + 3, 2, gold);
    }
    else if (m_session.RefresherArmed())
        pixeltext::DrawShadowed(m_screen, "X2", barX + BOSS_HP_BAR_W + 70, y + 3, 2, gold);
}

// Victory or defeat: the time, the best time, and how to go on (the same tap zones as Practice's Game Over).
void GameManager::RenderBossResult()
{
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color red = { 235, 70, 70, 255 };
    char value[32];
    bool won = m_boss.State() == practice::BossState::Won;
    int i = m_boss.BossIndex();

    ResultBegin(won ? "IMMORTAL DEFEATED!" : "DEFEATED", won ? gold : red, m_boss.Def().name, white, 2, 0, 0);
    if (won)
    {
        FormatTime(value, sizeof(value), m_boss.Elapsed());
        ResultRow("TIME", value, m_bossNewBest ? "NEW BEST!" : NULL, m_bossNewBest ? 1 : 0);
    }
    else
    {
        snprintf(value, sizeof(value), "%d%%", m_boss.BossHp());
        ResultRow("ITS HP LEFT", value);
    }
    if (m_bossBest[i] > 0.0f)
    {
        FormatTime(value, sizeof(value), m_bossBest[i]);
        ResultRow("BEST TIME", value, NULL, 2);
    }
    else
        ResultRow("BEST TIME", "NOT BEATEN YET", NULL, 2);
    ResultButtons("FIGHT AGAIN", "IMMORTALS", false);
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
    for (int i = 0; i < practice::ITEM_COUNT; ++i)
        for (int l = 0; l < 2; ++l)
            if (m_itemIcons[i][l] != NULL)
            {
                SDL_DestroyTexture(m_itemIcons[i][l]);
                m_itemIcons[i][l] = NULL;
            }
    for (int i = 0; i < practice::MATERIAL_COUNT + 6; ++i)
    {
        SDL_Texture*& icon = i < practice::MATERIAL_COUNT ? m_materialIcons[i] : m_runeIcons[i - practice::MATERIAL_COUNT];
        if (icon != NULL)
        {
            SDL_DestroyTexture(icon);
            icon = NULL;
        }
    }
    SDL_Texture** sheets[5 + 2 * IMMORTAL_ART_COUNT] = { &m_playerRunSheet, &m_playerCastSheet, &m_meteorSheet, &m_forgeSheet,
        &m_iceWallSheet };
    for (int i = 0; i < IMMORTAL_ART_COUNT; ++i)
    {
        sheets[5 + 2 * i] = &m_immortalRun[i];
        sheets[6 + 2 * i] = &m_immortalHit[i];
    }
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