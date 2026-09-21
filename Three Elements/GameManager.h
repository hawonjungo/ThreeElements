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

// The play field and the ground line belong to the Practice rules (practice::FIELD_*, practice::GROUND_LINE_Y).
static_assert(SCREEN_WIDTH == practice::FIELD_WIDTH && SCREEN_HEIGHT == practice::FIELD_HEIGHT,
	"the Practice play field must match the window");

// Tornado spell effect: one sprite sheet, 4 x 4 frames of 128 x 128 pixels, read left-to-right then top-to-bottom
// (frame f is at column f % 4, row f / 4). The frame count and animation speed are in Practice.h.
const char* const TORNADO_SHEET_PATH = "assets/Skills/Tornado/tornado_vfx_16f.png";
const int TORNADO_SHEET_COLUMNS = 4;
const int TORNADO_FRAME_SIZE = 128;
const float TORNADO_DRAW_SCALE = 0.5f;  // drawn 64 x 64: an exact 2:1 reduction of the pixel art, same on both axes
static_assert(TORNADO_SHEET_COLUMNS * TORNADO_SHEET_COLUMNS == practice::TORNADO_FRAME_COUNT, "4 x 4 sheet = 16 frames");

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
	MainPlayer m_player;
	Skill m_skillIcons[invoker::SKILL_COUNT];  // icon sprites, indexed by invoker::SkillId
	Keyboard m_keyQ, m_keyW, m_keyE, m_keyD, m_keyF;  // key icons: orbs (Q/W/E) and slot labels (D/F)
	EnemyObject m_enemySprites[practice::ENEMY_TYPE_COUNT];  // one sprite sheet per enemy definition, loaded once
	SDL_Texture* m_tornadoSheet = NULL;                      // Tornado spell effect, loaded once (NULL = not available)

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
	void LogUpdate(const practice::UpdateResult& result);

	void LogOutcome(practice::CastOutcome outcome);
	bool LoadTornadoSheet();

	Keyboard* KeyIcon(invoker::Orb orb);
	void RenderEnemy();
	void RenderTornadoes();
	void RenderInvokerHud();
	void RenderStatsHud();
	void RenderReadyScreen();
	void RenderGameOverScreen();
	void RenderDebugOverlay();
	void DimScreen(Uint8 alpha);
};

#endif
