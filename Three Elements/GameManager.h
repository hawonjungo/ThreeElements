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

	SDL_Texture* backgroundLayers[12];
	float backgroundPositions[12];
	float backgroundSpeeds[12];
protected:
	SDL_Window* m_window;
	SDL_Renderer* m_screen;
	SDL_Event m_event;

	// declare object
	BaseObject m_background;
	MainPlayer m_player;
	Skill m_skill;
	

	vector<Keyboard*> m_Keylist;
	vector<EnemyObject*> m_Enemylist;
	// Existing members...
	std::vector<EnemyObject*> activeEnemies;
	vector<int> availableEnemy;
	Uint32 lastRespawnTime;
	bool isEnemyOnScreen = false;

	const Uint32 respawnInterval = 5000; // 5 seconds
	vector<Skill*> m_Skilllist;
	vector<pair<int, int>> elementPos = { {50, 150}, {100, 150}, {150, 150} };
	vector<pair<int, int>> skillPos = { {150,250},{250,250} };

	map<string, Skill*> skillMap;

	const vector<pair<string, int>> enemyPaths = {
		  {"assets/enemies/mushroom_run.png", 8},
		  {"assets/enemies/goblin_run.png", 8},
		  {"assets/enemies/eyes_fly.png", 8},
		  {"assets/enemies/skeleton.png", 4},

		  {"assets/enemies/fire_wiz.png", 8},
		  {"assets/enemies/nec_walk.png", 10},
	  	  {"assets/enemies/worm_run.png", 9}


	};
public:
	static GameManager* getInstace()
	{
		if (instance_ == NULL)
			instance_ = new GameManager();
		return instance_;
	}
	bool loadBackgroundLayers();
	void renderBackgroundLayers();
	void updateBackgroundLayers();

	void respawnEnemy(Uint32 currentTime);

	bool InitSDL();
	void LoopGame();
	bool EnemyDistance(int x, int y, int minDistance);
	void Close();
};

#endif