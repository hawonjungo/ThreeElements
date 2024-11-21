#pragma once
#ifndef GAME_MANAGER_H_
#define GAME_MANAGER_H_

//Using SDL, SDL_image, standard IO, and strings
#include"Define.h"

#include "MainPlayer.h"
#include "BaseObject.h"
#include "Enemy.h"
#include "Keyboard.h"
#include "Skill.h"
#include <vector>
using namespace std;
//Screen dimension constants
const int SCREEN_WIDTH = 928;
const int SCREEN_HEIGHT = 544;

const int FRAME_PER_SECOND = 25;  // fps

const int RENDER_DRAW_COLOR = 0Xff;

class GameManager
{
private:
    static GameManager* instance_;
    GameManager();
    ~GameManager();
protected:
    SDL_Window* m_window;
    SDL_Renderer* m_screen;
    SDL_Event m_event;

    // declare object
    BaseObject m_background;
    MainPlayer m_player;
    Skill m_skill;
    //EnemyObject m_Enemy;  // temp, could be use to set x and random value later
   
    
    vector<Keyboard*> m_Keylist;
    vector<EnemyObject*> m_Enemylist;
    vector<Skill*> m_Skilllist;
    vector<pair<int, int>> elementPos = { {50, 150}, {100, 150}, {150, 150} };
    vector<pair<int, int>> skillPos = { {150,250},{250,250} };

    map<string, Skill*> skillMap;

public:
    static GameManager* getInstace()
    {
        if (instance_ == NULL)
            instance_ = new GameManager();
        return instance_;
    }
    
    bool InitSDL();
    void LoopGame();
    void Close();
};

#endif