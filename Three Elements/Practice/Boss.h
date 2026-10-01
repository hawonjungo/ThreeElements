#pragma once
#ifndef BOSS_H_
#define BOSS_H_

// Boss mode (GAMEPLAY_SPEC.md §25, update 1.2): one boss that only takes damage from a combo, with Dota-like spell
// timings. Tornado lifts the boss; the follow-up spells should land just as it comes down, and each is graded by
// how close it came (PERFECT / GREAT / GOOD); the combo's damage is the average of those grades.
//
// Same layer and same habits as PracticeSession and TutorialSession: no SDL, no clock (dt is passed in), no
// globals. Orbs, invoke and the D/F slots come from the Core InvokerState. Practice itself is untouched: its
// spells stay instant; the delays below exist only here.

#include "Field.h"

namespace practice
{
	// ---- spell timelines in Boss mode (B-5). INITIAL TUNING VALUES; Dota 2 Invoker is the reference. ----
	const float BOSS_LIFT_TIME = 2.5f;          // s the Tornado keeps the boss in the air (Dota: up to 2.9 s)
	const float BOSS_LIFT_HEIGHT = 110.0f;      // px at the top of the lift (drawing only, see BossLiftHeight)
	const float BOSS_SUN_STRIKE_DELAY = 1.7f;   // s from cast to impact
	const float BOSS_METEOR_DELAY = 1.3f;
	const float BOSS_EMP_DELAY = 2.9f;
	const float BOSS_SUN_STRIKE_RADIUS = 70.0f; // px around the impact point
	const float BOSS_METEOR_RADIUS = 80.0f;
	const float BOSS_EMP_RADIUS = 120.0f;
	// Tornado and Deafening Blast are projectiles flying at TORNADO_SPEED (Practice.h), the same 700 px/s.

	// ---- timing grades (B-14): how long after the landing a follow-up spell hit. INITIAL TUNING VALUES. ----
	const float BOSS_PERFECT_TIME = 0.15f;      // s after the landing: PERFECT up to here
	const float BOSS_GREAT_TIME = 0.4f;         //                      GREAT up to here, GOOD for the rest of the window
	const float BOSS_IDEAL_AFTER_LANDING = 0.1f;  // the timing bars aim here (inside PERFECT, a little room for early)
	const int   BOSS_SCORE_PERFECT = 100;       // % of the boss's HP, averaged over the combo's follow-up spells
	const int   BOSS_SCORE_GREAT = 60;
	const int   BOSS_SCORE_GOOD = 35;
	const int   BOSS_FULL_HP = 100;             // %

	const float BOSS_START_X = 900.0f;          // body-left of the boss when a fight starts
	const float BOSS_RESET_X = 760.0f;          // knocked back here after it reaches the player
	const float BOSS_PUSHBACK = 200.0f;         // px pushed back by a combo (at most to BOSS_RESET_X)
	const float BOSS_PUSH_SPEED = 600.0f;       // px/s while being pushed back
	const int   BOSS_MAX_COMBO = 5;
	const int   BOSS_COUNT = 8;                 // 1-3 from update 1.2, 4-8 from update 1.5 (all ten skills)

	// ---- update 1.5 (B-16, B-17, B-19): quick steps, holds, phases. INITIAL TUNING VALUES. ----
	const float BOSS_QUICK_PERFECT = 1.2f;      // s after the previous spell of the combo: PERFECT up to here
	const float BOSS_QUICK_GREAT = 2.0f;        //                                          GREAT up to here
	const float BOSS_QUICK_LIMIT = 3.2f;        // GOOD up to here; later the step is missed (TOO LATE)
	const float BOSS_FREEZE_TIME = 3.2f;        // Cold Snap: the boss does not move
	const float BOSS_ICE_WALL_TIME = 4.0f;      // Ice Wall: the boss moves at BOSS_ICE_WALL_SPEED
	const float BOSS_ICE_WALL_SPEED = 0.3f;
	const float BOSS_CONFUSE_TIME = 3.2f;       // Ghost Walk: the boss lost the player and stands still
	const int   BOSS_PHASE2_HP = 50;            // % at or below which a two-phase boss changes its combo
	const int   BOSS_MAX_PENDING = 16;          // delayed spells waiting to land (more are dropped, never judged)
	const float BOSS_MIN_WINDOW = 0.6f;         // a scaled OVERLORD's landing window never gets shorter than this

	// Delay from cast to impact of a delayed ground spell (Sun Strike, Chaos Meteor, EMP); 0 for any other spell.
	float BossSpellDelay(invoker::SkillId skill);
	float BossSpellRadius(invoker::SkillId skill);  // 0 when BossSpellDelay is 0
	// How high a lifted boss floats after `airTime` seconds in the air: up quickly, a gentle bob, down at the end.
	float BossLiftHeight(float airTime);

	enum class HitGrade { None, Perfect, Great, Good, Miss };  // None = not landed yet
	// Grade of a spell that hit the grounded boss `afterLanding` seconds after it came down (Miss past the window).
	HitGrade GradeHit(float afterLanding, float window);
	int GradeScore(HitGrade grade);                 // 100 / 60 / 35, 0 for Miss and None
	// Grade of a quick step cast `sincePrevious` seconds after the previous spell of the combo (B-16).
	HitGrade GradeQuick(float sincePrevious);
	enum class BossStepKind { Opener, Landing, Quick };

	struct BossDefinition
	{
		const char* name;
		int enemyDefinition;               // sprite and body of an enemy of the Practice table, drawn larger
		float scale;                       // drawn size: the enemy's frame times this
		unsigned char tint[3];             // colour modulation of the sprite
		invoker::SkillId combo[BOSS_MAX_COMBO];
		int comboLength;
		float speed;                       // px/s while walking
		float window;                      // s after landing in which a follow-up spell still scores
		bool guided;                       // boss 1: the CAST NOW cue (B-10)
		invoker::SkillId combo2[BOSS_MAX_COMBO];  // the second phase's combo (B-19); combo2Length 0 = one phase
		int combo2Length;
	};
	const BossDefinition& GetBossDefinition(int index);  // 0 <= index < BOSS_COUNT

	enum class BossState { Fighting, Won, Lost };
	enum class BossPhase { Walking, Airborne, PushedBack };
	enum class ComboFail { None, WrongSpell, TooEarly, TooLate, Missed };

	struct BossProjectile      // Tornado or Deafening Blast on its way
	{
		invoker::SkillId skill;
		Tornado motion;        // same flight as Practice's Tornado (MakeTornado / AdvanceTornado)
		int attempt;           // the combo attempt it belongs to; 0 = none (never judged)
		bool resolved;         // it has reached the boss once (hit it, or passed through it in the air)
	};
	struct PendingSpell        // Sun Strike / Chaos Meteor / EMP waiting to land
	{
		invoker::SkillId skill;
		float x;               // impact point (centre), field pixels
		float left;            // s until impact
		float delay;           // the whole delay, for drawing the closing ring
		int attempt;
	};
	struct BossImpact          // something reached the boss or the ground this update (for effects and sounds)
	{
		invoker::SkillId skill;
		float x;
		HitGrade grade;        // None: not a step of the running attempt; Miss: see `miss`
		ComboFail miss;        // why a step missed (TooEarly / TooLate / Missed)
	};

	struct BossInputResult
	{
		bool accepted;                  // false unless Fighting
		invoker::InvokerResult invoker; // what the Core did with the key
		bool attemptStarted;            // this cast started a combo attempt
		ComboFail fail;                 // WrongSpell when this cast broke the running attempt
		HitGrade grade;                 // a quick step: its grade, given at once (None otherwise)
	};

	struct BossUpdateResult
	{
		bool lifted;          // a Tornado hit and lifted the boss
		bool landed;          // the boss came down
		int impactCount;
		BossImpact impacts[4];
		bool comboComplete;   // an attempt ended with damage
		int damage;           // % taken by that combo
		ComboFail fail;       // an attempt ended without damage (or broke) during this update
		bool quickMissed;     // a quick step ran out of time (TOO LATE); the combo goes on
		bool phaseChanged;    // the boss switched to its second combo (B-19)
		bool playerHit;       // the boss reached the player: HP -1 (unless contactBlocked)
		bool contactBlocked;  // OVERLORD only: a Shield or a Black King Bar took that contact
		bool won;
		bool lost;
	};

	class BossSession
	{
	public:
		BossSession();

		void Start(int boss);                 // new fight against GetBossDefinition(boss)
		// ---- as an OVERLORD inside a PLAY run (spec §28): the run's lives and invoker come in, the boss may be
		// faster and its window shorter (never below BOSS_MIN_WINDOW), and the run sets its effects every frame.
		void StartOverlord(int boss, int playerHp, const invoker::InvokerState& invoker, float speedScale, float windowScale);
		void SetPlayerHp(int hp) { m_playerHp = hp; }
		// speedFactor: Frost / Smoke; still: Eul's; walkBack: Blink; contactBlocked: Shield / BKB; damageMultiplier: Refresher
		void SetModifiers(float speedFactor, bool still, bool walkBack, bool contactBlocked, int damageMultiplier);
		void PushBack(float px);              // Wind Waker: back by px at once (not past BOSS_START_X)
		float Speed() const { return m_speed; }
		float Window() const { return m_window; }
		BossInputResult Input(invoker::InputAction action);
		BossUpdateResult Update(float dt);
		void MarkAssisted() { if (m_state == BossState::Fighting) m_assisted = true; }  // recipe hint (B-12)

		// the fight
		BossState State() const { return m_state; }
		int BossIndex() const { return m_boss; }
		const BossDefinition& Def() const { return GetBossDefinition(m_boss); }
		int BossHp() const { return m_bossHp; }       // % left, 0..BOSS_FULL_HP
		int PlayerHp() const { return m_playerHp; }
		float Elapsed() const { return m_elapsed; }   // s since the fight started (the result time)
		bool Assisted() const { return m_assisted; }
		const invoker::InvokerState& Invoker() const { return m_invoker; }

		// the boss
		float X() const { return m_x; }               // body-left, field pixels, on the ground
		BossPhase Phase() const { return m_phase; }
		float AirTime() const { return m_airTime; }   // s in the air (Airborne only)
		Bounds Body() const;                          // visible body on the ground (the lift is not included)
		float CenterX() const;

		// the combo (the second phase's once the boss is at 50 % or less)
		const invoker::SkillId* Combo() const { return m_comboPhase == 1 ? Def().combo2 : Def().combo; }
		int ComboLength() const { return m_comboPhase == 1 ? Def().combo2Length : Def().comboLength; }
		int ComboPhase() const { return m_comboPhase; }
		BossStepKind StepKind(int step) const;        // Opener for step 0; see B-16
		// the next quick step: seconds left to cast it and the limit. false = the next step is not a quick one
		bool QuickTiming(int step, float& left, float& total) const;
		// holds (B-17), seconds left
		float FreezeLeft() const { return m_freezeLeft; }
		float SlowLeft() const { return m_slowLeft; }
		float ConfuseLeft() const { return m_confuseLeft; }

		// the combo attempt
		bool AttemptRunning() const { return m_running; }
		int CastSteps() const { return m_castIndex; } // combo spells cast so far in the running attempt
		bool StepDone(int step) const;                // step 0 = the Tornado hit, 1.. = follow-up graded
		HitGrade StepGrade(int step) const;           // follow-ups of the running attempt (None = not landed yet)
		ComboFail LastFail() const { return m_lastFail; }
		int LastDamage() const { return m_lastDamage; }
		bool CueNow() const;                          // guided boss: casting the next spell now scores at least GREAT
		// Timing bar of follow-up `step` (B-15): seconds until the ideal moment to cast it (negative = past it) and
		// the time from the attempt's start to that moment. false = no bar (no attempt, no landing known yet, or
		// the step is cast already).
		bool StepTiming(int step, float& untilIdeal, float& span) const;

		int ProjectileCount() const { return static_cast<int>(m_projectiles.size()); }
		const BossProjectile& GetProjectile(int i) const { return m_projectiles[i]; }
		int PendingCount() const { return m_pendingCount; }
		const PendingSpell& GetPending(int i) const { return m_pending[i]; }

	private:
		void Cast(invoker::SkillId skill, BossInputResult& result);
		void Fail(ComboFail reason, ComboFail& report);
		void JudgeImpact(invoker::SkillId skill, float x, int attempt, bool projectile, BossUpdateResult& result);
		void Resolve(BossUpdateResult& result);         // the attempt ends: damage from its grades
		float ImpactDelay(invoker::SkillId skill) const;  // from a cast now to its impact on the boss (-1 = none)
		bool LandingTime(float& when) const;            // the running attempt's landing (m_elapsed terms), if known
		void ApplyHold(invoker::SkillId skill);         // Cold Snap / Ice Wall / Ghost Walk as a step of the combo

		BossState m_state;
		int m_boss;
		int m_bossHp;
		int m_playerHp;
		float m_elapsed;
		bool m_assisted;
		invoker::InvokerState m_invoker;

		float m_x;
		BossPhase m_phase;
		float m_airTime;
		float m_pushTarget;
		int m_liftAttempt;          // the attempt whose Tornado lifted the boss (0 = none)
		float m_liftStart;          // m_elapsed when that happened

		int m_attempt;              // id of the latest attempt (1, 2, ...)
		bool m_running;
		float m_attemptStart;       // m_elapsed at its Tornado cast
		int m_castIndex;
		bool m_tornadoHit;
		HitGrade m_grades[BOSS_MAX_COMBO];
		ComboFail m_missReasons[BOSS_MAX_COMBO];
		bool m_landed;              // the running attempt's lift is over
		float m_landTime;           // m_elapsed when it came down
		ComboFail m_lastFail;
		int m_lastDamage;
		float m_castTime[BOSS_MAX_COMBO];   // m_elapsed when each step of the running attempt was cast
		int m_comboPhase;                   // 0, or 1 once a two-phase boss is at BOSS_PHASE2_HP or less
		float m_freezeLeft;
		float m_slowLeft;
		float m_confuseLeft;
		float m_speed;              // px/s and window of this fight (the definition's, scaled for a later OVERLORD)
		float m_window;
		float m_speedFactor;        // OVERLORD modifiers, set by the run every frame (neutral in Boss Fights)
		bool m_still;
		bool m_walkBack;
		bool m_contactBlocked;
		int m_damageMultiplier;

		std::vector<BossProjectile> m_projectiles;
		PendingSpell m_pending[BOSS_MAX_PENDING];
		int m_pendingCount;
	};
}

#endif // BOSS_H_
