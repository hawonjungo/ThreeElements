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
#include "Practice/Tutorial.h"
#include "Practice/Boss.h"
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
// The layers are 928 x 793. They used to be squashed into 928 x 544; now a 544 px band is cut out unscaled instead.
// Row 729 of the source (the top of the ground) lands on y = 500 = practice::GROUND_LINE_Y, where it was before,
// so the enemies and the player still stand on the ground.
const int BACKGROUND_SOURCE_HEIGHT = 793;
const int BACKGROUND_CROP_Y = 229;

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

// The player: the owner's "Injoker" character (art/make_injoker.py). For now one standing picture that glides:
// it bobs gently over a spinning magic ring while the background scrolls, and the orbs the player has loaded
// circle it, Invoker-style (owner 2026-09-30, option A; a run cycle sprite sheet may replace the picture later).
const char* const PLAYER_SPRITE_PATH = "assets/player/injoker.png";
const int PLAYER_DRAW_H = 120;           // on-screen height (the file is 2x, drawn smoothed)
const int PLAYER_DRAW_X = 44;            // left edge; the feet stand on practice::GROUND_LINE_Y
const float PLAYER_BOB_PX = 3.0f;        // glide: up-and-down amplitude
const float PLAYER_BOB_SPEED = 3.5f;     // rad/s
// the magic ring under the feet and the orbit of the loaded orbs (an ellipse around the hat and shoulders)
const int PLAYER_RING_RX = 58;
const int PLAYER_RING_RY = 12;
const int PLAYER_ORBIT_Y = 404;
const int PLAYER_ORBIT_RX = 62;
const int PLAYER_ORBIT_RY = 14;
const float PLAYER_ORBIT_SPEED = 1.8f;   // rad/s
const int PLAYER_ORB_RADIUS = 9;
const float PLAYER_ORB_FLASH = 0.35f;    // s: the orbs flare when R invokes

// Centre of the player's visible body; shared by every effect drawn on the player (Ghost Walk, Alacrity...).
const int PLAYER_BODY_CENTER_X = 86;
const int PLAYER_BODY_CENTER_Y = 440;

// Ready screen logo (the same art as the app icon).
const char* const TITLE_LOGO_PATH = "assets/title/injoker_logo.png";
const int TITLE_LOGO_H = 170;

// Skill icons (assets/skill, 128 px RGBA made by art/make_skill_icons.py) are drawn scaled to these sizes.
const int SKILL_SLOT_SIZE = 64;     // D / F slots
const int SKILL_HINT_SIZE = 80;     // under "TARGET: <name>"
const int SKILL_RECIPE_SIZE = 40;   // recipe reference list

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
// Raised 344 -> 184 (owner 2026-09-30): mid-height on the left edge is where a thumb rests when a phone is held
// sideways, and the buttons no longer cover the Injoker character and its orbs at the bottom left.
const int TOUCH_CLUSTER_TOP  = 184;  // Q/W/E/R row's top edge; D/F row is one TOUCH_BUTTON_STEP below (ends y368)
// With the touch buttons on screen the orb row and the D/F slots move right by this much, clear of the buttons.
const int TOUCH_HUD_SHIFT_X = 160;
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
const SDL_Rect TOUCH_GAMEOVER_RESTART_RECT = { 264, 335, 400, 45 };// "PRESS ENTER TO RESTART"
const SDL_Rect TOUCH_GAMEOVER_MENU_RECT  = { 364, 385, 200, 30 };  // "ESC  MENU" (Game Over)
// Sound on/off button (all states), drawn under ACC on the left of the HUD; M toggles it on a keyboard.
const SDL_Rect SOUND_BUTTON_RECT = { 16, 66, 88, 24 };
// Recipe hint on/off while playing (G on a keyboard), under the sound button. Spec §17: off by default; a run
// that had it on is not ranked and sets no records.
const SDL_Rect HINT_BUTTON_RECT = { 16, 94, 88, 24 };
// Recipe reference (owner 2026-09-30): opened with H or this button on the Ready and Game Over screens, never
// while Playing (no recipe hints in play, spec §17). Any key or tap closes it.
// Main menu (the Ready screen, owner 2026-09-30): one option per line, arrows + Enter, hotkeys, or a tap.
enum MenuItem { MENU_PLAY, MENU_TUTORIAL, MENU_BOSS, MENU_RECIPES, MENU_LEADERBOARD, MENU_SOUND, MENU_HINT, MENU_QUIT };
const int MENU_X = 340;
const int MENU_Y = 238;
const int MENU_W = 248;
const int MENU_ITEM_H = 31;   // 8 lines (7 on the web) must end above the bottom edge
const int MENU_STEP = 35;
// Top 3 of the leaderboard beside the menu; a tap opens the top 10.
const SDL_Rect TOP3_PANEL_RECT = { 628, 250, 276, 150 };
const SDL_Rect LEADERBOARD_BUTTON_GAMEOVER_RECT = { 474, 430, 230, 36 };
// Tutorial (spec §24): its button on the Ready screen (T), the card panel at the top of the screen (the stats HUD
// is hidden in the tutorial), the NEXT button on cards and the two choices on the end card.
const SDL_Rect TUTORIAL_PANEL_RECT          = { 120, 8, 540, 128 };
const SDL_Rect TUTORIAL_NEXT_RECT           = { 120 + 540 - 172, 8 + 128 - 40, 160, 32 };
const SDL_Rect TUTORIAL_PLAY_RECT           = { 140, 8 + 128 - 40, 250, 32 };
const SDL_Rect TUTORIAL_MENU_RECT           = { 120 + 540 - 172, 8 + 128 - 40, 160, 32 };
const float TUTORIAL_WRONG_FLASH = 0.6f;    // s: the expected key flashes after a wrong one
const float TUTORIAL_SPAWN_FLASH = 3.0f;    // s: a new run enemy and its target are highlighted
const SDL_Rect RECIPES_BUTTON_GAMEOVER_RECT = { 224, 430, 230, 36 };
const SDL_Rect TOUCH_PLAYING_MENU_RECT   = { 780,  30, 132, 30 };  // "ESC  MENU" HUD reminder, top-right

// Boss mode (spec §25): the boss list (menu line BOSS FIGHTS / key B), the fight's HUD, the result screen.
const SDL_Rect BOSS_SELECT_PANEL = { 150, 30, SCREEN_WIDTH - 300, SCREEN_HEIGHT - 60 };
const int BOSS_SELECT_ROW_Y = 94;       // the first boss's row
const int BOSS_SELECT_ROW_H = 108;
const int BOSS_SELECT_ROW_STEP = 120;
const int BOSS_COMBO_TILE = 40;         // the combo strip at the top right: one icon tile per spell
const int BOSS_COMBO_GAP = 22;          // room for the ">" between two tiles
const int BOSS_COMBO_Y = 84;            // under "NEXT: ..." (y 64), clear of the orb row (y 150) even with the hint orbs
const int BOSS_HP_BAR_W = 220;
const float BOSS_DAMAGE_SHOW = 1.5f;    // s the damage of a combo stays next to the HP bar
const float BOSS_BAR_NOW_EARLY = 0.05f; // timing bars (hint): "NOW" from this long before the ideal moment ...
const float BOSS_BAR_NOW_LATE = 0.25f;  // ... until this long after it (casting then still scores GREAT)

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
	BaseObject m_titleLogo;
	float m_orbFlash = 0.0f;           // s left of the invoke flare on the orbiting orbs
	Skill m_skillIcons[invoker::SKILL_COUNT];  // icon sprites, indexed by invoker::SkillId
	Keyboard m_keyQ, m_keyW, m_keyE, m_keyR, m_keyD, m_keyF;  // key icons: orbs, invoke, slot labels; R was
	                                                          // unused until the touch buttons needed an icon
	EnemyObject m_enemySprites[practice::ENEMY_TYPE_COUNT];  // one sprite sheet per enemy definition, loaded once
	SDL_Texture* m_tornadoSheet = NULL;                      // Tornado spell effect, loaded once (NULL = not available)
	SDL_Texture* m_ghostWalkSheet = NULL;                    // Ghost Walk aura, loaded once (NULL = not available)
	float m_ghostWalkLeft = 0.0f;                            // seconds of aura left; 0 = no aura

	skillvfx::Effect m_skillVfx[invoker::SKILL_COUNT] = {};  // code-drawn skill effects, one per SkillId (left <= 0 = off)
	int m_skillVfxCount = 0;                                   // casts so far, seeds each effect's particles

	practice::TopRun m_topRuns[practice::TOP_RUNS] = {};  // this device's best runs by survival time, best first
	int m_lastRank = 0;                  // where the run that just ended landed in m_topRuns (0 = not listed)
	int m_menuIndex = 0;                 // highlighted line of the main menu
	bool m_showLeaderboard = false;      // the top 10 is open (Ready / Game Over)
	practice::BestStats m_bests = { 0, 0, 0.0f };       // persistent records (spec §13), saved like m_topRuns
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
	bool m_audioReady = false;            // an audio device was opened
	bool m_recipeHint = false;            // show the target's recipe above its icon (saved with the settings)
	bool m_showRecipes = false;           // the recipe reference is open (Ready / Game Over, and the tutorial run)
	practice::TutorialSession m_tutorial; // spec §24; only meaningful while m_tutorialActive
	bool m_tutorialActive = false;        // the tutorial is on screen (the Practice session waits in Ready)
	bool m_tutorialDone = false;          // finished once on this machine / browser (saved with the settings)
	float m_tutorialWrongFlash = 0.0f;
	float m_tutorialSpawnFlash = 0.0f;
	practice::BossSession m_boss;          // spec §25; only meaningful while m_bossActive
	bool m_bossActive = false;             // a boss fight (or its result screen) is on screen
	bool m_showBossSelect = false;         // the boss list is open
	int m_bossSelect = 0;                  // highlighted boss in the list
	float m_bossBest[practice::BOSS_COUNT] = {};  // best fight time per boss in s (0 = not beaten yet), saved
	bool m_bossNewBest = false;            // the fight that just ended set its boss's best time
	float m_bossDamageLeft = 0.0f;         // s left of the "-60%" next to the boss's HP bar
	int m_bossDamageShown = 0;
	char m_bossComboText[24] = "";         // "COMBO -60%" rising above the boss (FloatText keeps a pointer)     // touch device (web media query) or any finger touch seen; PC keeps it off

	// Sound for a key the Core has handled (orb, invoke, cast whoosh), the Ghost Walk aura and the skill effect;
	// shared by Practice and the tutorial. The judged outcome (right / wrong) is handled by OnCastJudged.
	void PresentInvokerResult(invoker::InputAction action, const invoker::InvokerResult& result,
		practice::CastOutcome cast, bool hadEnemy, const practice::Bounds& enemyBody);

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
	void HandlePointerDown(int x, int y, bool& quit);
	void TouchToGame(float normX, float normY, int& gameX, int& gameY);  // SDL finger position -> game pixels  // touch (SDL_FINGERDOWN) and mouse (SDL_MOUSEBUTTONDOWN)
	void PressEnterAction();          // shared by the Enter key and its touch tap zones
	void PressEscapeAction(bool& quit); // shared by the Esc key and its touch tap zones
	void ProcessAction(invoker::InputAction action);
	void LogUpdate(const practice::UpdateResult& result);

	void LogOutcome(practice::CastOutcome outcome);
	bool LoadTornadoSheet();
	bool LoadGhostWalkSheet();
	void StartSkillVfx(invoker::SkillId skill, bool hadEnemy, const practice::Bounds& enemyBody);
	void ResetVisualEffects();
	void LoadTopRuns();
	void SaveTopRuns();
	int SubmitRun(const practice::Stats& st);  // rank 1..10 on this device, or 0
	void LoadBests();
	void SaveBests(const practice::BestStats& bests);
	void SaveRecordsSoFar();      // app going to the background (Android): save records reached so far
	void LoadSettings();          // sound on/off, saved like the records
	void SaveSettings();
	void ToggleMute();
	void ToggleRecipeHint();
	void EndSession();            // a session ended (Game Over or Esc while Playing): update and save the records

	void OnCastJudged(practice::CastOutcome outcome, const practice::Bounds& enemy);  // "+1" / "MISS" feedback
	void OnLeak(int lostHeartIndex);
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
	int HudShiftX() const { return m_showTouchControls ? TOUCH_HUD_SHIFT_X : 0; }  // orb row / D-F slots
	// main menu and leaderboard
	int MenuItemCount() const;
	MenuItem MenuItemAt(int index) const;
	SDL_Rect MenuItemRect(int index) const;
	void ActivateMenuItem(MenuItem item, bool& quit);
	void RenderMenu();
	void RenderTop3Panel();
	void RenderLeaderboard();
	void RenderPlayer();          // magic ring, orbiting orbs and the gliding Injoker
	// tutorial (spec §24)
	void StartTutorial();
	void ExitTutorial(bool startPractice);
	void HandleTutorialKey(SDL_Keycode sym, const SDL_Event& e);
	bool HandleTutorialPointer(int x, int y);  // true = the tap was used
	void ProcessTutorialAction(invoker::InputAction action);
	void RenderTutorial();
	void RenderHighlight(SDL_Rect rect);        // pulsing gold frame around what the player should look at
	void RenderButton(const SDL_Rect& rect, const char* label, bool pulse);
	// boss mode (spec §25)
	void StartBoss(int boss);
	void ExitBoss();                            // back to the boss list
	void HandleBossSelectKey(SDL_Keycode sym);
	void HandleBossKey(SDL_Keycode sym, const SDL_Event& e);
	bool HandleBossPointer(int x, int y);
	void ProcessBossAction(invoker::InputAction action);
	void PresentBossUpdate(const practice::BossUpdateResult& result);
	void OnBossFail(practice::ComboFail reason);
	void BossText(const char* text, SDL_Color color, int raise = 0);  // rises above the boss, like "+1" / "MISS"
	void LoadBossTimes();
	void SaveBossTimes();
	SDL_Rect BossSelectRect(int index) const;
	practice::Bounds BossDrawnBody() const;     // the boss's body where it is drawn (raised while in the air)
	void RenderBoss();
	void RenderBossImpacts();
	void RenderBossHud();
	void RenderBossCombo();
	void RenderBossSelect();
	void RenderBossResult();
	// what the play view shows: the tutorial's invoker / enemy while it runs, the Practice session's otherwise
	// (in a boss fight: the fight's invoker, and no Practice enemy)
	const invoker::InvokerState& ShownInvoker() const
	{
		return m_tutorialActive ? m_tutorial.Invoker() : m_bossActive ? m_boss.Invoker() : m_session.Invoker();
	}
	const practice::ActiveEnemy& ShownEnemy() const
	{
		static const practice::ActiveEnemy none = {};
		return m_tutorialActive ? m_tutorial.Enemy() : m_bossActive ? none : m_session.Enemy();
	}
	bool IsPlayView() const
	{
		return m_tutorialActive || (m_bossActive && m_boss.State() == practice::BossState::Fighting)
			|| m_session.State() == practice::GameState::Playing;
	}
	SDL_Rect TargetHintArea(invoker::SkillId target, int& textX) const;  // where RenderTargetHint draws
	void RenderRecipesButton(const SDL_Rect& rect);
	void RenderRecipes();
	void RenderSmallOrb(invoker::Orb orb, int centerX, int centerY);
	void DimScreen(Uint8 alpha);
};

#endif
