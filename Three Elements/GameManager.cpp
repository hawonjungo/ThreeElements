
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
        */

        /*
        g_sound_bullet[0] = Mix_LoadWAV(g_name_audio_bullet_main1);
        g_sound_bullet[1] = Mix_LoadWAV(g_name_audio_bullet_main2);
        g_sound_explosion = Mix_LoadWAV(g_name_audio_ex_main);
        g_sound_ex_main = Mix_LoadWAV(g_name_audio_ex_threats);

        if (g_sound_bullet[0] == NULL || g_sound_bullet[1] == NULL || g_sound_explosion == NULL)
        {
            return false;
        }
        */

        /*if (TTF_Init() == -1)
        {
            success = false;
        }*/

        /*
        * // prepare for font text
        font_time = TTF_OpenFont("font//dlxfont.ttf", 15);
        if (font_time == NULL)
        {
            success = false;
        }
        */
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
    EnemyObject* enemy1 = new EnemyObject();
    EnemyObject* enemy2 = new EnemyObject();
    bool bEnemy1 = enemy1->LoadImg("assets/mushroom_run.png", m_screen);
                   enemy2->LoadImg("assets/goblin_run.png", m_screen);
                   m_Enemylist.push_back(enemy1);
                   m_Enemylist.push_back(enemy2);
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

    bool bQ = keyQ->LoadImg("assets/keyboard/keyQ.png", m_screen);
    bool bW = keyW->LoadImg("assets/keyboard/keyW.png", m_screen);
    bool bE = keyE->LoadImg("assets/keyboard/keyE.png", m_screen);
    bool bR = keyR->LoadImg("assets/keyboard/keyR.png", m_screen);
    bool bD = keyD->LoadImg("assets/keyboard/keyD.png", m_screen);
    bool bF = keyF->LoadImg("assets/keyboard/keyF.png", m_screen);

    bool bKey = bQ && bW && bE && bR && bD && bF;

    skillMap["QQQ"]->LoadImg("assets/skill/GreenVolt.png", m_screen);
    skillMap["QWW"]->LoadImg("assets/skill/Tonado.png", m_screen);
    skillMap["EQQ"]->LoadImg("assets/skill/Freezing.png", m_screen);
    bool bCOLD_SNAP = sCOLD_SNAP->LoadImg("assets/skill/GreenVolt.png", m_screen);
    bool bTORNADO = sTORNADO->LoadImg("assets/skill/Tonado.png", m_screen);
    bool bICE_WALL = sICE_WALL->LoadImg("assets/skill/Freezing.png", m_screen);

    bool bSkill = bTORNADO && bCOLD_SNAP && bICE_WALL;

 


    if (bSkill)
    {
        m_Skilllist.push_back(sTORNADO);
        m_Skilllist.push_back(sCOLD_SNAP);
        m_Skilllist.push_back(sICE_WALL);
    }
          
    if (bPlayer)
    {
        m_player.set_clips();
        m_player.SetPos(10, 375);         
    }

                   
    if (bEnemy1)
    {
        int sp = 0;
        int spSkill = 0;
        for (int i = 0; i < m_Enemylist.size(); i++)
        {
            m_Enemylist[i]->set_clips();
            m_Enemylist[i]->SetPos(800+sp, 385);
            m_Enemylist[i]->SetVal(5, 0);
            sp += 50;
            

            m_Skilllist[i]->set_clips();
            m_Skilllist[i]->SetPos(700+spSkill, 150);
            spSkill += 100;
            
        }              
    }
    
    // setup when player click D & F later 
                //m_Keylist[i]->set_clips();
                //m_Keylist[i]->SetPos(150+spSkill , 200);
                //spSkill += 50;
          

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
   

        // Declare variables area
        string eleCombo = m_player.getElementComb();
        

        // render background
        if (bBkgn)
        {
            m_background.render(m_screen,NULL);
        }

        if (bPlayer)
        {
            m_player.Render(m_screen);
        }


        for (int i = 0; i < m_Enemylist.size(); i++)
        {
            m_Enemylist[i]->Render(m_screen);
            m_Enemylist[i]->UpdatePos();

            m_Skilllist[i]->Render(m_screen);
            
        }
  
        if (bKey)
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
        if (bSkill)
        {
            sort(eleCombo.begin(), eleCombo.end());
            for (int i = 0; i < 2; i++) {
                int x = skillPos[i].first;
                int y = skillPos[i].second;
                if (skillMap.find(eleCombo) != skillMap.end()) {
                    Skill* skill = skillMap[eleCombo];
                    skill->Render(m_screen);
                    skill->set_clips();
                    skill->SetPos(x, y);
                }
            }

            
  /*          if (m_skill.spellMap.find(eleCombo) != m_skill.spellMap.end()) {
                sTORNADO->Render(m_screen);
                rect_D = sTORNADO->getRect();
                int xp = rect_D.x;
                int yp = rect_D.y + rect_D.h + 20;
                sTORNADO->SetPos(xp, yp);
                sTORNADO->Render(m_screen);
            }*/


            int keyQNum = m_player.GetQKey();
            int keyWNum = m_player.GetWKey();
            int keyENum = m_player.GetEKey();
            int keyRState = m_player.GetRState();

            if (keyQNum == 3 && keyRState == true)
            {
                for (int i = 0; i < m_Skilllist.size(); i++)
                {
                    int keyType = m_Skilllist.at(i)->GetType();
                    if (keyType == TORNADO)
                    {
                        if (m_Skilllist[i]->GetActive() == true)
                        {
                            // Second time, D change to false
                            m_Skilllist[i]->Render(m_screen);
                        }
                        else
                        {
                            // First time
                            int xp = rect_D.x;
                            int yp = rect_D.y + rect_D.h + 20;
                            m_Skilllist[i]->SetPos(xp, yp);
                            m_Skilllist[i]->Render(m_screen);

                            // reset reset
                            m_player.ResetQR();
                        }
                    }

                }
            }
            else if (keyWNum == 3 )
            {
                for (int i = 0; i < m_Skilllist.size(); i++)
                {
                    int keyType = m_Skilllist.at(i)->GetType();
                    if (keyType == COLD_SNAP)
                    {
                        if (m_Skilllist[i]->GetActive() == true)
                        {
                            // Second time, D change to false
                            m_Skilllist[i]->Render(m_screen);
                        }
                        else
                        {
                            // First time
                            int xp = rect_D.x;
                            int yp = rect_D.y + rect_D.h + 20;
                            m_Skilllist[i]->SetPos(xp, yp);
                            m_Skilllist[i]->Render(m_screen);

                            // reset reset
                            m_player.ResetWR();
                        }
                    }
                }
                
            }
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