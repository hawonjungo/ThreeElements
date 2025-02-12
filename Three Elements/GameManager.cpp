
#include "GameManager.h"
#include "MainPlayer.h"
#include "Skill.h"
#include "ImpTimer.h"
#include <algorithm>


GameManager* GameManager::instance_ = NULL;


GameManager::GameManager()
{
    m_window = NULL;
    m_screen = NULL;

}

GameManager::~GameManager()
{
    for (auto& pair : skillMap) {
        delete pair.second; 
    }
}

bool GameManager::InitSDL()
{
    bool success = true;
    int ret = SDL_Init(SDL_INIT_VIDEO);
    if (ret < 0)
        return false;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    m_window = SDL_CreateWindow("Three Elements",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN);

    if (m_window == NULL)
    {
        success = false;
    }
    else
    {
        m_screen = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (m_screen == NULL)
            success = false;
        else
        {
            SDL_SetRenderDrawColor(m_screen, RENDER_DRAW_COLOR, RENDER_DRAW_COLOR, RENDER_DRAW_COLOR, RENDER_DRAW_COLOR);
            int imgFlags = IMG_INIT_PNG;
            if (!(IMG_Init(imgFlags) && imgFlags))
                success = false;
        }

        /*
        * // prepare for sound
        if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
        {
            success = false;
        }
        


        g_sound_bullet[0] = Mix_LoadWAV(g_name_audio_bullet_main1);
        g_sound_bullet[1] = Mix_LoadWAV(g_name_audio_bullet_main2);
        g_sound_explosion = Mix_LoadWAV(g_name_audio_ex_main);
        g_sound_ex_main = Mix_LoadWAV(g_name_audio_ex_threats);

        if (g_sound_bullet[0] == NULL || g_sound_bullet[1] == NULL || g_sound_explosion == NULL)
        {
            return false;
        }
     

        if (TTF_Init() == -1)
        {
            success = false;
        }

        
         // prepare for font text
        font_time = TTF_OpenFont("font//dlxfont.ttf", 15);
        if (font_time == NULL)
        {
            success = false;
        }
        
        */
        
        
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

    //background
    bool bBkgn = m_background.LoadImg("assets/bg.png", m_screen);
    bool bPlayer = m_player.LoadImg("assets/main.bmp", m_screen);

    
    // enemy
    
    //  Generate and load 10 random enemies
    //for (int i = 0; i < enemyPaths.size(); ++i) {
    //    EnemyObject* enemy = new EnemyObject();
    //   
    //     bool  bEnemy = enemy->LoadImg(enemyPaths[i], m_screen);
    //     m_Enemylist.push_back(enemy);
    //    if (bEnemy) {
    //        enemy->set_clips();
    //        enemy->SetPos(800 + i * 50, 395); // Adjust position as needed
    //        enemy->SetVal(5, 0);
    //       
    //    }
    //}


   /* EnemyObject* enemy1 = new EnemyObject();
    EnemyObject* enemy2 = new EnemyObject();
    EnemyObject* enemy3 = new EnemyObject();
    EnemyObject* enemy4 = new EnemyObject();
    bool bEnemy1 = enemy1->LoadImg("assets/mushroom_run.png", m_screen);
                   enemy2->LoadImg("assets/goblin_run.png", m_screen);
                   enemy3->LoadImg("assets/flight.png", m_screen);
                   enemy4->LoadImg("assets/flight.png", m_screen);
                   m_Enemylist.push_back(enemy1);
                   m_Enemylist.push_back(enemy2);
                   m_Enemylist.push_back(enemy3);
                   m_Enemylist.push_back(enemy4);*/
    // Skill
    m_skill.initializeSpellMap();
    // Generate and map Skill objects using a loop
    for (const auto& pair : m_skill.spellMap) {
        skillMap[pair.first] = new Skill();
    }
    Skill* sNO_SPELL = new Skill();
    Skill* sCOLD_SNAP = new Skill();
    Skill* sGHOST_WALK = new Skill();
    Skill* sICE_WALL = new Skill();
    Skill* sEMP = new Skill();
    Skill* sTORNADO = new Skill();
    Skill* sALACRITY = new Skill();
    Skill* sSUN_STRIKE = new Skill();
    Skill* sFORGE_SPIRIT = new Skill();
    Skill* sCHAOS_METEOR = new Skill();
    Skill* sDEAFENING_BLAST = new Skill();

    // keyboard
    Keyboard* keyQ = new Keyboard();
    Keyboard* keyW = new Keyboard();
    Keyboard* keyE = new Keyboard();
    Keyboard* keyR = new Keyboard();
    Keyboard* keyD = new Keyboard();
    Keyboard* keyF = new Keyboard();

    map<char, Keyboard*> keyMap = {
        {'Q', keyQ},
        {'W', keyW}, 
        {'E', keyE}
    };

    keyQ->LoadImg("assets/keyboard/keyQ.png", m_screen);
    keyW->LoadImg("assets/keyboard/keyW.png", m_screen);
    keyE->LoadImg("assets/keyboard/keyE.png", m_screen);
    keyR->LoadImg("assets/keyboard/keyR.png", m_screen);
    keyD->LoadImg("assets/keyboard/keyD.png", m_screen);
    keyF->LoadImg("assets/keyboard/keyF.png", m_screen);


    skillMap["QQQ"]->LoadImg("assets/skill/ColdSnap.png", m_screen);
    skillMap["QQW"]->LoadImg("assets/skill/GhostWalk.png", m_screen);
    skillMap["EQQ"]->LoadImg("assets/skill/IceWall.png", m_screen);
    skillMap["WWW"]->LoadImg("assets/skill/EMP.png", m_screen);
    skillMap["QWW"]->LoadImg("assets/skill/Tornado.png", m_screen);
    skillMap["EWW"]->LoadImg("assets/skill/Alacrity.png", m_screen);
    skillMap["EEE"]->LoadImg("assets/skill/SunStrike.png", m_screen);
    skillMap["EEQ"]->LoadImg("assets/skill/ForgeSpirit.png", m_screen);
    skillMap["EEW"]->LoadImg("assets/skill/Meteor.png", m_screen);
    skillMap["EQW"]->LoadImg("assets/skill/Blast.png", m_screen);

    //bool bCOLD_SNAP = sCOLD_SNAP->LoadImg("assets/skill/ColdSnap.png", m_screen);
    //bool bTORNADO = sTORNADO->LoadImg("assets/skill/Tonado.png", m_screen);
    //bool bICE_WALL = sICE_WALL->LoadImg("assets/skill/Freezing.png", m_screen);
    //bool bCOLD_SNAP = sCOLD_SNAP->LoadImg("assets/skill/GreenVolt.png", m_screen);
    //bool bTORNADO = sTORNADO->LoadImg("assets/skill/Tonado.png", m_screen);
    //bool bICE_WALL = sICE_WALL->LoadImg("assets/skill/Freezing.png", m_screen);
    //bool bCOLD_SNAP = sCOLD_SNAP->LoadImg("assets/skill/GreenVolt.png", m_screen);
    //bool bTORNADO = sTORNADO->LoadImg("assets/skill/Tonado.png", m_screen);
    //bool bICE_WALL = sICE_WALL->LoadImg("assets/skill/Freezing.png", m_screen);



    Skill* skillSlotD;
    Skill* skillSlotF;
	// change the true to false to disable the skill - later
    if (true)
    {
        m_Skilllist.push_back(sTORNADO);
        m_Skilllist.push_back(sCOLD_SNAP);
        m_Skilllist.push_back(sICE_WALL);
    }
          
    if (bPlayer)
    {
        m_player.set_clips();
        m_player.SetPos(10, 385);         
    }

                   
   /* if (bEnemy)
    {
        int sp = 0;
        int spSkill = 0;
        for (int i = 0; i < m_Enemylist.size(); i++)
        {
            m_Enemylist[i]->set_clips();
            m_Enemylist[i]->SetPos(800+sp, 395);
            m_Enemylist[i]->SetVal(5, 0);
            sp += 50;
            

            
        }              
    }*/
    
   
    SDL_Rect rect_D;
    SDL_Rect rect_F;
    // Need D and F rect
    for (int i = 0; i < m_Keylist.size(); i++)
    {
        int type = m_Keylist[i]->GetType();
        if (type == Keyboard::KEY_D)
        {
            rect_D = m_Keylist[i]->getRect();
        }
        else if (type == Keyboard::KEY_F)
        {
            rect_F = m_Keylist[i]->getRect();
        }
    }
    Uint32 lastRespawnTime = SDL_GetTicks() - 5000;
    Uint32 respawnInterval = 5000; // Initial respawn interval in milliseconds
   
    bool bStop = false;
    // ====================== render here !!!
    while (!bStop)
    {
        fps_timer.start();
        //Handle events on queue
        while (SDL_PollEvent(&m_event) != 0)
        {
            //User requests quit
            if (m_event.type == SDL_QUIT)
            {
               
                bStop = true;
            }
            //m_player.keyHandle(m_event);
            m_player.handleKeyPress(m_event);
        }
        //Clear screen
        SDL_SetRenderDrawColor(m_screen, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(m_screen);

        // Update and render background layers
        updateBackgroundLayers();
        renderBackgroundLayers();

        // Declare variables area
        string eleCombo = m_player.getElementComb();
        string eleComboR = m_player.getElementComb();

        // render background
        //if (bBkgn)
        //{
        //    m_background.render(m_screen,NULL);
        //}

        if (bPlayer)
        {
            m_player.Render(m_screen);
        }
        // Update and render enemies
        for (int i = 0; i < m_Enemylist.size(); ++i) {
            m_Enemylist[i]->Render(m_screen);
            m_Enemylist[i]->UpdatePos();

            // Check collision with player
            //if (checkCollision(m_player.getRect(), m_Enemylist[i]->getRect())) {
            //    bStop = true; // Stop the game if an enemy touches the player
            //}
        }

        /*for (int i = 0; i < m_Enemylist.size(); i++)
        {
            m_Enemylist[i]->Render(m_screen);
            m_Enemylist[i]->UpdatePos();

          
            
        }*/
  
        if (true)
        {          
            
            for (int i = 0; i < eleCombo.size(); ++i) {        

                char ele = eleCombo[i];
                int x = elementPos[i].first;
                int y = elementPos[i].second;
                               
                if (keyMap.find(ele) != keyMap.end()) {
                    Keyboard* key = keyMap[ele];
                    key->Render(m_screen);
                    key->set_clips();
                    key->SetPos(x, y);
                }
            }           
           
        }
        if (m_player.m_KeyRActive)
        {
            
                // Assuming you have a method to render the active spell
                m_player.skill.Render(m_screen);
                if (m_player.m_KeyRActive) {     
                    if (!m_player.slotD.empty() && m_player.slotD != m_player.slotF) {

                        skillSlotD = skillMap[m_player.slotD];
                        skillSlotD->Render(m_screen);
                        skillSlotD->set_clips();
                        skillSlotD->SetPos(skillPos[0].first, skillPos[0].second);

                    }
                    if (!m_player.slotF.empty()) {
                        skillSlotF = skillMap[m_player.slotF];
                        skillSlotF->Render(m_screen);
                        skillSlotF->set_clips();
                        skillSlotF->SetPos(skillPos[1].first, skillPos[1].second);

                    }
                       
                   
                   
                }
             
               


        }
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastRespawnTime >= respawnInterval) {
            respawnEnemy();
            lastRespawnTime = currentTime;
           // respawnInterval = std::max(1000U, respawnInterval - 500); // Decrease interval, minimum 1 second
        }
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

    // Clean memories
    for (int i = 0; i < m_Keylist.size(); i++)
    {
        Keyboard* p = m_Keylist[i];
        if (p != NULL)
        {
            delete p;
            p = NULL;
        }      
    }
    m_Keylist.clear();

    // Cleam for skill----------------------

    Close();
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
void GameManager::respawnEnemy() {
    int randomIndex = std::rand() % enemyPaths.size();
    EnemyObject* enemy = new EnemyObject();
    bool bEnemy = enemy->LoadImg(enemyPaths[randomIndex], m_screen);
    if (bEnemy) {
    
        enemy->set_clips();
        enemy->SetPos(800, 395); // Adjust position as needed
        enemy->SetVal(5, 0);

        m_Enemylist.push_back(enemy);
    }
    else {
        delete enemy;
    }
}
void GameManager::Close()
{
    //g_background.Free();

    SDL_DestroyRenderer(m_screen);
    m_screen = NULL;

    SDL_DestroyWindow(m_window);
    m_window = NULL;

    IMG_Quit();
    SDL_Quit();

    // cHECK TO CLEAR MEMORY FOR A VECTOR !!!
    /*
    
     m_background.free();
    m_player.free();

    for (int i = 0; i < m_Enemylist.size(); i++) {
        m_Enemylist[i]->free();
        delete m_Enemylist[i];
        m_Enemylist[i] = NULL;
    }
    m_Enemylist.clear();

    for (int i = 0; i < m_Keylist.size(); i++) {
        m_Keylist[i]->free();
        delete m_Keylist[i];
        m_Keylist[i] = NULL;
    }
    m_Keylist.clear();

    for (int i = 0; i < m_Skilllist.size(); i++) {
        m_Skilllist[i]->free();
        delete m_Skilllist[i];
        m_Skilllist[i] = NULL;
    }
    m_Skilllist.clear();

    SDL_DestroyRenderer(m_screen);
    m_screen = NULL;

    SDL_DestroyWindow(m_window);
    m_window = NULL;

    IMG_Quit();
    SDL_Quit();
    
    */
}