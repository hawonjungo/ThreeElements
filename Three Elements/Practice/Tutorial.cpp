#include "Tutorial.h"

#include <cstring>

using invoker::InputAction;
using invoker::InvokerEvent;
using invoker::SkillId;

namespace practice
{
	namespace
	{
		const TutorialOnEnter NONE = TutorialOnEnter::Nothing;
		const TutorialStepKind CARD = TutorialStepKind::Card;
		const TutorialStepKind KEYS = TutorialStepKind::Keys;

		// The script of GAMEPLAY_SPEC.md §24. Lines are at most ~40 characters (they are drawn at scale 2).
		const TutorialStep kSteps[] =
		{
			// 1 - elements
			{ CARD, 1, "QUAS, WEX AND EXORT:",         "YOUR THREE ELEMENTS.",               "",    HIGHLIGHT_ORBS, NONE },
			{ KEYS, 1, "PRESS Q, W AND E.",            "EACH ONE LOADS AN ORB.",             "QWE", HIGHLIGHT_ORBS, NONE },
			{ CARD, 1, "YOU HOLD 3 ORBS.",             "A 4TH PUSHES OUT THE OLDEST.",       "",    HIGHLIGHT_ORBS, NONE },
			{ KEYS, 1, "PRESS Q ONCE MORE.",           "",                                   "Q",   HIGHLIGHT_ORBS, NONE },
			// 2 - Sun Strike, key by key (the only guided spell, T-4)
			{ CARD, 2, "AN ENEMY! LOOK AT ITS TARGET:", "THE SPELL THAT DEFEATS IT.",       "",    HIGHLIGHT_ENEMY | HIGHLIGHT_TARGET, TutorialOnEnter::DummyWalksIn },
			{ CARD, 2, "SUN STRIKE  =  E E E",         "",                                   "",    HIGHLIGHT_TARGET, NONE },
			{ KEYS, 2, "PRESS E THREE TIMES.",         "",                                   "EEE", HIGHLIGHT_ORBS, NONE },
			{ CARD, 2, "3 ORBS + R = A SPELL.",        "PRESS R TO INVOKE IT.",              "",    HIGHLIGHT_ORBS, NONE },
			{ KEYS, 2, "INVOKE WITH R.",               "",                                   "R",   HIGHLIGHT_SLOT_D, NONE },
			{ CARD, 2, "SUN STRIKE IS IN SLOT D.",     "PRESS D TO CAST IT.",                "",    HIGHLIGHT_SLOT_D | HIGHLIGHT_ENEMY, NONE },
			{ KEYS, 2, "CAST WITH D.",                 "",                                   "D",   HIGHLIGHT_SLOT_D | HIGHLIGHT_ENEMY, NONE },
			{ CARD, 2, "RIGHT SPELL = ENEMY GONE!",    "",                                   "",    HIGHLIGHT_NONE, NONE },
			// 3 - the other rules, as cards
			{ CARD, 3, "ORDER DOES NOT MATTER:",       "QQW = QWQ = WQQ",                    "",    HIGHLIGHT_ORBS, NONE },
			{ CARD, 3, "A NEW SPELL GOES TO D,",       "THE OLD ONE TO F. BOTH CAST.",       "",    HIGHLIGHT_SLOT_D | HIGHLIGHT_SLOT_F, NONE },
			{ CARD, 3, "WRONG SPELL = MISS.",          "NO DAMAGE. JUST TRY AGAIN.",         "",    HIGHLIGHT_NONE, NONE },
			{ CARD, 3, "FORGOT A RECIPE?",             "PRESS H FOR THE RECIPE LIST.",       "",    HIGHLIGHT_NONE, NONE },
			// 4 - your turn
			{ CARD, 4, "NOW YOU! READ EACH TARGET,",   "FIND ITS KEYS, INVOKE AND CAST.",    "",    HIGHLIGHT_TARGET, NONE },
			{ TutorialStepKind::Run, 4, "DEFEAT 3 ENEMIES.", "H: RECIPE LIST",               "",    HIGHLIGHT_ENEMY | HIGHLIGHT_TARGET, TutorialOnEnter::StartRun },
			// end
			{ TutorialStepKind::Done, 4, "TUTORIAL COMPLETE!", "ENTER: PLAY   ESC: MENU", "", HIGHLIGHT_NONE, NONE },
		};
		const int STEP_COUNT = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));

		char KeyLetter(InputAction action)
		{
			switch (action)
			{
			case InputAction::Q: return 'Q';
			case InputAction::W: return 'W';
			case InputAction::E: return 'E';
			case InputAction::R: return 'R';
			case InputAction::D: return 'D';
			default:             return 'F';
			}
		}
	}

	int TutorialStepCount() { return STEP_COUNT; }

	const TutorialStep& GetTutorialStep(int index)
	{
		if (index < 0) index = 0;
		if (index >= STEP_COUNT) index = STEP_COUNT - 1;
		return kSteps[index];
	}

	TutorialSession::TutorialSession()
		: m_step(0), m_keysDone(0), m_stopX(0.0f), m_hearts(START_HP), m_runResolved(0), m_runDefeated(0),
		  m_runTimer(0.0f), m_lastTarget(SkillId::None), m_rng(1)
	{
		m_enemy = { false, 0, SkillId::None, 0.0f, 0.0f };
	}

	void TutorialSession::Start(unsigned seed)
	{
		m_invoker.Reset();
		m_enemy = { false, 0, SkillId::None, 0.0f, 0.0f };
		m_hearts = START_HP;
		m_runResolved = m_runDefeated = 0;
		m_runTimer = 0.0f;
		m_lastTarget = SkillId::None;
		m_rng = seed != 0 ? seed : 1u;
		EnterStep(0);
	}

	void TutorialSession::EnterStep(int index)
	{
		m_step = index < STEP_COUNT ? index : STEP_COUNT - 1;
		m_keysDone = 0;
		switch (Step().onEnter)
		{
		case TutorialOnEnter::DummyWalksIn:
			SpawnEnemy(SkillId::SunStrike, TUTORIAL_WALK_IN_SPEED);
			m_stopX = TUTORIAL_DUMMY_STOP_X;
			break;
		case TutorialOnEnter::StartRun:
			m_enemy.active = false;
			m_hearts = START_HP;
			m_runResolved = m_runDefeated = 0;
			m_runTimer = 0.5f;
			m_stopX = -1.0e9f;  // run enemies do not stop: they walk on to the player
			break;
		default:
			break;
		}
	}

	bool TutorialSession::Next()
	{
		if (Step().kind != TutorialStepKind::Card)
			return false;
		EnterStep(m_step + 1);
		return true;
	}

	char TutorialSession::ExpectedKey() const
	{
		const TutorialStep& s = Step();
		if (s.kind != TutorialStepKind::Keys || m_keysDone >= static_cast<int>(std::strlen(s.keys)))
			return 0;
		return s.keys[m_keysDone];
	}

	TutorialInputResult TutorialSession::Input(InputAction action)
	{
		TutorialInputResult r = { false, false, { InvokerEvent::InvokeIgnored, SkillId::None }, CastOutcome::None, false };
		const TutorialStep& s = Step();

		if (s.kind == TutorialStepKind::Keys)
		{
			if (KeyLetter(action) != ExpectedKey())
			{
				r.wrongKey = true;  // refused: the orbs and slots stay exactly as the script says
				return r;
			}
			r.accepted = true;
			r.invoker = m_invoker.Apply(action);
			if (r.invoker.event == InvokerEvent::Cast)
				r.cast = Judge(r.invoker.skill);
			if (++m_keysDone >= static_cast<int>(std::strlen(s.keys)))
			{
				EnterStep(m_step + 1);
				r.stepAdvanced = true;
			}
			return r;
		}

		if (s.kind == TutorialStepKind::Run)
		{
			r.accepted = true;
			r.invoker = m_invoker.Apply(action);
			if (r.invoker.event == InvokerEvent::Cast)
			{
				r.cast = Judge(r.invoker.skill);
				if (r.cast == CastOutcome::Correct)
				{
					++m_runDefeated;
					if (++m_runResolved >= TUTORIAL_RUN_ENEMIES)
					{
						EnterStep(m_step + 1);
						r.stepAdvanced = true;
					}
					else
						m_runTimer = TUTORIAL_RUN_DELAY;
				}
			}
			return r;
		}
		return r;  // cards and the end: gameplay keys do nothing
	}

	TutorialUpdateResult TutorialSession::Update(float dt)
	{
		TutorialUpdateResult r = { false, false, false };
		if (dt < 0.0f) dt = 0.0f;
		if (dt > MAX_FRAME_TIME) dt = MAX_FRAME_TIME;

		if (m_enemy.active && m_enemy.x > m_stopX)
		{
			m_enemy.x -= m_enemy.speed * dt;
			if (m_enemy.x < m_stopX)
				m_enemy.x = m_stopX;
		}

		if (Step().kind != TutorialStepKind::Run)
			return r;

		if (m_enemy.active && m_enemy.x <= HIT_LINE_X)  // a leak: a heart, never the end of the run
		{
			m_enemy.active = false;
			if (m_hearts > 0)
				--m_hearts;
			r.leaked = true;
			if (++m_runResolved >= TUTORIAL_RUN_ENEMIES)
			{
				EnterStep(m_step + 1);
				r.finished = true;
				return r;
			}
			m_runTimer = TUTORIAL_RUN_DELAY;
		}

		if (!m_enemy.active)
		{
			m_runTimer -= dt;
			if (m_runTimer <= 0.0f)
			{
				SkillId target;
				do
					target = static_cast<SkillId>(NextRandom() % invoker::SKILL_COUNT);
				while (target == m_lastTarget);
				SpawnEnemy(target, TUTORIAL_RUN_SPEED);
				r.spawned = true;
			}
		}
		return r;
	}

	void TutorialSession::SpawnEnemy(SkillId target, float speed)
	{
		int definition = 0;
		for (int i = 0; i < ENEMY_TYPE_COUNT; ++i)
		{
			if (GetEnemyDefinition(i).targetSkill == target)
			{
				definition = i;
				break;
			}
		}
		m_enemy = { true, definition, target, SPAWN_X, speed };
		m_enemy.scale = GetEnemyDefinition(definition).size;  // drawn like everywhere else (§32 M-8)
		m_lastTarget = target;
	}

	// The Practice rule (§10-11): right exactly when the spell is the enemy's target. No score, combo or accuracy.
	CastOutcome TutorialSession::Judge(SkillId spell)
	{
		if (!m_enemy.active)
			return CastOutcome::None;
		if (spell == m_enemy.target)
		{
			m_enemy.active = false;
			return CastOutcome::Correct;
		}
		return CastOutcome::Incorrect;
	}

	unsigned TutorialSession::NextRandom()
	{
		m_rng = m_rng * 1664525u + 1013904223u;
		return m_rng >> 8;
	}
}
