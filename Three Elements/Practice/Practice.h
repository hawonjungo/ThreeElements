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
#include "Field.h"
#include "Items.h"
#include "Boss.h"

#include <vector>

namespace practice
{
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
		bool assisted;      // the recipe hint was on at some point in this run (spec §17); information only since
		                    // 1.6.1: a hinted run is ranked and sets records like any other
		int gold;           // PLAY: gold earned in this run (elites, bosses, Bounty); Survival: always 0
		int kills;          // enemies defeated
		int bossesDefeated; // PLAY: overlords and immortals beaten; stage = bossesDefeated + 1
		int immortalsBeaten; // PLAY: immortals among them (spec §28, §32)

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
	enum class EnemyKind { Normal, Elite, Overlord };
	// The tiers since 1.9 (§32): Normal (1 skill), Elite (a chain of 2), Overlord (a chain of 3; "boss" before 1.9)
	// and, above them, the Immortal: a §25 combo fight ("overlord" in 1.6 - 1.8).
	enum class Rune { None, Regeneration, Frost, DoubleDamage, Bounty, Shield };  // the reward for an overlord
	const int   PLAY_MAX_CHAIN = 3;
	// From the 10th enemy on, every 5th one is special: the 20th, 30th, 40th ... an overlord, the others of them
	// (the 10th, 15th, 25th, 35th ...) an elite. The first 9 enemies are all normal (14 before 1.9.4).
	const int   PLAY_ELITE_FIRST = 10;
	const int   PLAY_ELITE_EVERY = 5;
	const int   PLAY_OVERLORD_FIRST = 20;
	const int   PLAY_OVERLORD_EVERY = 10;
	const float PLAY_ELITE_SPEED = 0.8f;            // speed multipliers (longer chains need more keys)
	const float PLAY_OVERLORD_SPEED = 0.6f;
	const float PLAY_ELITE_SCALE = 1.4f;            // drawn (and hit) this much larger than the enemy's own size:
	const float PLAY_OVERLORD_SCALE = 2.0f;         // the tiers must look different at a glance (owner, 1.9.3)
	const int   PLAY_LEAK_NORMAL = 1;               // lives lost when it reaches the player
	const int   PLAY_LEAK_ELITE = 2;
	const int   PLAY_LEAK_OVERLORD = 3;
	const int   PLAY_POINTS_NORMAL = 1;             // score per kill
	const int   PLAY_POINTS_ELITE = 3;
	const int   PLAY_POINTS_OVERLORD = 10;
	const int   PLAY_GOLD_ELITE = 5;                // gold only from elites, bosses and Bounty
	const int   PLAY_GOLD_OVERLORD = 20;
	const int   PLAY_BOUNTY_GOLD = 25;
	const int   PLAY_MAX_HP = 5;                    // Regeneration never goes above this
	const float PLAY_FROST_TIME = 15.0f;            // s of Frost: enemies at PLAY_FROST_SPEED
	const float PLAY_FROST_SPEED = 0.6f;
	const float PLAY_DOUBLE_TIME = 20.0f;           // s of Double Damage: points x2
	const float PLAY_EULS_PUSHBACK = 150.0f;        // px, Wind Waker (Eul's level 2) pushes the enemy back (§27 I-5)
	const float PLAY_SMOKE_SPEED = 0.5f;            // Smoke of Deceit: enemies at 50 % speed
	const int   PLAY_MAX_RUNE_CHOICES = 3;          // Aghanim's Blessing

	EnemyKind PlayEnemyKind(int enemyNumber);       // enemyNumber = 1 for the first enemy of the run

	// ---- IMMORTAL (spec §28 and §32): a §25 combo fight inside the run. INITIAL TUNING VALUES. ----
	// It comes by chance (§32 M-3): every elite or overlord beaten raises the chance and rolls it at once; on a hit
	// the Immortal is the next thing to appear (an extra: it takes no enemy number) and the chance is back at 0.
	const int   PLAY_IMMORTAL_CHANCE_ELITE = 10;    // % added by an elite beaten
	const int   PLAY_IMMORTAL_CHANCE_OVERLORD = 20; // % added by an overlord beaten
	const float PLAY_IMMORTAL_WARNING = 4.0f;       // s of "IMMORTAL INCOMING" before the fight (time to read its combo)
	const int   PLAY_IMMORTAL_POINTS = 50;
	const int   PLAY_IMMORTAL_GOLD = 100;
	const float PLAY_IMMORTAL_SPEED_STEP = 0.15f;   // from the sixth of a run on: +15 % walking speed per immortal ...
	const float PLAY_IMMORTAL_WINDOW_STEP = 0.10f;  // ... and -10 % landing window (never below BOSS_MIN_WINDOW)
	const int   PLAY_IMMORTAL_MAX_DROPS = 2;        // materials an Immortal drops at most (M-7)
	int ImmortalTier(int boss);                     // 1 for a combo of 4, 2 for 5 or 6, 3 for 7 or 8 (the horn)
	int ImmortalDropCount(int boss);                // 1 for the first three, 2 for the last two

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
		bool runeChoice;   // a boss with Aghanim equipped: the game waits for ChooseRune() instead of a random rune
		bool immortal;     // it was an IMMORTAL (§28): kind is Overlord, and it dropped materials
		int materialCount; // 0, or what an Immortal dropped (1 or 2, §32 M-7)
		Material materials[PLAY_IMMORTAL_MAX_DROPS];
		int immortalChance;   // PLAY, an elite or overlord beaten: the chance after it, in % (what was rolled) ...
		bool immortalComing;  // ... and whether the roll hit: the Immortal is next
	};

	struct ItemUseResult
	{
		bool used;         // the item did something (and started its cooldown / used a unit up)
		ItemId item;
	};

	Bounds EnemyBounds(const ActiveEnemy& enemy);  // visible body of the enemy, where spells can hit it

	struct InputResult
	{
		bool accepted;                  // false when the session is not Playing
		invoker::InvokerResult invoker; // what the Invoker Core did with the key
		CastOutcome cast;               // Correct / Incorrect only for a filled slot cast against an active enemy
		bool tornadoLaunched;           // a Tornado projectile was created (it is judged later, when it hits)
		KillReport kill;                // a Correct cast that finished the enemy (a chain step is Correct, not a kill)
		bool immortal;                  // the key went to the IMMORTAL fight: `boss` says what it did there
		BossInputResult boss;
	};

	struct UpdateResult
	{
		bool spawned;      // an enemy appeared this update
		bool leaked;       // an enemy reached the player this update (HP was reduced)
		bool gameOver;     // HP reached 0 this update
		CastOutcome cast;  // a Tornado hit was judged this update (None if no projectile hit)
		KillReport kill;   // that hit finished the enemy
		int leakDamage;    // lives lost by the leak (PLAY: 1 / 2 / 3)
		bool shieldUsed;   // a Shield rune or a Black King Bar took the leak instead
		bool immortalWarning;  // "IMMORTAL INCOMING" started this update (immortalBoss, immortalTier)
		int immortalBoss;
		int immortalTier;
		bool immortalFight;    // the warning ended: the fight starts
		bool immortalUpdated;  // an IMMORTAL fight ran this update: `boss` is its result (a contact also sets `leaked`)
		BossUpdateResult boss;
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
		// orbs and D / F slots (the IMMORTAL fight works on its own copy, taken in and handed back, §28 O-4)
		const invoker::InvokerState& Invoker() const { return m_immortalActive ? m_immortal.Invoker() : m_invoker; }
		int SpawnCount() const { return m_spawnCount; }  // enemies spawned this session
		// Seeds the best combo record from saved data (never lowers it). Used once at start-up.
		void RestoreBestCombo(int record) { if (record > m_stats.bestCombo) m_stats.bestCombo = record; }
		// The player turned the recipe hint on during this run (or started it with the hint on): the run is marked
		// assisted for good. It changes nothing else (owner 2026-10-01: hinted runs are ranked). Ignored outside Playing.
		void MarkAssisted()
		{
			if (m_state == GameState::Playing)
				m_stats.assisted = true;
		}

		// PLAY runes (P3-4): the reward a boss gives, also callable directly (tests). Returns the rune applied.
		Rune ApplyRune(Rune rune);
		// PLAY items (§27): the loadout is copied in before a run (ignored while Playing); UseItem uses the item in a
		// slot (0..5) when it is ready. Consumables used up here are gone from Loadout() too (the caller saves it).
		void SetLoadout(const Inventory& inv) { if (m_state != GameState::Playing) m_inv = inv; }
		const Inventory& Loadout() const { return m_inv; }
		ItemUseResult UseItem(int slot);
		float ItemCooldown(ItemId id) const { return m_itemCooldown[static_cast<int>(id)]; }       // s left
		float ItemCooldownTotal(ItemId id) const { return m_itemCooldownTotal[static_cast<int>(id)]; } // s at use
		float BkbLeft() const { return m_bkbLeft; }
		float SmokeLeft() const { return m_smokeLeft; }
		// Aghanim (§27 I-5): after a boss the run waits (enemies stop, keys do nothing) until a rune is chosen.
		int RuneChoiceCount() const { return m_runeChoiceCount; }
		Rune RuneChoice(int i) const { return m_runeChoices[i]; }
		Rune ChooseRune(int i);                  // applies choice i; Rune::None when nothing is being chosen
		float FrostLeft() const { return m_frostLeft; }
		float DoubleLeft() const { return m_doubleLeft; }
		bool HasShield() const { return m_shield; }

		// IMMORTAL (§28): first the warning, then the fight (Immortal() is a §25 boss fight driven by this run).
		float ImmortalWarningLeft() const { return m_immortalWarning; }   // > 0 during the warning
		bool ImmortalActive() const { return m_immortalActive; }          // the fight is on
		int ImmortalBoss() const { return m_immortalBoss; }               // boss index, valid during warning and fight
		int ImmortalTierNow() const { return m_immortalTier; }
		// the boss; it stands at the edge of the field (not moving) during the warning already
		const BossSession& Immortal() const { return m_immortal; }
		bool RefresherArmed() const { return m_refresherArmed; }          // the next damaging combo deals x2 (O-6)
		int DefeatedBy() const { return m_defeatedBy; }                   // boss index after losing to one, else -1 (O-9)
		int ImmortalChance() const { return m_immortalChance; }           // %, 0..100 (§32 M-3)
		bool ImmortalDue() const { return m_immortalDue; }                // the roll hit: it is the next to appear

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
		void BeginImmortal(UpdateResult& result);       // the warning starts instead of a spawn
		void StartImmortalSession();                    // the boss with this run's lives, slots and scaling
		void UpdateImmortal(float dt, UpdateResult& result);
		void GiveBossRune();                            // a boss's rune (or Aghanim's choice), into m_kill

		GameState m_state;
		Stats m_stats;
		ActiveEnemy m_enemy;
		std::vector<Tornado> m_tornadoes;  // the projectiles in flight; one that hits or leaves is erased
		invoker::InvokerState m_invoker;
		invoker::SkillId m_lastTarget;    // target of the previous enemy (never picked twice in a row)
		float m_spawnTimer;               // seconds until the next enemy while none is active
		unsigned m_rng;
		int m_spawnCount;
		SessionMode m_mode = SessionMode::Survival;
		float m_frostLeft = 0.0f;         // PLAY rune timers and the shield
		float m_doubleLeft = 0.0f;
		bool m_shield = false;
		KillReport m_kill = {};           // filled by JudgeCast when it finishes an enemy
		Rune PickRune();
		int AddGold(int gold);            // Hand of Midas applied; returns the gold actually gained
		Inventory m_inv = EmptyInventory();
		float m_itemCooldown[ITEM_COUNT] = {};
		float m_itemCooldownTotal[ITEM_COUNT] = {};
		float m_backLeft = 0.0f;          // Blink: the enemy walks back
		float m_stillLeft = 0.0f;         // Eul's: the enemy stands still
		float m_bkbLeft = 0.0f;
		float m_smokeLeft = 0.0f;
		Rune m_runeChoices[PLAY_MAX_RUNE_CHOICES] = {};
		int m_runeChoiceCount = 0;
		float m_difficultyTime = 0.0f;    // the difficulty clock: survival time minus the IMMORTAL time (O-5)
		BossSession m_immortal;
		bool m_immortalActive = false;
		float m_immortalWarning = 0.0f;
		int m_immortalBoss = 0;
		int m_immortalTier = 0;
		int m_lastImmortal = -1;          // never the same one twice in a row once they are random
		int m_immortalChance = 0;         // %
		bool m_immortalDue = false;
		bool m_refresherArmed = false;
		int m_defeatedBy = -1;
	};
}

#endif // PRACTICE_H_
