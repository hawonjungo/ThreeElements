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

namespace practice
{
	// ---- play field (logical pixels of the 928x544 window) ----
	// Enemy positions are the x of the enemy's visible body, left edge (see EnemyDefinition::bodyLeft).
	const float SPAWN_X = 990.0f;        // sprite starts fully off-screen on the right
	const float HIT_LINE_X = 140.0f;     // an enemy whose body reaches this x has reached the player
	const float MAX_FRAME_TIME = 0.1f;   // dt is clamped to this so a hitch never teleports an enemy
	const int   START_HP = 3;

	// ---- enemies ----
	const int ENEMY_TYPE_COUNT = 10;     // one visual identity per skill

	struct EnemyDefinition
	{
		int id;                          // equals its index in the table
		const char* name;                // for logs / the debug overlay only
		const char* sprite;              // asset path (data only, no textures here)
		int frames;                      // frames in the horizontal sprite sheet
		int bodyLeft;                    // first visible column of a drawn frame (after the horizontal flip)
		int feetRow;                     // row of the lowest visible pixel of a frame
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
		int bestCombo;      // record: kept across restarts while the application runs (not saved to disk yet)
		int correctCasts;
		int incorrectCasts;
		float survivalTime; // seconds spent Playing

		int TotalCasts() const { return correctCasts + incorrectCasts; }
		double Accuracy() const  // 0.0 .. 1.0; 0.0 while no cast has been judged yet
		{
			int total = TotalCasts();
			return total > 0 ? static_cast<double>(correctCasts) / static_cast<double>(total) : 0.0;
		}
	};

	struct ActiveEnemy
	{
		bool active;
		int definition;                 // index into the enemy table
		invoker::SkillId target;        // copied from the definition when spawned
		float x;                        // body-left, logical pixels
		float speed;                    // px/s, fixed at spawn
	};

	struct InputResult
	{
		bool accepted;                  // false when the session is not Playing
		invoker::InvokerResult invoker; // what the Invoker Core did with the key
		CastOutcome cast;               // Correct / Incorrect only for a filled slot cast against an active enemy
	};

	struct UpdateResult
	{
		bool spawned;   // an enemy appeared this update
		bool leaked;    // an enemy reached the player this update (HP was reduced)
		bool gameOver;  // HP reached 0 this update
	};

	class PracticeSession
	{
	public:
		PracticeSession();                // starts in Ready

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

	private:
		void SpawnEnemy();
		unsigned NextRandom();
		void StartWaiting();              // arm the timer for the next enemy
		void ResetSession();              // fresh stats (best combo kept), no enemy, empty orbs and D/F

		GameState m_state;
		Stats m_stats;
		ActiveEnemy m_enemy;
		invoker::InvokerState m_invoker;
		invoker::SkillId m_lastTarget;    // target of the previous enemy (never picked twice in a row)
		float m_spawnTimer;               // seconds until the next enemy while none is active
		unsigned m_rng;
		int m_spawnCount;
	};
}

#endif // PRACTICE_H_
