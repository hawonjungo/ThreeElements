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
#include "SkillVfx.h"
#include "Core/Invoker.h"
#include "Practice/Practice.h"
#include <vector>
using namespace std;
//Screen dimension constants
const int SCREEN_WIDTH = 928;
const int SCREEN_HEIGHT = 544;

const int FRAME_PER_SECOND = 60;  // native frame cap; the web build follows the display (requestAnimationFrame).
                                  // Was 25 while animation and scrolling counted frames; all of it runs on dt now.
const float MAX_FRAME_DT = 0.1f;  // s; a longer gap (tab hidden, debugger pause) is treated as 0.1 s

// Parallax scrolling in real time; reproduces the old per-frame speed at 25 FPS (layer i moved 0.1 * (i + 1) px
// per frame). The enemy walk-cycle speed is ENEMY_ANIM_FPS in Enemy.h.
const float BACKGROUND_LAYER_SPEED = 2.5f;  // px/s for layer 0; layer i scrolls at (i + 1) times this

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

// Ghost Walk aura (presentation only, Practice does not know about it): a swirl ring drawn around the player for a
// fixed time after a Ghost Walk cast. The 5 x 5 sheet (256 px frames) starts with a humanoid silhouette that turns
// into the ring; only the pure ring frames (15..24) are used, so no character is ever drawn.
const char* const GHOST_WALK_SHEET_PATH = "assets/Skills/Ghost/Ghost Walk-spritesheet.png";
const int GHOST_WALK_SHEET_COLUMNS = 5;
const int GHOST_WALK_FRAME_SIZE = 256;
const int GHOST_WALK_FIRST_FRAME = 15;
const int GHOST_WALK_FRAME_COUNT = 10;
const float GHOST_WALK_FPS = 10.0f;
const float GHOST_WALK_DURATION = 3.0f;  // seconds; a new cast starts the time again
const int GHOST_WALK_DRAW_SIZE = 128;    // px on screen
const Uint8 GHOST_WALK_ALPHA = 150;      // the ring's centre is a solid swirl: see-through, so the player stays clearly visible

// Centre of the player's visible body (the sprite is drawn at (10, 385)); shared by every effect drawn on the player.
const int PLAYER_BODY_CENTER_X = 121;
const int PLAYER_BODY_CENTER_Y = 476;

// The other 8 skills are drawn in code by SkillVfx.* (owner 2026-09-29; they replaced the TEST placeholder sheets).

// Touch controls (mobile web): six buttons for Q/W/E/R/D/F, drawn with the six existing keyboard icons
// (Keyboard class, assets/keyboard/*.png, already-loaded 2-frame 32x32 sheets) at a larger on-screen size.
// No new art, no new input system: SDL_FINGERDOWN and SDL_MOUSEBUTTONDOWN both hit-test this same table and
// feed the result through the same ProcessAction() path the keyboard already uses.
// Layout (owner decision 2026-09-24, moved to the left side + raised + enlarged 2026-09-25): Q/W/E/R sit in
// one row exactly like the top row of a physical keyboard (evenly spaced, touching gaps only); D/F sit in a
// second row directly below, shifted right by half a key step so D lines up under E/R and F under R,
// mirroring the real keyboard's home-row stagger. The cluster sits bottom-left.
// Enlarged 72 -> 88 px (2026-09-23, owner: finger-sized); now that the orb/slot HUD sits in the centre, the
// cluster can drop down to the bottom edge. Only drawn/hit-tested on touch devices (m_showTouchControls).
const int TOUCH_BUTTON_SIZE = 88;
const int TOUCH_BUTTON_GAP = 8;
const int TOUCH_BUTTON_STEP = TOUCH_BUTTON_SIZE + TOUCH_BUTTON_GAP;  // 96: centre-to-centre spacing within a row
const int TOUCH_CLUSTER_LEFT = 16;   // Q's left edge; E/R's row spans TOUCH_CLUSTER_LEFT .. +3*STEP+SIZE
const int TOUCH_CLUSTER_TOP  = 344;  // Q/W/E/R row's top edge; D/F row is one TOUCH_BUTTON_STEP below (ends y528)
// Element colours, indexed by invoker::Orb (owner 2026-09-23): Quas = ice, Wex = lightning, Exort = fire. Used
// for the HUD orbs and the colour band on the Q/W/E touch buttons, so an active orb is recognisable at a glance.
const SDL_Color kOrbColors[3] =
{
	{  90, 200, 255, 255 },  // Quas: ice blue
	{ 190, 100, 255, 255 },  // Wex: electric violet
	{ 255, 120,  30, 255 },  // Exort: fire orange
};
struct TouchButton { invoker::InputAction action; SDL_Rect rect; };
const TouchButton kTouchButtons[6] =
{
	{ invoker::InputAction::Q, { TOUCH_CLUSTER_LEFT + 0 * TOUCH_BUTTON_STEP,                        TOUCH_CLUSTER_TOP,                    TOUCH_BUTTON_SIZE, TOUCH_BUTTON_SIZE } },
	{ invoker::InputAction::W, { TOUCH_CLUSTER_LEFT + 1 * TOUCH_BUTTON_STEP,                        TOUCH_CLUSTER_TOP,                    TOUCH_BUTTON_SIZE, TOUCH_BUTTON_SIZE } },
	{ invoker::InputAction::E, { TOUCH_CLUSTER_LEFT + 2 * TOUCH_BUTTON_STEP,                        TOUCH_CLUSTER_TOP,                    TOUCH_BUTTON_SIZE, TOUCH_BUTTON_SIZE } },
	{ invoker::InputAction::R, { TOUCH_CLUSTER_LEFT + 3 * TOUCH_BUTTON_STEP,                        TOUCH_CLUSTER_TOP,                    TOUCH_BUTTON_SIZE, TOUCH_BUTTON_SIZE } },
	{ invoker::InputAction::D, { TOUCH_CLUSTER_LEFT + 2 * TOUCH_BUTTON_STEP + TOUCH_BUTTON_STEP / 2, TOUCH_CLUSTER_TOP + TOUCH_BUTTON_STEP, TOUCH_BUTTON_SIZE, TOUCH_BUTTON_SIZE } },
	{ invoker::InputAction::F, { TOUCH_CLUSTER_LEFT + 3 * TOUCH_BUTTON_STEP + TOUCH_BUTTON_STEP / 2, TOUCH_CLUSTER_TOP + TOUCH_BUTTON_STEP, TOUCH_BUTTON_SIZE, TOUCH_BUTTON_SIZE } },
};
// Generous tap zones over the existing Enter/Esc text/HUD reminder, so a touch-only player can start, restart,
// back out and quit without a keyboard. Same idea as kTouchButtons: reuse what is already drawn, not new UI.
const SDL_Rect TOUCH_READY_START_RECT    = { 264, 250, 400, 45 };  // "PRESS ENTER TO START"
const SDL_Rect TOUCH_READY_QUIT_RECT     = { 364, 375, 200, 30 };  // "ESC  QUIT"
const SDL_Rect TOUCH_GAMEOVER_RESTART_RECT = { 264, 335, 400, 45 };// "PRESS ENTER TO RESTART"
const SDL_Rect TOUCH_GAMEOVER_MENU_RECT  = { 364, 385, 200, 30 };  // "ESC  MENU" (Game Over)
// Sound on/off button (all states), drawn under ACC on the left of the HUD; M toggles it on a keyboard.
const SDL_Rect SOUND_BUTTON_RECT = { 16, 66, 88, 24 };
const SDL_Rect TOUCH_PLAYING_MENU_RECT   = { 780,  30, 132, 30 };  // "ESC  MENU" HUD reminder, top-right

// Hit / miss / leak feedback (presentation only, owner 2026-09-28: kept light). Seconds unless noted.
const float FEEDBACK_TEXT_TIME = 0.7f;    // "+1" / "MISS" rise and vanish
const float FEEDBACK_TEXT_RISE = 40.0f;   // px travelled upward over that time
const float FEEDBACK_BURST_TIME = 0.4f;   // expanding gold ring where an enemy was defeated
const float FEEDBACK_ENEMY_FLASH = 0.3f;  // enemy tinted red after a wrong cast
const float FEEDBACK_LEAK_FLASH = 0.5f;   // red frame around the screen after a leak
const float FEEDBACK_SHAKE_TIME = 0.3f;   // screen shake after a leak
const int   FEEDBACK_SHAKE_PX = 4;        // shake amplitude
const float FEEDBACK_HP_BLINK = 0.8f;     // the HP square that was just lost blinks
const int   FEEDBACK_MAX_TEXTS = 8;
const int   FEEDBACK_MAX_BURSTS = 4;

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
	Keyboard m_keyQ, m_keyW, m_keyE, m_keyR, m_keyD, m_keyF;  // key icons: orbs, invoke, slot labels; R was
	                                                          // unused until the touch buttons needed an icon
	EnemyObject m_enemySprites[practice::ENEMY_TYPE_COUNT];  // one sprite sheet per enemy definition, loaded once
	SDL_Texture* m_tornadoSheet = NULL;                      // Tornado spell effect, loaded once (NULL = not available)
	SDL_Texture* m_ghostWalkSheet = NULL;                    // Ghost Walk aura, loaded once (NULL = not available)
	float m_ghostWalkLeft = 0.0f;                            // seconds of aura left; 0 = no aura

	skillvfx::Effect m_skillVfx[invoker::SKILL_COUNT] = {};  // code-drawn skill effects, one per SkillId (left <= 0 = off)
	int m_skillVfxCount = 0;                                   // casts so far, seeds each effect's particles

	int m_topScores[10] = {};  // highest scores this browser/machine has seen, highest first
	practice::BestStats m_bests = { 0, 0, 0.0f };       // persistent records (spec §13), saved like m_topScores
	practice::BestUpdate m_lastBestUpdate = { false, false, false };  // records beaten by the session that just ended

	// feedback effects (see FEEDBACK_* above); a slot with left <= 0 is free
	struct FloatText { float left; int x; int y; const char* text; SDL_Color color; };
	struct Burst { float left; int x; int y; };
	FloatText m_floatTexts[FEEDBACK_MAX_TEXTS] = {};
	Burst m_bursts[FEEDBACK_MAX_BURSTS] = {};
	float m_enemyFlashLeft = 0.0f;
	float m_leakFlashLeft = 0.0f;
	float m_shakeLeft = 0.0f;
	float m_hpBlinkLeft = 0.0f;
	int m_hpBlinkIndex = -1;  // which HP square blinks (the one just lost)

	practice::PracticeSession m_session;  // the Practice Mode rules: enemy, HP, score, combo, accuracy, difficulty
	bool m_debug = false;                 // --debug: also print the enemy's target skill (development only)
	Uint32 m_lastTick = 0;                // SDL_GetTicks() at the previous frame, for dt
	bool m_hasPlayer = false;             // player sprite loaded
	bool m_showTouchControls = false;
	bool m_audioReady = false;            // an audio device was opened     // touch device (web media query) or any finger touch seen; PC keeps it off

	// Invoker HUD, centred horizontally (owner 2026-09-23): orb centres (40 px discs) and the D/F slot icons'
	// top-left corners (64 x 64). Both groups are symmetric around SCREEN_WIDTH / 2 = 464.
	vector<pair<int, int>> elementPos = { {412, 170}, {464, 170}, {516, 170} };
	vector<pair<int, int>> skillPos = { {382,250},{482,250} };

public:
	static GameManager* getInstace()
	{
		if (instance_ == NULL)
			instance_ = new GameManager();
		return instance_;
	}
	bool loadBackgroundLayers();
	void renderBackgroundLayers();
	void updateBackgroundLayers(float dt);

	void SetDebug(bool on) { m_debug = on; }

	bool InitSDL();
	void LoopGame();
	void Close();

private:
	void LoadAssets();
	bool RunFrame();  // one frame of input, rules and drawing; false once quit was requested
	void HandleKeyDown(const SDL_Event& e, bool& quit);
	void HandlePointerDown(int x, int y, bool& quit);  // touch (SDL_FINGERDOWN) and mouse (SDL_MOUSEBUTTONDOWN)
	void PressEnterAction();          // shared by the Enter key and its touch tap zones
	void PressEscapeAction(bool& quit); // shared by the Esc key and its touch tap zones
	void ProcessAction(invoker::InputAction action);
	void LogUpdate(const practice::UpdateResult& result);

	void LogOutcome(practice::CastOutcome outcome);
	bool LoadTornadoSheet();
	bool LoadGhostWalkSheet();
	void StartSkillVfx(invoker::SkillId skill, bool hadEnemy, const practice::Bounds& enemyBody);
	void ResetVisualEffects();
	void LoadTopScores();
	void SaveTopScores();
	bool SubmitScore(int score);  // true if it entered the top 10
	void LoadBests();
	void SaveBests();
	void LoadSettings();          // sound on/off, saved like the records
	void SaveSettings();
	void ToggleMute();
	void EndSession();            // a session ended (Game Over or Esc while Playing): update and save the records

	void OnCastJudged(practice::CastOutcome outcome, const practice::Bounds& enemy);  // "+1" / "MISS" feedback
	void OnLeak();
	void UpdateFeedback(float dt);
	void RenderFeedback();        // rings and floating text, in the scene
	void RenderLeakFlash();       // red frame, over everything but the HUD text

	void RenderOrb(invoker::Orb orb, int centerX, int centerY);
	void RenderEnemy();
	void RenderTornadoes();
	void RenderGhostWalk();
	void RenderSkillVfx();
	void RenderTouchControls();
	void RenderInvokerHud();
	void RenderStatsHud();
	void RenderReadyScreen();
	void RenderGameOverScreen();
	void RenderTargetHint();
	void RenderSoundButton();
	void DimScreen(Uint8 alpha);
};

#endif
