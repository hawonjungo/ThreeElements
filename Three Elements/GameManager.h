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

// TEST placeholders for the 8 skills that have no real effect yet (Practice judges every cast as it always
// has; these are presentation only, same loading/animation approach as Tornado and Ghost Walk: one 4 x 4, 128 px,
// 16-frame sheet per skill). One entry per invoker::SkillId; NULL for the two skills that already have their own
// dedicated effect (Tornado, Ghost Walk). Not a new system: same struct-free table style as kEnemies/keyPaths.
const char* const kPlaceholderVfxPath[invoker::SKILL_COUNT] =
{
	"assets/Skills/ColdSnap/coldsnap_vfx_16f.png",              // ColdSnap
	NULL,                                                        // GhostWalk (own dedicated aura)
	"assets/Skills/IceWall/icewall_vfx_16f.png",                // IceWall
	"assets/Skills/EMP/emp_vfx_16f.png",                        // EMP
	NULL,                                                        // Tornado (own dedicated projectile)
	"assets/Skills/Alacrity/alacrity_vfx_16f.png",              // Alacrity
	"assets/Skills/SunStrike/sunstrike_vfx_16f.png",            // SunStrike
	"assets/Skills/ForgeSpirit/forgespirit_vfx_16f.png",        // ForgeSpirit
	"assets/Skills/ChaosMeteor/chaosmeteor_vfx_16f.png",        // ChaosMeteor
	"assets/Skills/DeafeningBlast/deafeningblast_vfx_16f.png",  // DeafeningBlast
};
// How long each placeholder stays on screen. 1.6 s plays the 16 frames once at 10 fps; longer durations hold the
// last frame afterwards (Ice Wall keeps standing, the impact glow lingers a moment). Alacrity loops for 3 s.
const float kPlaceholderVfxDuration[invoker::SKILL_COUNT] = { 1.6f, 0.0f, 3.0f, 1.8f, 0.0f, 3.0f, 1.8f, 1.6f, 2.0f, 1.4f };
const bool  kPlaceholderVfxLoop[invoker::SKILL_COUNT]     = { false, false, false, false, false, true, false, false, false, false };
// true = drawn on the player (self-cast effects); false = drawn where the enemy was when the skill was cast.
// Forge Spirit joins Deafening Blast here now that it travels (a projectile has to start at the player).
const bool  kPlaceholderVfxAtPlayer[invoker::SKILL_COUNT] = { false, false, false, false, false, true, false, true, false, true };
// px/s a placeholder travels away from where it started, along the direction captured at cast time; 0 = stays
// put. Standardized (2026-09-24) to Tornado's own TORNADO_SPEED (Practice.h): every projectile-style effect
// in the game now travels at the same 700 px/s, instead of each placeholder guessing its own number.
// Forge Spirit ('simple fire projectile') now actually travels, matching its description; it did not before.
const float kPlaceholderVfxSpeed[invoker::SKILL_COUNT]    = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 700.0f, 0.0f, 700.0f };
// true = the sprite is rotated (SDL_RenderCopyEx) to face its travel direction instead of drawn upright.
const bool  kPlaceholderVfxRotates[invoker::SKILL_COUNT]  = { false, false, false, false, false, false, false, true, false, true };

const int PLACEHOLDER_VFX_SHEET_COLUMNS = 4;
const int PLACEHOLDER_VFX_FRAME_SIZE = 128;
const int PLACEHOLDER_VFX_FRAME_COUNT = 16;
const float PLACEHOLDER_VFX_FPS = 10.0f;
const int PLACEHOLDER_VFX_DRAW_SIZE = 140;  // px on screen

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
const SDL_Rect TOUCH_PLAYING_MENU_RECT   = { 780,  30, 132, 30 };  // "ESC  MENU" HUD reminder, top-right

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

	// TEST placeholders (see kPlaceholderVfxPath above), one slot per SkillId; NULL / 0 = nothing to draw.
	SDL_Texture* m_placeholderVfxSheet[invoker::SKILL_COUNT] = {};
	float m_placeholderVfxLeft[invoker::SKILL_COUNT] = {};
	int m_placeholderVfxX[invoker::SKILL_COUNT] = {};   // origin (where it was cast from)
	int m_placeholderVfxY[invoker::SKILL_COUNT] = {};
	float m_placeholderVfxDirX[invoker::SKILL_COUNT] = {};  // unit vector, captured once at cast time (no homing)
	float m_placeholderVfxDirY[invoker::SKILL_COUNT] = {};

	int m_topScores[10] = {};  // highest scores this browser/machine has seen, highest first

	practice::PracticeSession m_session;  // the Practice Mode rules: enemy, HP, score, combo, accuracy, difficulty
	bool m_debug = false;                 // --debug: also print the enemy's target skill (development only)
	bool m_showTouchControls = false;     // touch device (web media query) or any finger touch seen; PC keeps it off

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
	void updateBackgroundLayers();

	void SetDebug(bool on) { m_debug = on; }

	bool InitSDL();
	void LoopGame();
	void Close();

private:
	void HandleKeyDown(const SDL_Event& e, bool& quit);
	void HandlePointerDown(int x, int y, bool& quit);  // touch (SDL_FINGERDOWN) and mouse (SDL_MOUSEBUTTONDOWN)
	void PressEnterAction();          // shared by the Enter key and its touch tap zones
	void PressEscapeAction(bool& quit); // shared by the Esc key and its touch tap zones
	void ProcessAction(invoker::InputAction action);
	void LogUpdate(const practice::UpdateResult& result);

	void LogOutcome(practice::CastOutcome outcome);
	bool LoadTornadoSheet();
	bool LoadGhostWalkSheet();
	bool LoadPlaceholderVfxSheets();
	void StartPlaceholderVfx(invoker::SkillId skill, bool hadEnemy, const practice::Bounds& enemyBody);
	void ResetVisualEffects();
	void LoadTopScores();
	void SaveTopScores();
	bool SubmitScore(int score);  // true if it entered the top 10

	void RenderOrb(invoker::Orb orb, int centerX, int centerY);
	void RenderEnemy();
	void RenderTornadoes();
	void RenderGhostWalk();
	void RenderPlaceholderVfx();
	void RenderTouchControls();
	void RenderInvokerHud();
	void RenderStatsHud();
	void RenderReadyScreen();
	void RenderGameOverScreen();
	void RenderTargetHint();
	void DimScreen(Uint8 alpha);
};

#endif
