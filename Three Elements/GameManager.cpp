
#include "GameManager.h"
#include "MainPlayer.h"
#include "Skill.h"
#include "ImpTimer.h"
#include "PixelText.h"
#include <ctime>


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

    // key icons: the orbs (Q/W/E) and the slot labels (D/F)
    Keyboard* keyIcons[] = { &m_keyQ, &m_keyW, &m_keyE, &m_keyD, &m_keyF };
    const char* keyPaths[] = {
        "assets/keyboard/keyQ.png", "assets/keyboard/keyW.png", "assets/keyboard/keyE.png",
        "assets/keyboard/keyD.png", "assets/keyboard/keyF.png" };
    for (int i = 0; i < 5; ++i)
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

    if (bPlayer)
    {
        m_player.set_clips();
        m_player.SetPos(10, 385);
    }

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
        }

        // Practice rules: enemy movement, spawning, leaks, Game Over (input of this frame is already applied)
        LogUpdate(m_session.Update(dt));

        //Clear screen
        SDL_SetRenderDrawColor(m_screen, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(m_screen);

        // Update and render background layers
        updateBackgroundLayers();
        renderBackgroundLayers();

        if (bPlayer)
        {
            m_player.Render(m_screen);
        }
        RenderEnemy();
        RenderTornadoes();  // above the background, player and enemy, below the HUD
        RenderInvokerHud();
        RenderStatsHud();
        RenderDebugOverlay();

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
    if (sym == SDLK_ESCAPE)
    {
        // Ready: quit the application. Playing / Game Over: step back to Ready, never quit.
        if (m_session.PressEscape())
            quit = true;
        else
            printf("[practice] back to Ready\n");
        return;
    }
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER)
    {
        unsigned seed = static_cast<unsigned>(std::time(NULL)) ^ (SDL_GetTicks() << 8);
        if (m_session.PressEnter(seed))  // Ready / Game Over: new session (ignored while Playing)
            printf("[practice] session started (seed %u)\n", seed);
        return;
    }

    invoker::InputAction action;
    if (MainPlayer::TranslateKey(e, action))
        ProcessAction(action);
}

// Feeds one logical key to the session and prints the development log.
void GameManager::ProcessAction(invoker::InputAction action)
{
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

    if (r.tornadoLaunched)
        printf("[practice] Tornado launched (judged when it hits the enemy)\n");
    LogOutcome(r.cast);
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
    }
}

// ------------------------------------------------------------------ drawing

Keyboard* GameManager::KeyIcon(invoker::Orb orb)
{
    switch (orb)
    {
    case invoker::Orb::Quas: return &m_keyQ;
    case invoker::Orb::Wex:  return &m_keyW;
    default:                 return &m_keyE;
    }
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

// Current Q/W/E orbs and the two invoked spells (D = newest, F = previous). Never shows recipes or targets.
void GameManager::RenderInvokerHud()
{
    const invoker::InvokerState& inv = m_session.Invoker();

    for (int i = 0; i < inv.OrbCount(); ++i)
    {
        Keyboard* key = KeyIcon(inv.GetOrb(i));
        key->SetPos(elementPos[i].first, elementPos[i].second);
        key->Render(m_screen);
    }

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

    pixeltext::DrawCentered(m_screen, "PRESS ENTER TO RESTART", SCREEN_WIDTH, 350, 3, white);
    pixeltext::DrawCentered(m_screen, "ESC  MENU", SCREEN_WIDTH, 395, 2, grey);
}

// Development only (--debug): shows which skill the active enemy requires. Off in normal play.
void GameManager::RenderDebugOverlay()
{
    const practice::ActiveEnemy& e = m_session.Enemy();
    if (!m_debug || !e.active)
        return;

    const SDL_Color yellow = { 255, 235, 60, 255 };
    char buf[64];
    snprintf(buf, sizeof(buf), "DEBUG TARGET: %s", invoker::GetSkillDefinition(e.target).name);
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

    SDL_DestroyRenderer(m_screen);
    m_screen = NULL;

    SDL_DestroyWindow(m_window);
    m_window = NULL;

    IMG_Quit();
    SDL_Quit();
}