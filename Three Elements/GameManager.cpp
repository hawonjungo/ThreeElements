
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS  // plain fopen/fscanf (LoadTopScores/SaveTopScores) instead of the MSVC-only *_s variants,
#endif                            // so the same code compiles unchanged under Emscripten's clang for the web build
#include "GameManager.h"
#include "MainPlayer.h"
#include "Skill.h"
#include "ImpTimer.h"
#include "PixelText.h"
#include <cmath>
#include <cstdio>
#include <ctime>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


GameManager* GameManager::instance_ = NULL;


GameManager::GameManager()
{
    m_window = NULL;
    m_screen = NULL;

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

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    m_window = SDL_CreateWindow("Three Elements",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN);

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

void GameManager::LoopGame()
{
    // frame fps
    ImpTimer fps_timer;

    bool bPlayer = m_player.LoadImg("assets/main.bmp", m_screen);

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
        m_skillIcons[i].LoadImg(invoker::GetSkillDefinition(static_cast<invoker::SkillId>(i)).icon, m_screen);
        m_skillIcons[i].set_clips();
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
    LoadPlaceholderVfxSheets();  // TEST placeholders for the 8 skills without a real effect yet
    LoadTopScores();

    if (bPlayer)
    {
        m_player.set_clips();
        m_player.SetPos(10, 385);
    }

#ifdef __EMSCRIPTEN__
    // Phones/tablets show the on-screen Q/W/E/R/D/F cluster from the start; a PC browser keeps it hidden
    // (same media query the web shell uses). Any finger touch later turns it on too (see SDL_FINGERDOWN).
    m_showTouchControls = EM_ASM_INT({ return window.matchMedia('(hover: none) and (pointer: coarse)').matches ? 1 : 0; }) != 0;
#endif

    printf("Three Elements - Practice Mode. Press Enter to start.\n");

    Uint32 lastTick = SDL_GetTicks();
    bool bStop = false;
    // ====================== render here !!!
    while (!bStop)
    {
        fps_timer.start();

        // Real time since the previous frame. The Practice rules run on this, not on the frame count.
        Uint32 now = SDL_GetTicks();
        float dt = static_cast<float>(now - lastTick) / 1000.0f;
        lastTick = now;

        //Handle events on queue
        while (SDL_PollEvent(&m_event) != 0)
        {
            //User requests quit
            if (m_event.type == SDL_QUIT)
            {
                bStop = true;
            }
            else if (m_event.type == SDL_KEYDOWN)
            {
                HandleKeyDown(m_event, bStop);
            }
            else if (m_event.type == SDL_FINGERDOWN)  // phone/tablet touch (normalised 0..1 coordinates)
            {
                m_showTouchControls = true;  // a touch screen is in use, whatever the media query said
                HandlePointerDown(static_cast<int>(m_event.tfinger.x * SCREEN_WIDTH),
                    static_cast<int>(m_event.tfinger.y * SCREEN_HEIGHT), bStop);
            }
            else if (m_event.type == SDL_MOUSEBUTTONDOWN && m_event.button.which != SDL_TOUCH_MOUSEID)
            {
                // which == SDL_TOUCH_MOUSEID would be a synthetic mouse event for a tap SDL_FINGERDOWN
                // already handled above; a real mouse click (desktop testing) is not, and still works.
                HandlePointerDown(m_event.button.x, m_event.button.y, bStop);
            }
        }

        // Practice rules: enemy movement, spawning, leaks, Game Over (input of this frame is already applied)
        LogUpdate(m_session.Update(dt));
        m_ghostWalkLeft = m_ghostWalkLeft > dt ? m_ghostWalkLeft - dt : 0.0f;
        for (int vi = 0; vi < invoker::SKILL_COUNT; ++vi)
            m_placeholderVfxLeft[vi] = m_placeholderVfxLeft[vi] > dt ? m_placeholderVfxLeft[vi] - dt : 0.0f;

        //Clear screen
        SDL_SetRenderDrawColor(m_screen, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(m_screen);

        // Update and render background layers
        updateBackgroundLayers();
        renderBackgroundLayers();

        RenderGhostWalk();  // behind the player: the player is never covered
        if (bPlayer)
        {
            m_player.Render(m_screen);
        }
        RenderEnemy();
        RenderTornadoes();  // above the background, player and enemy, below the HUD
        RenderPlaceholderVfx();  // TEST placeholders, drawn above everything else in the scene
        RenderTouchControls();  // on top of the scene, only while Playing
        RenderInvokerHud();
        RenderStatsHud();
        RenderTargetHint();

        if (m_session.State() == practice::GameState::Ready)
            RenderReadyScreen();
        else if (m_session.State() == practice::GameState::GameOver)
            RenderGameOverScreen();

        //Update screen
        SDL_RenderPresent(m_screen);

        int real_imp_time = fps_timer.get_ticks();
        int time_one_frame = 1000 / FRAME_PER_SECOND;// ms

        if (real_imp_time < time_one_frame)
        {
            int delay_time = time_one_frame - real_imp_time;
            if (delay_time >= 0)
                SDL_Delay(delay_time);
        }
    }

    Close();
}

// ------------------------------------------------------------------ input and session

void GameManager::HandleKeyDown(const SDL_Event& e, bool& quit)
{
    if (e.key.repeat)  // auto-repeat of a held key is not a new press (Enter/Esc included)
        return;

    SDL_Keycode sym = e.key.keysym.sym;
    // Enter / Esc only map to the session control calls; the rules are in PracticeSession.
    if (sym == SDLK_ESCAPE) { PressEscapeAction(quit); return; }
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
        printf("[practice] session started (seed %u)\n", seed);
    }
}

// Ready -> quit the application. Playing / Game Over -> step back to Ready, never quit. Shared by the
// Esc key and every touch "back/quit" tap zone.
void GameManager::PressEscapeAction(bool& quit)
{
    ResetVisualEffects();
    if (m_session.PressEscape())
        quit = true;
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

    if (r.invoker.event == invoker::InvokerEvent::Cast && r.invoker.skill == invoker::SkillId::GhostWalk)
        m_ghostWalkLeft = GHOST_WALK_DURATION;  // visual only; the cast is judged like any other spell
    if (r.invoker.event == invoker::InvokerEvent::Cast)
        StartPlaceholderVfx(r.invoker.skill, hadEnemy, enemyBody);  // no-op for skills with no placeholder

    if (r.tornadoLaunched)
        printf("[practice] Tornado launched (judged when it hits the enemy)\n");
    LogOutcome(r.cast);
}

// The six touch buttons (Q/W/E/R/D/F), drawn only while Playing (matches the keyboard: those keys are
// no-ops in Ready/Game Over too). Reuses the already-loaded keyboard icons at a larger size - no new art.
void GameManager::RenderTouchControls()
{
    if (m_session.State() != practice::GameState::Playing || !m_showTouchControls)
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

// Filled disc, one horizontal line per row (SDL2 has no circle primitive).
static void FillCircle(SDL_Renderer* renderer, int cx, int cy, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        int dx = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// One active orb: a disc in its element colour (ice / lightning / fire) with a light core and its key letter.
void GameManager::RenderOrb(invoker::Orb orb, int centerX, int centerY)
{
    SDL_Color c = kOrbColors[static_cast<int>(orb)];
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 70);   // soft glow
    FillCircle(m_screen, centerX, centerY, 26);
    SDL_SetRenderDrawColor(m_screen, 10, 12, 18, 255);     // dark rim
    FillCircle(m_screen, centerX, centerY, 20);
    SDL_SetRenderDrawColor(m_screen, c.r, c.g, c.b, 255);
    FillCircle(m_screen, centerX, centerY, 18);
    SDL_SetRenderDrawColor(m_screen, 255, 255, 255, 110);  // highlight, top-left
    FillCircle(m_screen, centerX - 6, centerY - 6, 6);
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    const char* letter = orb == invoker::Orb::Quas ? "Q" : orb == invoker::Orb::Wex ? "W" : "E";
    const SDL_Color white = { 255, 255, 255, 255 };
    pixeltext::DrawShadowed(m_screen, letter, centerX - pixeltext::Width(letter, 2) / 2, centerY - 7, 2, white);
}

// The single active enemy, drawn where the Practice session says it is.
void GameManager::RenderEnemy()
{
    const practice::ActiveEnemy& e = m_session.Enemy();
    if (!e.active)
        return;

    const practice::EnemyDefinition& def = practice::GetEnemyDefinition(e.definition);
    EnemyObject& sprite = m_enemySprites[e.definition];
    // e.x is the left edge of the visible body; the frame starts bodyLeft pixels earlier
    sprite.SetPos(static_cast<int>(e.x) - def.bodyLeft, static_cast<int>(practice::GROUND_LINE_Y) - def.feetRow);
    sprite.Render(m_screen);
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
        m_placeholderVfxLeft[i] = 0.0f;
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
    FILE* f = fopen("highscores.txt", "r");
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
    FILE* f = fopen("highscores.txt", "w");
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

// Loads the 8 TEST placeholder sheets the same way LoadTornadoSheet() loads Tornado's: no colour key, exact
// 4 x 4 / 128 px size check. A missing or wrong-sized sheet just means that one skill draws nothing; the others
// and the rest of the game are unaffected.
bool GameManager::LoadPlaceholderVfxSheets()
{
    bool allOk = true;
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
    {
        const char* path = kPlaceholderVfxPath[i];
        if (path == NULL)
            continue;

        SDL_Surface* surface = IMG_Load(path);
        if (surface == NULL)
        {
            printf("Failed to load placeholder VFX %s: %s\n", path, IMG_GetError());
            allOk = false;
            continue;
        }
        const int size = PLACEHOLDER_VFX_SHEET_COLUMNS * PLACEHOLDER_VFX_FRAME_SIZE;
        if (surface->w == size && surface->h == size)
            m_placeholderVfxSheet[i] = SDL_CreateTextureFromSurface(m_screen, surface);
        else
            printf("Placeholder VFX %s is %dx%d, expected %dx%d: not used\n", path, surface->w, surface->h, size, size);
        SDL_FreeSurface(surface);

        if (m_placeholderVfxSheet[i] == NULL)
            allOk = false;
        else
        {
            SDL_SetTextureBlendMode(m_placeholderVfxSheet[i], SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(m_placeholderVfxSheet[i], SDL_ScaleModeNearest);
        }
    }
    return allOk;
}

// Starts (or restarts) the placeholder for `skill`, if it has one. Enemy-targeted placeholders, and any that
// travel (speed > 0, so they need a direction to travel in), need an enemy to aim at (`hadEnemy`); with none,
// nothing is shown or started, same convention as casting into an empty encounter.
void GameManager::StartPlaceholderVfx(invoker::SkillId skill, bool hadEnemy, const practice::Bounds& enemyBody)
{
    int i = static_cast<int>(skill);
    if (i < 0 || i >= invoker::SKILL_COUNT || m_placeholderVfxSheet[i] == NULL)
        return;
    bool needsEnemy = !kPlaceholderVfxAtPlayer[i] || kPlaceholderVfxSpeed[i] > 0.0f;
    if (needsEnemy && !hadEnemy)
        return;

    m_placeholderVfxLeft[i] = kPlaceholderVfxDuration[i];
    if (kPlaceholderVfxAtPlayer[i])
    {
        m_placeholderVfxX[i] = PLAYER_BODY_CENTER_X;
        m_placeholderVfxY[i] = PLAYER_BODY_CENTER_Y;
    }
    else
    {
        m_placeholderVfxX[i] = static_cast<int>(enemyBody.x + enemyBody.w * 0.5f);
        m_placeholderVfxY[i] = static_cast<int>(enemyBody.y + enemyBody.h * 0.5f);
    }

    // Direction from the origin toward the enemy, captured once here: fixed for the life of the effect (no homing).
    m_placeholderVfxDirX[i] = 1.0f;
    m_placeholderVfxDirY[i] = 0.0f;
    if (hadEnemy)
    {
        float dx = (enemyBody.x + enemyBody.w * 0.5f) - m_placeholderVfxX[i];
        float dy = (enemyBody.y + enemyBody.h * 0.5f) - m_placeholderVfxY[i];
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f)
        {
            m_placeholderVfxDirX[i] = dx / len;
            m_placeholderVfxDirY[i] = dy / len;
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

// The 8 TEST placeholders: one non-looping (or looping, for Alacrity) animation each, drawn at the position and
// for the duration StartPlaceholderVfx() set. Nothing is drawn once its time runs out.
void GameManager::RenderPlaceholderVfx()
{
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
    {
        if (m_placeholderVfxSheet[i] == NULL || m_placeholderVfxLeft[i] <= 0.0f)
            continue;

        float age = kPlaceholderVfxDuration[i] - m_placeholderVfxLeft[i];
        int frame = static_cast<int>(age * PLACEHOLDER_VFX_FPS);
        if (kPlaceholderVfxLoop[i])
            frame %= PLACEHOLDER_VFX_FRAME_COUNT;
        else if (frame >= PLACEHOLDER_VFX_FRAME_COUNT)
            frame = PLACEHOLDER_VFX_FRAME_COUNT - 1;  // hold the last frame instead of disappearing early

        SDL_Rect src = { (frame % PLACEHOLDER_VFX_SHEET_COLUMNS) * PLACEHOLDER_VFX_FRAME_SIZE,
            (frame / PLACEHOLDER_VFX_SHEET_COLUMNS) * PLACEHOLDER_VFX_FRAME_SIZE,
            PLACEHOLDER_VFX_FRAME_SIZE, PLACEHOLDER_VFX_FRAME_SIZE };

        // Travels in a straight line from where it was cast, at a fixed speed: 0 for every placeholder except
        // Deafening Blast, so this is a no-op (px/py == the cast position) for the other seven.
        float travelled = kPlaceholderVfxSpeed[i] * age;
        int px = m_placeholderVfxX[i] + static_cast<int>(m_placeholderVfxDirX[i] * travelled);
        int py = m_placeholderVfxY[i] + static_cast<int>(m_placeholderVfxDirY[i] * travelled);
        SDL_Rect dst = { px - PLACEHOLDER_VFX_DRAW_SIZE / 2, py - PLACEHOLDER_VFX_DRAW_SIZE / 2,
            PLACEHOLDER_VFX_DRAW_SIZE, PLACEHOLDER_VFX_DRAW_SIZE };

        if (kPlaceholderVfxRotates[i])
        {
            // The art is authored facing local +x ("forward"); rotating it to (dirX, dirY) points it at the enemy.
            // SDL's angle is degrees, clockwise, which is exactly atan2(dy, dx) in screen space (y grows downward).
            double angle = std::atan2(m_placeholderVfxDirY[i], m_placeholderVfxDirX[i]) * (180.0 / 3.14159265358979323846);
            SDL_RenderCopyEx(m_screen, m_placeholderVfxSheet[i], &src, &dst, angle, NULL, SDL_FLIP_NONE);
        }
        else
        {
            SDL_RenderCopy(m_screen, m_placeholderVfxSheet[i], &src, &dst);
        }
    }
}

// Current Q/W/E orbs and the two invoked spells (D = newest, F = previous). Never shows recipes or targets.
// Only while Playing: centred, it would otherwise sit under the Ready / Game Over text.
void GameManager::RenderInvokerHud()
{
    if (m_session.State() != practice::GameState::Playing)
        return;

    const invoker::InvokerState& inv = m_session.Invoker();

    // three orb sockets: empty ones are a faint ring, so the player sees how many orbs are loaded
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    for (int i = inv.OrbCount(); i < 3; ++i)
    {
        SDL_SetRenderDrawColor(m_screen, 200, 200, 210, 90);
        FillCircle(m_screen, elementPos[i].first, elementPos[i].second, 20);
        SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 140);
        FillCircle(m_screen, elementPos[i].first, elementPos[i].second, 17);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);
    for (int i = 0; i < inv.OrbCount(); ++i)
        RenderOrb(inv.GetOrb(i), elementPos[i].first, elementPos[i].second);

    // frames for the two slots, so an empty slot is visible too
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_screen, 0, 0, 0, 110);
    for (int i = 0; i < 2; ++i)
    {
        SDL_Rect frame = { skillPos[i].first - 2, skillPos[i].second - 2, 68, 68 };
        SDL_RenderFillRect(m_screen, &frame);
    }
    SDL_SetRenderDrawBlendMode(m_screen, SDL_BLENDMODE_NONE);

    m_keyD.SetPos(skillPos[0].first + 16, skillPos[0].second - 36);
    m_keyD.Render(m_screen);
    m_keyF.SetPos(skillPos[1].first + 16, skillPos[1].second - 36);
    m_keyF.Render(m_screen);

    invoker::SkillId spellD = inv.GetSlot(invoker::Slot::D);
    invoker::SkillId spellF = inv.GetSlot(invoker::Slot::F);
    if (spellD != invoker::SkillId::None)
    {
        Skill& icon = m_skillIcons[static_cast<int>(spellD)];
        icon.SetPos(skillPos[0].first, skillPos[0].second);
        icon.Render(m_screen);
    }
    if (spellF != invoker::SkillId::None)
    {
        Skill& icon = m_skillIcons[static_cast<int>(spellF)];
        icon.SetPos(skillPos[1].first, skillPos[1].second);
        icon.Render(m_screen);
    }
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

    // row 1: HP squares, score, combo, best combo
    pixeltext::DrawShadowed(m_screen, "HP", 16, 12, 2, white);
    for (int i = 0; i < st.maxHp; ++i)
    {
        SDL_Rect box = { 56 + i * 24, 10, 18, 18 };
        if (i < st.hp)
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
    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO START", SCREEN_WIDTH, 270, 3, white);
    pixeltext::DrawCentered(m_screen, "Q W E  ORBS     R  INVOKE     D F  CAST", SCREEN_WIDTH, 350, 2, grey);
    pixeltext::DrawCentered(m_screen, "ESC  QUIT", SCREEN_WIDTH, 385, 2, grey);
}

void GameManager::RenderGameOverScreen()
{
    const practice::Stats& st = m_session.GetStats();
    const SDL_Color white = { 255, 255, 255, 255 };
    const SDL_Color red = { 235, 70, 70, 255 };
    const SDL_Color grey = { 200, 200, 210, 255 };
    char buf[64];

    DimScreen(170);
    pixeltext::DrawCentered(m_screen, "GAME OVER", SCREEN_WIDTH, 80, 8, red);

    snprintf(buf, sizeof(buf), "SCORE %d", st.score);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 170, 3, white);
    snprintf(buf, sizeof(buf), "BEST COMBO %d", st.bestCombo);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 205, 3, white);
    char value[16];
    FormatAccuracy(value, sizeof(value), st);
    snprintf(buf, sizeof(buf), "ACCURACY %s", value);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 240, 3, white);
    FormatTime(value, sizeof(value), st.survivalTime);
    snprintf(buf, sizeof(buf), "SURVIVED %s", value);
    pixeltext::DrawCentered(m_screen, buf, SCREEN_WIDTH, 275, 3, white);

    char topBuf[128];
    int pos = snprintf(topBuf, sizeof(topBuf), "TOP 10");
    for (int i = 0; i < 10 && pos < static_cast<int>(sizeof(topBuf)); ++i)
        pos += snprintf(topBuf + pos, sizeof(topBuf) - pos, " %d", m_topScores[i]);
    pixeltext::DrawCentered(m_screen, topBuf, SCREEN_WIDTH, 312, 2, white);

    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO RESTART", SCREEN_WIDTH, 350, 3, white);
    pixeltext::DrawCentered(m_screen, "ESC  MENU", SCREEN_WIDTH, 395, 2, grey);
}

// Shows which skill the active enemy requires. Owner decision (2026-09-23): always on for every player,
// not just --debug builds; see GAMEPLAY_SPEC.md E-8 and the "Target-skill hint" rule in §17.
void GameManager::RenderTargetHint()
{
    const practice::ActiveEnemy& e = m_session.Enemy();
    if (!e.active)
        return;

    const SDL_Color yellow = { 255, 235, 60, 255 };
    char buf[64];
    snprintf(buf, sizeof(buf), "TARGET: %s", invoker::GetSkillDefinition(e.target).name);
    pixeltext::DrawShadowed(m_screen, buf, SCREEN_WIDTH - pixeltext::Width(buf, 2) - 16, 78, 2, yellow);
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
        backgroundSpeeds[i] = 0.1f * (i + 1); // Adjust speed as needed
    }
    return true;
}

void GameManager::renderBackgroundLayers() {
    for (int i = 0; i < 12; ++i) {
        SDL_Rect destRect = { static_cast<int>(backgroundPositions[i]), 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderCopy(m_screen, backgroundLayers[i], NULL, &destRect);

        // Render a second copy of the layer to create a seamless loop
        if (backgroundPositions[i] <= 0) {
            destRect.x = static_cast<int>(backgroundPositions[i]) + SCREEN_WIDTH;
            SDL_RenderCopy(m_screen, backgroundLayers[i], NULL, &destRect);
        }
        else {
            destRect.x = static_cast<int>(backgroundPositions[i]) - SCREEN_WIDTH;
            SDL_RenderCopy(m_screen, backgroundLayers[i], NULL, &destRect);
        }
    }
}

void GameManager::updateBackgroundLayers() {
    for (int i = 0; i < 12; ++i) {
        backgroundPositions[i] -= backgroundSpeeds[i];
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
    for (int i = 0; i < invoker::SKILL_COUNT; ++i)
    {
        if (m_placeholderVfxSheet[i] != NULL)
        {
            SDL_DestroyTexture(m_placeholderVfxSheet[i]);
            m_placeholderVfxSheet[i] = NULL;
        }
    }

    SDL_DestroyRenderer(m_screen);
    m_screen = NULL;

    SDL_DestroyWindow(m_window);
    m_window = NULL;

    IMG_Quit();
    SDL_Quit();
}