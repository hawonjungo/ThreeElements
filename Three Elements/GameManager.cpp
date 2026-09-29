
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
#include <ctime>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


GameManager* GameManager::instance_ = NULL;

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
    m_window = SDL_CreateWindow("Three Elements",
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
    m_hasPlayer = m_player.LoadImg("assets/main.bmp", m_screen);

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
    LoadTopScores();
    LoadBests();
    LoadSettings();
    m_session.RestoreBestCombo(m_bests.combo);  // the HUD's BEST is the all-time record from the start

    if (m_hasPlayer)
    {
        m_player.set_clips();
        m_player.SetPos(10, 385);
    }

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
        OnCastJudged(update.cast, enemyBefore);
    if (update.leaked)
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
    m_tutorialWrongFlash -= dt;
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
    if (m_shakeLeft > 0.0f)
    {
        float strength = m_shakeLeft / FEEDBACK_SHAKE_TIME;
        float t = static_cast<float>(now) / 1000.0f;
        SDL_Rect shaken = { static_cast<int>(FEEDBACK_SHAKE_PX * strength * (0.5f + 0.5f * std::sin(t * 90.0f))),
            static_cast<int>(FEEDBACK_SHAKE_PX * strength * (0.5f + 0.5f * std::cos(t * 70.0f))),
            SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderSetViewport(m_screen, &shaken);
    }

    // Update and render background layers
    updateBackgroundLayers(dt);
    renderBackgroundLayers();

    RenderGhostWalk();  // behind the player: the player is never covered
    if (m_hasPlayer)
    {
        m_player.Render(m_screen);
    }
    RenderEnemy();
    RenderTornadoes();  // above the background, player and enemy, below the HUD
    RenderSkillVfx();   // code-drawn skill effects, above everything else in the scene
    RenderFeedback();   // "+1" / "MISS" and the defeat ring, part of the scene
    SDL_RenderSetViewport(m_screen, NULL);  // end of the (possibly shaken) scene

    RenderLeakFlash();
    RenderTouchControls();  // on top of the scene, only while Playing
    RenderInvokerHud();
    RenderStatsHud();
    RenderTargetHint();

    if (m_tutorialActive)
        RenderTutorial();
    else if (m_session.State() == practice::GameState::Ready)
        RenderReadyScreen();
    else if (m_session.State() == practice::GameState::GameOver)
        RenderGameOverScreen();
    if (m_showRecipes && m_session.State() != practice::GameState::Playing)
        RenderRecipes();
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
    if (m_showRecipes)  // the recipe reference is open: any key just closes it
    {
        m_showRecipes = false;
        return;
    }
    if (m_tutorialActive)
    {
        HandleTutorialKey(sym, e);
        return;
    }
    if (sym == SDLK_h && m_session.State() != practice::GameState::Playing) { m_showRecipes = true; return; }
    if (sym == SDLK_t && m_session.State() == practice::GameState::Ready) { StartTutorial(); return; }
    // Enter / Esc only map to the session control calls; the rules are in PracticeSession.
    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) { PressEscapeAction(quit); return; }  // AC_BACK: Android Back
    if (sym == SDLK_m) { ToggleMute(); return; }  // not a gameplay key: works in every state
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

    if (hit(SOUND_BUTTON_RECT))  // sound on/off, in every state
    {
        ToggleMute();
        return;
    }
    if (m_showRecipes)  // the recipe reference is open: a tap anywhere closes it
    {
        m_showRecipes = false;
        return;
    }
    if (m_tutorialActive)
    {
        HandleTutorialPointer(x, y);
        return;
    }
    if (m_session.State() == practice::GameState::Ready && hit(TUTORIAL_BUTTON_READY_RECT))
    {
        StartTutorial();
        return;
    }
    if ((m_session.State() == practice::GameState::Ready && hit(RECIPES_BUTTON_READY_RECT)) ||
        (m_session.State() == practice::GameState::GameOver && hit(RECIPES_BUTTON_GAMEOVER_RECT)))
    {
        m_showRecipes = true;
        return;
    }

    switch (m_session.State())
    {
    case practice::GameState::Ready:
        if (hit(TOUCH_READY_START_RECT))
            PressEnterAction();
        else if (hit(TOUCH_READY_QUIT_RECT))
            PressEscapeAction(quit);
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

    PresentInvokerResult(action, r.invoker, r.cast, hadEnemy, enemyBody);

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
    practice::CastOutcome cast, bool hadEnemy, const practice::Bounds& enemyBody)
{
    // sounds: each orb has its element's sound, R only sounds when it really invoked, a judged cast sounds right /
    // wrong (OnCastJudged), and a cast that is not judged at once (Tornado launch, no enemy) just whooshes
    if (result.event == invoker::InvokerEvent::OrbAdded)
        audio::Play(action == invoker::InputAction::Q ? audio::Sfx::OrbQuas
            : action == invoker::InputAction::W ? audio::Sfx::OrbWex : audio::Sfx::OrbExort);
    else if (result.event == invoker::InvokerEvent::Invoked)
        audio::Play(audio::Sfx::Invoke);
    else if (result.event == invoker::InvokerEvent::Cast && cast == practice::CastOutcome::None)
        audio::Play(audio::Sfx::Cast);

    if (result.event == invoker::InvokerEvent::Cast && result.skill == invoker::SkillId::GhostWalk)
        m_ghostWalkLeft = GHOST_WALK_DURATION;  // visual only; the cast is judged like any other spell
    if (result.event == invoker::InvokerEvent::Cast)
        StartSkillVfx(result.skill, hadEnemy, enemyBody);  // no-op for Tornado / Ghost Walk (own effects)
    if (cast != practice::CastOutcome::None)
        OnCastJudged(cast, enemyBody);
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
        if (SubmitScore(st.score))
            printf("[practice] new top-10 score!\n");
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
    if (flash)
        SDL_SetTextureColorMod(sprite.p_object_, 255, 90, 90);
    sprite.Render(m_screen);
    if (flash)
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
    for (int i = 0; i < m_session.ActiveTornadoCount(); ++i)
    {
        const practice::Tornado& t = m_session.GetTornado(i);

        int frame = practice::TornadoFrame(t.animTime);
        SDL_Rect src = { (frame % TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE, (frame / TORNADO_SHEET_COLUMNS) * TORNADO_FRAME_SIZE,
            TORNADO_FRAME_SIZE, TORNADO_FRAME_SIZE };
        SDL_Rect dst = { static_cast<int>(t.x) - size / 2, static_cast<int>(t.y) - size / 2, size, size };
        SDL_RenderCopy(m_screen, m_tornadoSheet, &src, &dst);
    }
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
void GameManager::OnCastJudged(practice::CastOutcome outcome, const practice::Bounds& enemy)
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
                correct ? "+1" : "MISS", correct ? gold : red };
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

// Loads the saved top-10 list: a small text file next to the exe on native, the browser's localStorage on
// the web build (a native file write there would only live in Emscripten's in-memory FS and vanish on
// reload). Simplest practical persistence for a prototype: no database, no server, no new file format.
void GameManager::LoadTopScores()
{
    for (int i = 0; i < 10; ++i)
        m_topScores[i] = 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var raw = localStorage.getItem('threeElements_topScores');
        var values = raw ? raw.split(',').map(Number) : [];
        for (var i = 0; i < 10; ++i)
            HEAP32[($0 >> 2) + i] = values[i] | 0;
    }, m_topScores);
#else
    FILE* f = fopen(SavePath("highscores.txt").c_str(), "r");
    if (f != NULL)
    {
        for (int i = 0; i < 10 && fscanf(f, "%d", &m_topScores[i]) == 1; ++i) {}
        fclose(f);
    }
#endif
}

// Writes the top-10 list back out to the same place LoadTopScores() reads from.
void GameManager::SaveTopScores()
{
#ifdef __EMSCRIPTEN__
    EM_ASM({
        var values = [];
        for (var i = 0; i < 10; ++i)
            values.push(HEAP32[($0 >> 2) + i]);
        localStorage.setItem('threeElements_topScores', values.join(','));
    }, m_topScores);
#else
    FILE* f = fopen(SavePath("highscores.txt").c_str(), "w");
    if (f != NULL)
    {
        for (int i = 0; i < 10; ++i)
            fprintf(f, "%d\n", m_topScores[i]);
        fclose(f);
    }
#endif
}

// Inserts `score` into the sorted top-10 list if it belongs there (a plain insertion sort over 10 slots -
// there is no need for anything fancier), and saves immediately. Returns whether it made the list.
bool GameManager::SubmitScore(int score)
{
    if (score <= m_topScores[9])
        return false;
    int i = 9;
    while (i > 0 && m_topScores[i - 1] < score)
    {
        m_topScores[i] = m_topScores[i - 1];
        --i;
    }
    m_topScores[i] = score;
    SaveTopScores();
    return true;
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
    if (m_session.State() != practice::GameState::Playing)
        return;
    practice::BestStats sofar = m_bests;
    if (practice::MergeBests(sofar, m_session.GetStats()).Any())
        SaveBests(sofar);
}

// Game coordinates (0..928, 0..544) of a touch. SDL_FINGER* positions are 0..1 of the whole window, while the game
// is drawn into a letterboxed logical area (SDL_RenderSetLogicalSize): go through the renderer's scale and viewport.
void GameManager::TouchToGame(float normX, float normY, int& gameX, int& gameY)
{
    int outW = 0, outH = 0;
    SDL_GetRendererOutputSize(m_screen, &outW, &outH);
    float scaleX = 1.0f, scaleY = 1.0f;
    SDL_RenderGetScale(m_screen, &scaleX, &scaleY);
    SDL_Rect viewport;
    SDL_RenderGetViewport(m_screen, &viewport);
    gameX = static_cast<int>(normX * outW / scaleX) - viewport.x;
    gameY = static_cast<int>(normY * outH / scaleY) - viewport.y;
}

// A session is over, by Game Over or by Esc while Playing: its score, combo and survival time can set records.
void GameManager::EndSession()
{
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
#ifdef __EMSCRIPTEN__
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
        if (fscanf(f, "muted %d tutorial %d", &muted, &tutorialDone) < 1)
            muted = 0;
        fclose(f);
    }
#endif
    audio::SetMuted(muted != 0);
    m_tutorialDone = tutorialDone != 0;
}

void GameManager::SaveSettings()
{
    int muted = audio::IsMuted() ? 1 : 0;
    int tutorialDone = m_tutorialDone ? 1 : 0;
#ifdef __EMSCRIPTEN__
    EM_ASM({
        try {
            localStorage.setItem('threeElements_muted', $0 ? '1' : '0');
            localStorage.setItem('threeElements_tutorialDone', $1 ? '1' : '0');
        } catch (e) {}
    }, muted, tutorialDone);
#else
    FILE* f = fopen(SavePath("settings.txt").c_str(), "w");
    if (f != NULL)
    {
        fprintf(f, "muted %d tutorial %d\n", muted, tutorialDone);
        fclose(f);
    }
#endif
}

void GameManager::ToggleMute()
{
    audio::SetMuted(!audio::IsMuted());
    SaveSettings();
    printf("[audio] sound %s\n", audio::IsMuted() ? "off" : "on");
}

// "SOUND ON" / "SOUND OFF" under ACC (tap or click it, or press M). Hidden when there is no audio device at all.
void GameManager::RenderSoundButton()
{
    if (!m_audioReady)
        return;
    const SDL_Rect& r = SOUND_BUTTON_RECT;
    bool muted = audio::IsMuted();
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 20, 22, 30, 160);
    SDL_RenderFillRect(m_screen, &r);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 110);
    SDL_RenderDrawRect(m_screen, &r);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    const char* label = muted ? "SOUND OFF" : "SOUND ON";
    const SDL_Color on = { 200, 235, 200, 255 };
    const SDL_Color off = { 170, 170, 180, 255 };
    pixeltext::DrawShadowed(m_screen, label, r.x + (r.w - pixeltext::Width(label, 1)) / 2, r.y + (r.h - 7) / 2, 1,
        muted ? off : on);
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
        PressEnterAction();
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
        RenderHighlight({ elementPos[0].first - 26, elementPos[0].second - 26, elementPos[2].first - elementPos[0].first + 52, 52 });
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
            RenderHighlight({ skillPos[i].first - 2, skillPos[i].second - 38, SKILL_SLOT_SIZE + 4, SKILL_SLOT_SIZE + 40 });
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
        RenderButton(TUTORIAL_PLAY_RECT, "PLAY PRACTICE", true);
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
        skillvfx::Render(m_screen, static_cast<invoker::SkillId>(i), m_skillVfx[i]);
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
        draw::FillCircle(m_screen, elementPos[i].first, elementPos[i].second, 20);
        SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 140);
        draw::FillCircle(m_screen, elementPos[i].first, elementPos[i].second, 17);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    for (int i = 0; i < inv.OrbCount(); ++i)
        RenderOrb(inv.GetOrb(i), elementPos[i].first, elementPos[i].second);

    // frames for the two slots, so an empty slot is visible too (the icons are cut-outs, the frame is their tile)
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 2; ++i)
    {
        SDL_Rect frame = { skillPos[i].first - 2, skillPos[i].second - 2, SKILL_SLOT_SIZE + 4, SKILL_SLOT_SIZE + 4 };
        SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 170);
        SDL_RenderFillRect(m_screen, &frame);
        SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 70);
        SDL_RenderDrawRect(m_screen, &frame);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    m_keyD.SetPos(skillPos[0].first + 16, skillPos[0].second - 36);
    m_keyD.Render(m_screen);
    m_keyF.SetPos(skillPos[1].first + 16, skillPos[1].second - 36);
    m_keyF.Render(m_screen);

    invoker::SkillId spellD = inv.GetSlot(invoker::Slot::D);
    invoker::SkillId spellF = inv.GetSlot(invoker::Slot::F);
    if (spellD != invoker::SkillId::None)
        m_skillIcons[static_cast<int>(spellD)].RenderAt(m_screen, skillPos[0].first, skillPos[0].second, SKILL_SLOT_SIZE);
    if (spellF != invoker::SkillId::None)
        m_skillIcons[static_cast<int>(spellF)].RenderAt(m_screen, skillPos[1].first, skillPos[1].second, SKILL_SLOT_SIZE);
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
    snprintf(buf, sizeof(buf), "BEST %d", st.bestCombo);
    pixeltext::DrawShadowed(m_screen, buf, 590, 12, 2, white);

    // row 2: accuracy and survival time
    char value[16];
    FormatAccuracy(value, sizeof(value), st);
    snprintf(buf, sizeof(buf), "ACC %s", value);
    pixeltext::DrawShadowed(m_screen, buf, 16, 42, 2, white);
    FormatTime(value, sizeof(value), st.survivalTime);
    snprintf(buf, sizeof(buf), "TIME %s", value);
    pixeltext::DrawShadowed(m_screen, buf, 190, 42, 2, white);

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

void GameManager::RenderReadyScreen()
{
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color gold = { 255, 210, 90, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    DimScreen(150);
    pixeltext::DrawCentered(m_screen, "THREE ELEMENTS", SCREEN_WIDTH, 120, 6, gold);
    pixeltext::DrawCentered(m_screen, "PRACTICE MODE", SCREEN_WIDTH, 190, 3, white);

    // the persistent records (spec §13)
    char time[16];
    char buf[96];
    FormatTime(time, sizeof(time), m_bests.survivalTime);
    snprintf(buf, sizeof(buf), "BEST  SCORE %d   COMBO %d   TIME %s", m_bests.score, m_bests.combo, time);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 230, 2, gold);

    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO START", SCREEN_WIDTH, 270, 3, white);
    pixeltext::DrawCentered(m_screen, "Q W E  ORBS     R  INVOKE     D F  CAST", SCREEN_WIDTH, 350, 2, grey);
#ifndef __EMSCRIPTEN__
    pixeltext::DrawCentered(m_screen, "ESC  QUIT", SCREEN_WIDTH, 385, 2, grey);  // nothing to quit on the web
#endif
    RenderRecipesButton(RECIPES_BUTTON_READY_RECT);
    RenderButton(TUTORIAL_BUTTON_READY_RECT, "TUTORIAL  (T)", !m_tutorialDone);  // pulses until done once
}

void GameManager::RenderGameOverScreen()
{
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

    char topBuf[128];
    int pos = snprintf(topBuf, sizeof(topBuf), "TOP 10");
    for (int i = 0; i < 10 && pos < static_cast<int>(sizeof(topBuf)); ++i)
        pos += snprintf(topBuf + pos, sizeof(topBuf) - pos, " %d", m_topScores[i]);
    pixeltext::DrawCentered(m_screen, topBuf, SCREEN_WIDTH, 312, 2, white);

    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO RESTART", SCREEN_WIDTH, 350, 3, white);
    pixeltext::DrawCentered(m_screen, "ESC  MENU", SCREEN_WIDTH, 395, 2, grey);
    RenderRecipesButton(RECIPES_BUTTON_GAMEOVER_RECT);
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

    // the skill's icon below the text, centred under it, large enough to read at a glance (owner 2026-09-30)
    SDL_Rect tile = { area.x + (area.w - SKILL_HINT_SIZE - 4) / 2, 100, SKILL_HINT_SIZE + 4, SKILL_HINT_SIZE + 4 };
    if (tile.x + tile.w > SCREEN_WIDTH - 16)  // short names: keep the tile inside the right margin
        tile.x = SCREEN_WIDTH - 16 - tile.w;
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 10, 12, 20, 170);
    SDL_RenderFillRect(m_screen, &tile);
    SDL_SetRenderDrawColor(m_screen, 255, 235, 60, 160);
    SDL_RenderDrawRect(m_screen, &tile);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    m_skillIcons[static_cast<int>(e.target)].RenderAt(m_screen, tile.x + 2, tile.y + 2, SKILL_HINT_SIZE);
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
    for (int i = 0; i < 12; ++i) {
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