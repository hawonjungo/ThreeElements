
#include "GameManager.h"
#include "MainObject.h"
#include "ImpTimer.h"


GameManager* GameManager::instance_ = NULL;


GameManager::GameManager()
{
    m_window = NULL;
    m_screen = NULL;
}

bool GameManager::InitSDL()
{
    bool success = true;
    int ret = SDL_Init(SDL_INIT_VIDEO);
    if (ret < 0)
        return false;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    m_window = SDL_CreateWindow("Title Game",
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

    // keyboard
    Keyboard* keyQ = new Keyboard();
    Keyboard* keyW = new Keyboard();
    Keyboard* keyE = new Keyboard();
    Keyboard* keyR = new Keyboard();
    Keyboard* keyD = new Keyboard();
    Keyboard* keyF = new Keyboard();

    bool bQ = keyQ->LoadImg("assets/keyboard/keyQ.png", m_screen);
    keyQ->SetType(Keyboard::KEY_Q);

    bool bW = keyW->LoadImg("assets/keyboard/keyW.png", m_screen);
    keyW->SetType(Keyboard::KEY_W);

    bool bE = keyE->LoadImg("assets/keyboard/keyE.png", m_screen);
    keyE->SetType(Keyboard::KEY_E);

    bool bR = keyR->LoadImg("assets/keyboard/keyR.png", m_screen);
    keyR->SetType(Keyboard::KEY_R);

    bool bD = keyD->LoadImg("assets/keyboard/keyD.png", m_screen);
    keyD->SetType(Keyboard::KEY_D);

    bool bF = keyF->LoadImg("assets/keyboard/keyF.png", m_screen);
    keyF->SetType(Keyboard::KEY_F);

    bool bKey = bQ & bW & bE & bR & bD & bF;

    m_Keylist.push_back(keyQ);
    m_Keylist.push_back(keyW);
    m_Keylist.push_back(keyE);
    m_Keylist.push_back(keyR);
    m_Keylist.push_back(keyD);
    m_Keylist.push_back(keyF);

    // Skill
    Skill* sTonado = new Skill();
    Skill* sGreenVolt = new Skill();
    Skill* sFreezing = new Skill();

    bool bSTonado = sTonado->LoadImg("assets/skill/Tonado.png", m_screen);
    bool bGreenVolt = sGreenVolt->LoadImg("assets/skill/GreenVolt.png", m_screen);
    bool bFreeza = sFreezing->LoadImg("assets/skill/Freezing.png", m_screen);

    bool bSkill = bSTonado & bGreenVolt & bFreeza;

    if (bSkill)
    {
        m_Skilllist.push_back(sTonado);
        m_Skilllist.push_back(sGreenVolt);
        m_Skilllist.push_back(sFreezing);
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
    
    // setup when player click and it show up later
    if (bKey)
    {
        int sp = 0;
        int spSkill = 0;
        for (int i = 0; i < m_Keylist.size(); i++)
        {           
            if (i < 3) {
                m_Keylist[i]->set_clips();
                m_Keylist[i]->SetPos((50 + sp), 150);
            }
            else if (i == 3) {
                m_Keylist[i]->set_clips();
                m_Keylist[i]->SetPos(250, 150);
            }
            else {
                m_Keylist[i]->set_clips();
                m_Keylist[i]->SetPos(150+spSkill , 200);
                spSkill += 50;
            }           
            sp += 50;        
        }
    }

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
            m_player.keyHandle(m_event);
        }
        //Clear screen
        SDL_SetRenderDrawColor(m_screen, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(m_screen);
   
        // render background
        if (bBkgn)
        {
            m_background.render(m_screen, NULL);
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
            int keyDown = m_player.GetKeyPress();
            for (int i = 0; i < m_Keylist.size(); i++)
            {
                int keyType = m_Keylist.at(i)->GetType();
                if (keyDown  == keyType)
                {
                    m_Keylist[i]->Render(m_screen);
                }   
                else if (keyType == Keyboard::KEY_D || keyType == Keyboard::KEY_F)
                {
                    m_Keylist[i]->Render(m_screen);
                }
            }
        }

        if (bSkill == true)
        {
            int keyQNum = m_player.GetQKey();
            int keyWNum = m_player.GetWKey();
            int keyENum = m_player.GetEKey();
            int keyRState = m_player.GetRState();

            if (keyQNum == 3 && keyRState == true)
            {
                for (int i = 0; i < m_Skilllist.size(); i++)
                {
                    int keyType = m_Skilllist.at(i)->GetType();
                    if (keyType == Skill::SKILL_TONADO)
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
            else if (keyWNum == 3 && keyRState == true)
            {
                for (int i = 0; i < m_Skilllist.size(); i++)
                {
                    int keyType = m_Skilllist.at(i)->GetType();
                    if (keyType == Skill::SKILL_GREEN_VOLT)
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
}