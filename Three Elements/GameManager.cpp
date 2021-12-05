
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
    ImpTimer fps_timer;

    bool bBkgn = m_background.LoadImg("assets/bg.png", m_screen);
    bool bPlayer = m_player.LoadImg("assets/main.bmp", m_screen);

    EnemyObject* enemy1 = new EnemyObject();
    EnemyObject* enemy2 = new EnemyObject();
    bool bEnemy1 = enemy1->LoadImg("assets/mushroom_run.png", m_screen);
                   enemy2->LoadImg("assets/ene1.png", m_screen);

                   m_Enemylist.push_back(enemy1);
                   m_Enemylist.push_back(enemy2);
                   
    if (bEnemy1)
    {
        for (int i = 0; i < m_Enemylist.size(); i++)
        {
            m_Enemylist[i]->set_clips();
            m_Enemylist[i]->SetPos(800, 375);
            m_Enemylist[i]->SetVal(5, 0);
        }              
    }

    

 

    if (bPlayer)
    {
        m_player.set_clips();
        m_player.SetPos(10, 375);
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

    // nho giai phong vung nho cho list enemy (tim hieu cach giai phong vung nho cho 1 vector )
}