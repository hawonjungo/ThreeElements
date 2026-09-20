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
#include "Core/Invoker.h"
#include "Practice/Practice.h"
#include <vector>
using namespace std;
//Screen dimension constants
const int SCREEN_WIDTH = 928;
const int SCREEN_HEIGHT = 544;

const int FRAME_PER_SECOND = 25;  // fps

const int RENDER_DRAW_COLOR = 0Xff;

// y of the ground the enemies run on: an enemy's lowest visible pixel (EnemyDefinition::feetRow) is drawn here
const int GROUND_LINE_Y = 500;

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
	Skill m_skillIcons[invoker::SKILL_COUNT];  // icon sprites, indexed by invoker::SkillId
	Keyboard m_keyQ, m_keyW, m_keyE, m_keyD, m_keyF;  // key icons: orbs (Q/W/E) and slot labels (D/F)
	EnemyObject m_enemySprites[practice::ENEMY_TYPE_COUNT];  // one sprite sheet per enemy definition, loaded once

	practice::PracticeSession m_session;  // the Practice Mode rules: enemy, HP, score, combo, accuracy, difficulty
	bool m_debug = false;                 // --debug: also print the enemy's target skill (development only)

	vector<pair<int, int>> elementPos = { {50, 150}, {100, 150}, {150, 150} };
	vector<pair<int, int>> skillPos = { {150,250},{250,250} };

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

	void SetDebug(bool on) { m_debug = on; }

	bool InitSDL();
	void LoopGame();
	void Close();

private:
	void HandleKeyDown(const SDL_Event& e, bool& quit);
	void ProcessAction(invoker::InputAction action);
	void StartSession();
	void LogUpdate(const practice::UpdateResult& result);

	Keyboard* KeyIcon(invoker::Orb orb);
	void RenderEnemy();
	void RenderInvokerHud();
	void RenderStatsHud();
	void RenderReadyScreen();
	void RenderGameOverScreen();
	void RenderDebugOverlay();
	void DimScreen(Uint8 alpha);
};

#endif
