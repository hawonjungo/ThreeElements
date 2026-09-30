#pragma once
#ifndef PRACTICE_H_
#define PRACTICE_H_

// Practice Mode rules (GAMEPLAY_SPEC.md sections 7-15 and 20).
//
// Sits on top of the Invoker Core and below the SDL presentation. No SDL, no rendering, no clock and no
// globals: time is passed in as `dt` (seconds) and the random seed is passed in, so everything here is
// deterministic and testable without a window (Tests/PracticeTests.cpp).
//
// One session = one play from Start() to Game Over. There is exactly one active enemy at a time; it
// requires one skill (its `targetSkill`). Casting that skill removes it, any other skill does nothing,
// and an enemy that reaches the player costs 1 HP.

#include "../Core/Invoker.h"

#include <vector>

namespace practice
{
	// ---- play field (logical pixels of the 928x544 window) ----
	// Enemy positions are the x of the enemy's visible body, left edge (see EnemyDefinition::bodyLeft).
	const float SPAWN_X = 990.0f;        // sprite starts fully off-screen on the right
	const float HIT_LINE_X = 140.0f;     // an enemy whose body reaches this x has reached the player
	const float MAX_FRAME_TIME = 0.1f;   // dt is clamped to this so a hitch never teleports an enemy
	const int   START_HP = 3;

	// The window the game is drawn in (GameManager.h asserts that it matches SCREEN_WIDTH / SCREEN_HEIGHT).
	constexpr float FIELD_WIDTH = 928.0f;
	constexpr float FIELD_HEIGHT = 544.0f;
	const float GROUND_LINE_Y = 500.0f;  // y of the ground the enemies run on: their feetRow is drawn here

	// Where spells leave the player: the front of the player's body, at the hand holding the staff. The Injoker
	// sprite (assets/player/injoker.png) is drawn 82 x 120 px with its left edge at x 44 and its feet on the ground
	// (GameManager.h, PLAYER_DRAW_*), so it spans x 44..126, y 378..498; the staff hand is near (120, 450).
	const float PLAYER_CAST_X = 128.0f;
	const float PLAYER_CAST_Y = 450.0f;

	// ---- enemies ----
	const int ENEMY_TYPE_COUNT = 10;     // one visual identity per skill

	struct EnemyDefinition
	{
		int id;                          // equals its index in the table
		const char* name;                // for logs / the debug overlay only
		const char* sprite;              // asset path (data only, no textures here)
		int frames;                      // frames in the horizontal sprite sheet
		int bodyLeft;                    // first visible column of a drawn frame (after the horizontal flip)
		int feetRow;                     // row of the sprite that is drawn on GROUND_LINE_Y
		int bodyWidth;                   // visible body: width, and first / last visible row of a frame
		int bodyTop;                     //   (union over all animation frames); together with bodyLeft this is
		int bodyBottom;                  //   the hit box that spells collide with
		invoker::SkillId targetSkill;    // the skill this enemy requires; data, not derived from the sprite
		float speedMultiplier;           // applied on top of the difficulty speed
	};

	const EnemyDefinition& GetEnemyDefinition(int index);  // 0 <= index < ENEMY_TYPE_COUNT

	// ---- difficulty: a plain deterministic function of the elapsed survival time ----
	// INITIAL MVP TUNING VALUES. They are a starting point for playtesting, not balanced or final.
	// Bounds: the speed never exceeds MAX_ENEMY_SPEED and the delay never drops below MIN_CHALLENGE_DELAY.
	const float START_ENEMY_SPEED = 125.0f;   // px/s (the speed the game had before Practice Mode)
	const float MAX_ENEMY_SPEED = 380.0f;
	const float ENEMY_SPEED_GROWTH = 2.5f;    // px/s gained per second survived
	const float START_CHALLENGE_DELAY = 1.5f; // seconds between one challenge ending and the next enemy appearing
	const float MIN_CHALLENGE_DELAY = 0.5f;
	const float CHALLENGE_DELAY_DECAY = 0.01f;  // seconds lost per second survived

	struct DifficultyParams
	{
		float enemySpeed;      // px/s
		float challengeDelay;  // seconds
	};

	DifficultyParams DifficultyAt(float elapsedSeconds);

	// ---- session ----
	enum class GameState { Ready, Playing, GameOver };
	enum class CastOutcome { None, Correct, Incorrect };  // None = not a judged cast

	struct Stats
	{
		int hp;
		int maxHp;
		int score;
		int combo;          // current combo, reset by every new session
		int bestCombo;      // record: kept across restarts; seeded from saved data with RestoreBestCombo()
		int correctCasts;
		int incorrectCasts;
		float survivalTime; // seconds spent Playing
		bool assisted;      // the recipe hint was on at some point in this run (spec §17): not ranked, no records
		int gold;           // PLAY: gold earned in this run (elites, bosses, Bounty); Survival: always 0
		int kills;          // enemies defeated
		int bossesDefeated; // PLAY: stage = bossesDefeated + 1

		int TotalCasts() const { return correctCasts + incorrectCasts; }
		double Accuracy() const  // 0.0 .. 1.0; 0.0 while no cast has been judged yet
		{
			int total = TotalCasts();
			return total > 0 ? static_cast<double>(correctCasts) / static_cast<double>(total) : 0.0;
		}
	};

	// Persistent records (spec §13, P-7). Practice only compares; loading and saving them is the presentation
	// layer's job (a file on desktop, localStorage on the web).
	struct BestStats
	{
		int score;
		int combo;
		float survivalTime;  // seconds
	};
	struct BestUpdate  // which records a finished session beat (strictly greater; a tie is not a new record)
	{
		bool score;
		bool combo;
		bool survivalTime;
		bool Any() const { return score || combo || survivalTime; }
	};
	// Raises each record in `bests` that `session` beat and reports which ones changed.
	BestUpdate MergeBests(BestStats& bests, const Stats& session);

	// Leaderboard (owner 2026-09-30): players are ranked by **survival time**. Practice only orders runs; where the
	// list lives (this device now, online boards later) is the presentation layer's business.
	const int TOP_RUNS = 10;
	struct TopRun
	{
		float survivalTime;  // seconds; 0 = empty slot
		int score;           // shown next to the time, and breaks ties
	};
	// Inserts `run` into `list` (TOP_RUNS entries, best first) and returns its rank 1..TOP_RUNS, or 0 when it did not
	// make the list (or survived no time at all). Longer time ranks higher; at equal time the higher score does; a run
	// equal to an existing entry goes below it.
	int InsertTopRun(TopRun* list, const TopRun& run);

	// ---- PLAY mode (spec §26): the main game. Survival rules plus chains, elites, bosses, runes and gold. ----
	// INITIAL TUNING VALUES, like the difficulty ones.
	enum class SessionMode { Survival, Play };      // Survival = the former Practice, unchanged
	enum class EnemyKind { Normal, Elite, Boss };
	enum class Rune { None, Regeneration, Frost, DoubleDamage, Bounty, Shield };  // the reward for a boss
	const int   PLAY_MAX_CHAIN = 3;
	const int   PLAY_BOSS_EVERY = 10;              // every 10th enemy of a PLAY run is a boss
	const float PLAY_ELITE_CHANCE_MAX = 0.3f;       // elites: 0 % at the start ...
	const float PLAY_ELITE_RAMP_TIME = 180.0f;      // ... rising to 30 % after this many seconds
	const float PLAY_ELITE_SPEED = 0.8f;            // speed multipliers (longer chains need more keys)
	const float PLAY_BOSS_SPEED = 0.6f;
	const float PLAY_ELITE_SCALE = 1.4f;            // drawn (and hit) this much larger
	const float PLAY_BOSS_SCALE = 2.0f;
	const int   PLAY_LEAK_NORMAL = 1;               // lives lost when it reaches the player
	const int   PLAY_LEAK_ELITE = 2;
	const int   PLAY_LEAK_BOSS = 3;
	const int   PLAY_POINTS_NORMAL = 1;             // score per kill
	const int   PLAY_POINTS_ELITE = 3;
	const int   PLAY_POINTS_BOSS = 10;
	const int   PLAY_GOLD_ELITE = 5;                // gold only from elites, bosses and Bounty
	const int   PLAY_GOLD_BOSS = 20;
	const int   PLAY_BOUNTY_GOLD = 25;
	const int   PLAY_MAX_HP = 5;                    // Regeneration never goes above this
	const float PLAY_FROST_TIME = 15.0f;            // s of Frost: enemies at PLAY_FROST_SPEED
	const float PLAY_FROST_SPEED = 0.6f;
	const float PLAY_DOUBLE_TIME = 20.0f;           // s of Double Damage: points x2

	float EliteChance(float elapsedSeconds);        // 0 .. PLAY_ELITE_CHANCE_MAX

	// PLAY leaderboard (by score; ties: more bosses, then longer time). Same list rules as InsertTopRun.
	struct PlayRun
	{
		int score;       // 0 = empty slot
		float time;      // seconds
		int stage;       // bosses defeated + 1
	};
	int InsertPlayRun(PlayRun* list, const PlayRun& run);  // list has TOP_RUNS entries; rank 1..TOP_RUNS or 0

	struct ActiveEnemy
	{
		bool active;
		int definition;                 // index into the enemy table
		invoker::SkillId target;        // the skill needed now (Survival: copied from the definition)
		float x;                        // body-left, logical pixels
		float speed;                    // px/s, fixed at spawn
		EnemyKind kind;                 // PLAY only; Survival enemies are Normal
		invoker::SkillId chain[PLAY_MAX_CHAIN];  // PLAY: the skills needed in order (chain[chainStep] == target)
		int chainLength;                // 1 for a normal enemy (0 is treated as 1)
		int chainStep;                  // skills of the chain already broken
		float scale;                    // drawn size and hit box (0 is treated as 1)
	};

	// What a correct cast that finished an enemy gave (PLAY: points, gold and a boss's rune).
	struct KillReport
	{
		bool killed;
		EnemyKind kind;
		int points;
		int gold;
		Rune rune;
	};

	// Axis-aligned box in field pixels.
	struct Bounds
	{
		float x;
		float y;
		float w;
		float h;
	};

	Bounds EnemyBounds(const ActiveEnemy& enemy);  // visible body of the enemy, where spells can hit it

	// ---- Tornado: the first spell with a real effect ----
	// Casting Tornado (from D or F) launches a projectile from the player toward the enemy. The direction is
	// fixed at launch (no homing). The cast is judged when the projectile hits the enemy, not when it is cast.
	// INITIAL MVP TUNING VALUES, like the difficulty ones.
	const float TORNADO_SPEED = 700.0f;         // px/s
	const float TORNADO_HIT_RADIUS = 20.0f;     // hit circle around the projectile centre (the sprite is drawn 64 px wide)
	const float TORNADO_MAX_DISTANCE = 1200.0f; // it is removed after flying this far ...
	const float TORNADO_FIELD_MARGIN = 64.0f;   // ... or when its centre is this far outside the field
	const float TORNADO_MAX_STEP = 10.0f;       // px: longest move tested for a hit at once, so nothing tunnels
	// No limit on how many projectiles are in flight: every valid cast makes one. Each lives at most
	// TORNADO_MAX_DISTANCE / TORNADO_SPEED (about 1.7 s) and a cast needs its own key press, so the number
	// alive is bounded by how fast keys can be pressed and no technical cap is needed.
	const int   TORNADO_FRAME_COUNT = 16;       // frames of the sprite sheet (4 x 4 grid)
	const float TORNADO_ANIM_FPS = 10.0f;       // animation speed, independent of the projectile speed

	struct Tornado
	{
		bool active;
		float x;          // centre, field pixels
		float y;
		float dirX;       // unit vector, set once at launch
		float dirY;
		float travelled;  // px flown so far
		float animTime;   // seconds alive, drives the animation
		int enemyId;      // the enemy it was launched at (PracticeSession::SpawnCount() at that moment)
	};

	// (dx, dy) is only a direction (it is normalised); a zero vector falls back to "straight right".
	Tornado MakeTornado(float originX, float originY, float dx, float dy, int enemyId);
	void AdvanceTornado(Tornado& tornado, float dt);           // moves it by dir * speed * dt; deactivates it when done
	bool TornadoHits(const Tornado& tornado, const Bounds& target);  // hit circle against box
	int TornadoFrame(float animTime);                          // 0 .. TORNADO_FRAME_COUNT-1, looping

	struct InputResult
	{
		bool accepted;                  // false when the session is not Playing
		invoker::InvokerResult invoker; // what the Invoker Core did with the key
		CastOutcome cast;               // Correct / Incorrect only for a filled slot cast against an active enemy
		bool tornadoLaunched;           // a Tornado projectile was created (it is judged later, when it hits)
		KillReport kill;                // a Correct cast that finished the enemy (a chain step is Correct, not a kill)
	};

	struct UpdateResult
	{
		bool spawned;      // an enemy appeared this update
		bool leaked;       // an enemy reached the player this update (HP was reduced)
		bool gameOver;     // HP reached 0 this update
		CastOutcome cast;  // a Tornado hit was judged this update (None if no projectile hit)
		KillReport kill;   // that hit finished the enemy
		int leakDamage;    // lives lost by the leak (PLAY: 1 / 2 / 3)
		bool shieldUsed;   // a Shield rune took the leak instead
	};

	class PracticeSession
	{
	public:
		PracticeSession();                // starts in Ready

		// Survival (the former Practice) or PLAY; kept for every later Start / PressEnter. Ignored while Playing.
		void SetMode(SessionMode mode) { if (m_state != GameState::Playing) m_mode = mode; }
		SessionMode Mode() const { return m_mode; }

		void Start(unsigned seed);        // new session (also used to restart): resets everything except the
		                                  // best combo record, state = Playing
		void ReturnToReady();             // stop the current session and go back to Ready (same reset as Start)

		// Session control keys. The presentation only maps the SDL keys to these two calls.
		bool PressEnter(unsigned seed);   // Ready / Game Over -> new session; ignored while Playing. true = started
		bool PressEscape();               // Ready -> true (quit the application); Playing / Game Over -> Ready, false

		InputResult Input(invoker::InputAction action);  // Q/W/E/R/D/F; ignored unless Playing
		UpdateResult Update(float dt);                   // seconds; ignored unless Playing

		GameState State() const { return m_state; }
		const Stats& GetStats() const { return m_stats; }
		const ActiveEnemy& Enemy() const { return m_enemy; }
		const invoker::InvokerState& Invoker() const { return m_invoker; }
		int SpawnCount() const { return m_spawnCount; }  // enemies spawned this session
		// Seeds the best combo record from saved data (never lowers it). Used once at start-up.
		void RestoreBestCombo(int record) { if (record > m_stats.bestCombo) m_stats.bestCombo = record; }
		// The player turned the recipe hint on during this run (or started it with the hint on): the run is marked
		// assisted for good, so it sets no records and enters no leaderboard. Ignored outside Playing.
		// A best combo reached earlier in the same run is taken back.
		void MarkAssisted()
		{
			if (m_state != GameState::Playing)
				return;
			m_stats.assisted = true;
			m_stats.bestCombo = m_recordAtStart;
		}

		// PLAY runes (P3-4): the reward a boss gives, also callable directly (tests). Returns the rune applied.
		Rune ApplyRune(Rune rune);
		float FrostLeft() const { return m_frostLeft; }
		float DoubleLeft() const { return m_doubleLeft; }
		bool HasShield() const { return m_shield; }

		// Launches a Tornado from the player along (dx, dy) at the current enemy. Input() calls it with the direction
		// toward the enemy; it is public so a test can aim somewhere else. false = no enemy (or not Playing).
		bool LaunchTornado(float dx, float dy);
		const Tornado& GetTornado(int index) const { return m_tornadoes[index]; }  // 0 <= index < ActiveTornadoCount()
		int ActiveTornadoCount() const { return static_cast<int>(m_tornadoes.size()); }  // only live projectiles are kept

	private:
		void SpawnEnemy();
		unsigned NextRandom();
		void StartWaiting();              // arm the timer for the next enemy
		void ResetSession();              // fresh stats (best combo kept), no enemy, empty orbs and D/F
		CastOutcome JudgeCast(invoker::SkillId spell);  // the one place that decides right / wrong and scores it
		void UpdateTornadoes(float dt, UpdateResult& result);

		GameState m_state;
		Stats m_stats;
		ActiveEnemy m_enemy;
		std::vector<Tornado> m_tornadoes;  // the projectiles in flight; one that hits or leaves is erased
		invoker::InvokerState m_invoker;
		invoker::SkillId m_lastTarget;    // target of the previous enemy (never picked twice in a row)
		float m_spawnTimer;               // seconds until the next enemy while none is active
		unsigned m_rng;
		int m_spawnCount;
		int m_recordAtStart = 0;          // best combo record when this run started (for MarkAssisted)
		SessionMode m_mode = SessionMode::Survival;
		float m_frostLeft = 0.0f;         // PLAY rune timers and the shield
		float m_doubleLeft = 0.0f;
		bool m_shield = false;
		KillReport m_kill = {};           // filled by JudgeCast when it finishes an enemy
		Rune PickRune();
	};
}

#endif // PRACTICE_H_
