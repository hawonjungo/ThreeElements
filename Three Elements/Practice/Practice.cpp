#include "Practice.h"

namespace practice
{
	namespace
	{
		using invoker::SkillId;

		// The 10 enemies. Sprite, frame count and body/feet measurements come from the sheets in
		// assets/enemies (measured on the drawn, horizontally flipped frames). The skill each enemy
		// requires is plain data: change `targetSkill` here to remap, no other code depends on it.
		const EnemyDefinition kEnemies[ENEMY_TYPE_COUNT] =
		{
			// id  name        sprite                            frames bodyLeft feetRow target                    speed
			{ 0, "goblin",   "assets/enemies/goblin_run.png",    8,  56, 100, SkillId::Alacrity,       1.0f },
			{ 1, "skeleton", "assets/enemies/skeleton.png",      4,  45, 100, SkillId::ColdSnap,       1.0f },
			{ 2, "fire wiz", "assets/enemies/fire_wiz.png",      8,  52, 100, SkillId::SunStrike,      1.0f },
			{ 3, "dark wiz", "assets/enemies/dark_wiz.png",      8,  54, 100, SkillId::ChaosMeteor,    1.0f },
			{ 4, "eyes",     "assets/enemies/eyes_fly.png",      8,  52, 100, SkillId::GhostWalk,      1.0f },
			{ 5, "mushroom", "assets/enemies/mushroom_run.png",  8,  62, 100, SkillId::ForgeSpirit,    1.0f },
			{ 6, "necro",    "assets/enemies/nec_walk.png",     10,  45, 100, SkillId::DeafeningBlast, 1.0f },
			{ 7, "worm",     "assets/enemies/worm_run.png",      9,  35, 100, SkillId::Tornado,        1.0f },
			{ 8, "kitsune",  "assets/enemies/kitsune_run.png",   8,  54, 127, SkillId::EMP,            1.0f },
			{ 9, "knight",   "assets/enemies/knight_run.png",    8,  24,  43, SkillId::IceWall,        1.0f }
		};
	}

	const EnemyDefinition& GetEnemyDefinition(int index)
	{
		return kEnemies[index];
	}

	DifficultyParams DifficultyAt(float elapsedSeconds)
	{
		float t = elapsedSeconds > 0.0f ? elapsedSeconds : 0.0f;

		DifficultyParams p;
		p.enemySpeed = START_ENEMY_SPEED + ENEMY_SPEED_GROWTH * t;
		if (p.enemySpeed > MAX_ENEMY_SPEED)
			p.enemySpeed = MAX_ENEMY_SPEED;

		p.challengeDelay = START_CHALLENGE_DELAY - CHALLENGE_DELAY_DECAY * t;
		if (p.challengeDelay < MIN_CHALLENGE_DELAY)
			p.challengeDelay = MIN_CHALLENGE_DELAY;
		return p;
	}

	// ------------------------------------------------------------------ PracticeSession

	PracticeSession::PracticeSession()
		: m_state(GameState::Ready), m_lastTarget(SkillId::None), m_spawnTimer(0.0f), m_rng(1), m_spawnCount(0)
	{
		m_stats = { START_HP, START_HP, 0, 0, 0, 0, 0, 0.0f };
		m_enemy = { false, 0, SkillId::None, 0.0f, 0.0f };
	}

	void PracticeSession::Start(unsigned seed)
	{
		m_stats = { START_HP, START_HP, 0, 0, 0, 0, 0, 0.0f };
		m_enemy = { false, 0, SkillId::None, 0.0f, 0.0f };
		m_invoker.Reset();                       // orbs and D/F slots
		m_lastTarget = SkillId::None;
		m_rng = seed != 0 ? seed : 1u;
		m_spawnCount = 0;
		StartWaiting();                          // difficulty clock is survivalTime = 0
		m_state = GameState::Playing;
	}

	InputResult PracticeSession::Input(invoker::InputAction action)
	{
		InputResult result;
		result.accepted = false;
		result.invoker = { invoker::InvokerEvent::InvokeIgnored, SkillId::None };
		result.cast = CastOutcome::None;

		if (m_state != GameState::Playing)
			return result;

		result.accepted = true;
		result.invoker = m_invoker.Apply(action);

		// Only a cast of a filled slot while an enemy is active is judged.
		// Empty slots and casts with no enemy are ignored: no penalty, not counted.
		if (result.invoker.event == invoker::InvokerEvent::Cast && m_enemy.active)
		{
			if (result.invoker.skill == m_enemy.target)
			{
				result.cast = CastOutcome::Correct;
				++m_stats.correctCasts;
				++m_stats.score;
				++m_stats.combo;
				if (m_stats.combo > m_stats.bestCombo)
					m_stats.bestCombo = m_stats.combo;
				m_enemy.active = false;          // the enemy disappears immediately
				StartWaiting();
			}
			else
			{
				result.cast = CastOutcome::Incorrect;  // no damage, no HP loss, combo untouched, enemy keeps coming
				++m_stats.incorrectCasts;
			}
		}
		return result;
	}

	UpdateResult PracticeSession::Update(float dt)
	{
		UpdateResult result = { false, false, false };
		if (m_state != GameState::Playing)
			return result;

		if (dt < 0.0f)
			dt = 0.0f;
		if (dt > MAX_FRAME_TIME)
			dt = MAX_FRAME_TIME;

		m_stats.survivalTime += dt;

		if (!m_enemy.active)
		{
			m_spawnTimer -= dt;
			if (m_spawnTimer <= 0.0f)
			{
				SpawnEnemy();
				result.spawned = true;
			}
			return result;
		}

		m_enemy.x -= m_enemy.speed * dt;
		if (m_enemy.x <= HIT_LINE_X)             // "<=": a large dt may jump past the exact line
		{
			m_enemy.active = false;              // the enemy disappears
			--m_stats.hp;
			m_stats.combo = 0;
			result.leaked = true;
			if (m_stats.hp <= 0)
			{
				m_stats.hp = 0;
				m_state = GameState::GameOver;   // no new enemy, the clock stops
				result.gameOver = true;
			}
			else
			{
				StartWaiting();
			}
		}
		return result;
	}

	void PracticeSession::StartWaiting()
	{
		m_spawnTimer = DifficultyAt(m_stats.survivalTime).challengeDelay;
	}

	void PracticeSession::SpawnEnemy()
	{
		// uniform pick among the enemies that do not require the previous target again
		int candidates[ENEMY_TYPE_COUNT];
		int count = 0;
		for (int i = 0; i < ENEMY_TYPE_COUNT; ++i)
		{
			if (kEnemies[i].targetSkill != m_lastTarget)
				candidates[count++] = i;
		}
		int pick = candidates[NextRandom() % static_cast<unsigned>(count)];
		const EnemyDefinition& def = kEnemies[pick];

		m_enemy.active = true;
		m_enemy.definition = pick;
		m_enemy.target = def.targetSkill;
		m_enemy.x = SPAWN_X;
		m_enemy.speed = DifficultyAt(m_stats.survivalTime).enemySpeed * def.speedMultiplier;  // fixed at spawn
		m_lastTarget = def.targetSkill;
		++m_spawnCount;
	}

	unsigned PracticeSession::NextRandom()
	{
		// xorshift32: tiny, deterministic and identical on every platform
		unsigned x = m_rng;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		m_rng = x;
		return x;
	}
}
