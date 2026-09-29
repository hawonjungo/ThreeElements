#pragma once
#ifndef TUTORIAL_H_
#define TUTORIAL_H_

// Tutorial mode (GAMEPLAY_SPEC.md §24): a short scripted introduction for players new to Invoker.
//
// Same layer and same rules as PracticeSession (no SDL, no clock, dt and the seed are passed in): orbs, invoke
// and the D/F slots come from the Core InvokerState, and a cast is right exactly when the spell is the enemy's
// target. What differs is the pacing: the player moves through a fixed script of steps.
//   - Card: text to read; Next() goes on.
//   - Keys: an exact key sequence (e.g. "EEER"); only the expected key is applied, others are refused.
//   - Run:  three slow enemies with random targets, played freely; a leak costs a heart but never ends the run.
//   - Done: the end card.
// Nothing here touches scores, records or the top 10.

#include "Practice.h"

namespace practice
{
	enum class TutorialStepKind { Card, Keys, Run, Done };

	// What the presentation should frame with the gold highlight during a step (bit flags).
	enum TutorialHighlight
	{
		HIGHLIGHT_NONE    = 0,
		HIGHLIGHT_ORBS    = 1 << 0,  // the orb row
		HIGHLIGHT_ENEMY   = 1 << 1,  // the enemy
		HIGHLIGHT_TARGET  = 1 << 2,  // the TARGET hint (skill name and icon)
		HIGHLIGHT_SLOT_D  = 1 << 3,
		HIGHLIGHT_SLOT_F  = 1 << 4,
	};

	// Something that happens when a step begins.
	enum class TutorialOnEnter { Nothing, DummyWalksIn, StartRun };

	struct TutorialStep
	{
		TutorialStepKind kind;
		int lesson;             // 1..TUTORIAL_LESSON_COUNT
		const char* line1;      // card / instruction text (English, capitals)
		const char* line2;      // second line, or ""
		const char* keys;       // Keys steps: the exact sequence, e.g. "EEE" (letters Q W E R D F); else ""
		int highlight;          // TutorialHighlight flags
		TutorialOnEnter onEnter;
	};

	const int TUTORIAL_LESSON_COUNT = 4;
	const int TUTORIAL_RUN_ENEMIES = 3;        // enemies in the final run
	const float TUTORIAL_DUMMY_STOP_X = 560.0f;   // lesson 2: the dummy walks in and stops here
	const float TUTORIAL_WALK_IN_SPEED = 260.0f;  // px/s while walking in to that spot
	const float TUTORIAL_RUN_SPEED = 70.0f;       // px/s in the final run (Practice starts at 125)
	const float TUTORIAL_RUN_DELAY = 1.5f;        // s between one run enemy ending and the next appearing

	int TutorialStepCount();
	const TutorialStep& GetTutorialStep(int index);  // 0 <= index < TutorialStepCount()

	struct TutorialInputResult
	{
		bool accepted;                  // the key was used (a Keys step's expected key, or any key in the Run)
		bool wrongKey;                  // a Keys step refused it: not the expected key
		invoker::InvokerResult invoker; // what the Core did with an accepted key
		CastOutcome cast;               // Correct / Incorrect for a cast at an enemy
		bool stepAdvanced;              // the key finished its step; the next step has begun
	};

	struct TutorialUpdateResult
	{
		bool spawned;   // an enemy appeared
		bool leaked;    // an enemy reached the player (Run only: a heart was lost)
		bool finished;  // the Run is over; the Done card has begun
	};

	class TutorialSession
	{
	public:
		TutorialSession();

		void Start(unsigned seed);   // back to the first step, empty orbs and slots, no enemy
		bool Next();                 // NEXT on a Card step; false (ignored) on any other step
		TutorialInputResult Input(invoker::InputAction action);
		TutorialUpdateResult Update(float dt);

		int StepIndex() const { return m_step; }
		const TutorialStep& Step() const { return GetTutorialStep(m_step); }
		bool IsDone() const { return Step().kind == TutorialStepKind::Done; }
		int KeysDone() const { return m_keysDone; }            // Keys steps: how many of the sequence are pressed
		char ExpectedKey() const;                              // Keys steps: the next key ('E', 'R', ...); else 0
		const invoker::InvokerState& Invoker() const { return m_invoker; }
		const ActiveEnemy& Enemy() const { return m_enemy; }
		int Hearts() const { return m_hearts; }                // Run: hearts left (starts at START_HP, may reach 0)
		int RunResolved() const { return m_runResolved; }      // Run: enemies defeated or leaked so far
		int RunDefeated() const { return m_runDefeated; }

	private:
		void EnterStep(int index);
		void SpawnEnemy(invoker::SkillId target, float speed);
		CastOutcome Judge(invoker::SkillId spell);
		unsigned NextRandom();

		int m_step;
		int m_keysDone;
		invoker::InvokerState m_invoker;
		ActiveEnemy m_enemy;
		float m_stopX;               // the enemy stops here (lesson 2), or walks on to the player (Run)
		int m_hearts;
		int m_runResolved;
		int m_runDefeated;
		float m_runTimer;            // Run: seconds until the next enemy while none is active
		invoker::SkillId m_lastTarget;
		unsigned m_rng;
	};
}

#endif // TUTORIAL_H_
