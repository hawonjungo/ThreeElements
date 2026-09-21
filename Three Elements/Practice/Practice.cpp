#include "Practice.h"

#include <cmath>

namespace practice
{
	namespace
	{
		using invoker::SkillId;

		// The 10 enemies. Sprite, frame count and body/feet measurements come from the sheets in
		// assets/enemies (measured on the drawn, horizontally flipped frames, over all animation frames). The
		// skill each enemy requires is plain data: change `targetSkill` here to remap, nothing else depends on it.
		const EnemyDefinition kEnemies[ENEMY_TYPE_COUNT] =
		{
			// id  name        sprite                            frames left feet  w  top bottom  target                    speed
			{ 0, "goblin",   "assets/enemies/goblin_run.png",    8,  56, 100, 38, 63, 100, SkillId::Alacrity,       1.0f },
			{ 1, "skeleton", "assets/enemies/skeleton.png",      4,  45, 100, 45, 50, 100, SkillId::ColdSnap,       1.0f },
			{ 2, "fire wiz", "assets/enemies/fire_wiz.png",      8,  52, 100, 52, 33, 100, SkillId::SunStrike,      1.0f },
			{ 3, "dark wiz", "assets/enemies/dark_wiz.png",      8,  54, 100, 43, 59,  99, SkillId::ChaosMeteor,    1.0f },
			{ 4, "eyes",     "assets/enemies/eyes_fly.png",      8,  52, 100, 42, 60,  92, SkillId::GhostWalk,      1.0f },
			{ 5, "mushroom", "assets/enemies/mushroom_run.png",  8,  62, 100, 26, 62, 100, SkillId::ForgeSpirit,    1.0f },
			{ 6, "necro",    "assets/enemies/nec_walk.png",     10,  45, 100, 51, 19,  99, SkillId::DeafeningBlast, 1.0f },
			{ 7, "worm",     "assets/enemies/worm_run.png",      9,  35, 100, 88, 24,  96, SkillId::Tornado,        1.0f },
			{ 8, "kitsune",  "assets/enemies/kitsune_run.png",   8,  54, 127, 57, 44, 127, SkillId::EMP,            1.0f },
			{ 9, "knight",   "assets/enemies/knight_run.png",    8,  24,  43, 31, 14,  43, SkillId::IceWall,        1.0f }
		};
	}

	const EnemyDefinition& GetEnemyDefinition(int index)
	{
		return kEnemies[index];
	}

	Bounds EnemyBounds(const ActiveEnemy& enemy)
	{
		const EnemyDefinition& def = kEnemies[enemy.definition];
		Bounds b;
		b.x = enemy.x;  // enemy.x is already the left edge of the visible body
		b.y = GROUND_LINE_Y - static_cast<float>(def.feetRow) + static_cast<float>(def.bodyTop);
		b.w = static_cast<float>(def.bodyWidth);
		b.h = static_cast<float>(def.bodyBottom - def.bodyTop + 1);
		return b;
	}

	// ---- Tornado projectile (pure functions: no rendering, no clock) ----

	Tornado MakeTornado(float originX, float originY, float dx, float dy, int enemyId)
	{
		Tornado t;
		t.active = true;
		t.x = originX;
		t.y = originY;
		float length = std::sqrt(dx * dx + dy * dy);
		if (length > 1e-4f)
		{
			t.dirX = dx / length;  // the direction is decided here, once
			t.dirY = dy / length;
		}
		else
		{
			t.dirX = 1.0f;         // deterministic fallback: straight right
			t.dirY = 0.0f;
		}
		t.travelled = 0.0f;
		t.animTime = 0.0f;
		t.enemyId = enemyId;
		return t;
	}

	void AdvanceTornado(Tornado& t, float dt)
	{
		if (!t.active)
			return;
		if (dt < 0.0f)
			dt = 0.0f;

		float step = TORNADO_SPEED * dt;
		t.x += t.dirX * step;
		t.y += t.dirY * step;
		t.travelled += step;
		t.animTime += dt;

		bool outside = t.x < -TORNADO_FIELD_MARGIN || t.x > FIELD_WIDTH + TORNADO_FIELD_MARGIN
			|| t.y < -TORNADO_FIELD_MARGIN || t.y > FIELD_HEIGHT + TORNADO_FIELD_MARGIN;
		if (t.travelled >= TORNADO_MAX_DISTANCE || outside)
			t.active = false;
	}

	bool TornadoHits(const Tornado& t, const Bounds& b)
	{
		if (!t.active)
			return false;

		// closest point of the box to the projectile centre
		float cx = t.x < b.x ? b.x : (t.x > b.x + b.w ? b.x + b.w : t.x);
		float cy = t.y < b.y ? b.y : (t.y > b.y + b.h ? b.y + b.h : t.y);
		float dx = t.x - cx;
		float dy = t.y - cy;
		return dx * dx + dy * dy <= TORNADO_HIT_RADIUS * TORNADO_HIT_RADIUS;
	}

	int TornadoFrame(float animTime)
	{
		if (animTime < 0.0f)
			animTime = 0.0f;
		float frame = std::fmod(animTime * TORNADO_ANIM_FPS, static_cast<float>(TORNADO_FRAME_COUNT));
		return static_cast<int>(frame);
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

	// Everything a session owns goes back to its initial value; only the best combo record survives.
	void PracticeSession::ResetSession()
	{
		int record = m_stats.bestCombo;
		m_stats = { START_HP, START_HP, 0, 0, 0, 0, 0, 0.0f };
		m_stats.bestCombo = record;
		m_enemy = { false, 0, SkillId::None, 0.0f, 0.0f };
		m_tornadoes.clear();                     // projectiles in flight disappear with the session
		m_invoker.Reset();                       // orbs and D/F slots
		m_lastTarget = SkillId::None;
		m_spawnCount = 0;
		m_spawnTimer = 0.0f;
	}

	void PracticeSession::Start(unsigned seed)
	{
		ResetSession();
		m_rng = seed != 0 ? seed : 1u;
		StartWaiting();                          // difficulty clock is survivalTime = 0
		m_state = GameState::Playing;
	}

	void PracticeSession::ReturnToReady()
	{
		ResetSession();
		m_state = GameState::Ready;
	}

	bool PracticeSession::PressEnter(unsigned seed)
	{
		if (m_state == GameState::Playing)       // no accidental restart in the middle of a session
			return false;
		Start(seed);
		return true;
	}

	bool PracticeSession::PressEscape()
	{
		if (m_state == GameState::Ready)
			return true;                         // nothing left to go back to: quit
		ReturnToReady();                         // Playing and Game Over step back to Ready, never quit
		return false;
	}

	InputResult PracticeSession::Input(invoker::InputAction action)
	{
		InputResult result;
		result.accepted = false;
		result.invoker = { invoker::InvokerEvent::InvokeIgnored, SkillId::None };
		result.cast = CastOutcome::None;
		result.tornadoLaunched = false;

		if (m_state != GameState::Playing)
			return result;

		result.accepted = true;
		result.invoker = m_invoker.Apply(action);

		// Only a cast of a filled slot while an enemy is active is judged.
		// Empty slots and casts with no enemy are ignored: no penalty, not counted.
		if (result.invoker.event == invoker::InvokerEvent::Cast)
		{
			if (result.invoker.skill == SkillId::Tornado)
			{
				// A projectile spell: nothing is judged now. UpdateTornadoes judges it if it reaches the enemy.
				if (m_enemy.active)
				{
					Bounds body = EnemyBounds(m_enemy);
					result.tornadoLaunched = LaunchTornado(body.x + body.w * 0.5f - PLAYER_CAST_X,
						body.y + body.h * 0.5f - PLAYER_CAST_Y);
				}
			}
			else
			{
				result.cast = JudgeCast(result.invoker.skill);
			}
		}
		return result;
	}

	// The single definition of a right / wrong spell against the active enemy, and what each one scores.
	CastOutcome PracticeSession::JudgeCast(SkillId spell)
	{
		if (!m_enemy.active)
			return CastOutcome::None;

		if (spell == m_enemy.target)
		{
			++m_stats.correctCasts;
			++m_stats.score;
			++m_stats.combo;
			if (m_stats.combo > m_stats.bestCombo)
				m_stats.bestCombo = m_stats.combo;
			m_enemy.active = false;              // the enemy disappears
			StartWaiting();
			return CastOutcome::Correct;
		}

		++m_stats.incorrectCasts;                // no damage, no HP loss, combo untouched, enemy keeps coming
		return CastOutcome::Incorrect;
	}

	bool PracticeSession::LaunchTornado(float dx, float dy)
	{
		if (m_state != GameState::Playing || !m_enemy.active)
			return false;                        // nothing to aim at: no projectile

		m_tornadoes.push_back(MakeTornado(PLAYER_CAST_X, PLAYER_CAST_Y, dx, dy, m_spawnCount));
		return true;
	}

	// Moves the projectiles and judges the ones that hit the enemy they were launched at.
	// A projectile that hit or left the play area is erased at the end, so the list only holds live ones.
	void PracticeSession::UpdateTornadoes(float dt, UpdateResult& result)
	{
		const float maxStepTime = TORNADO_MAX_STEP / TORNADO_SPEED;  // hit test at least every TORNADO_MAX_STEP px
		for (size_t i = 0; i < m_tornadoes.size(); ++i)
		{
			Tornado& t = m_tornadoes[i];
			float remaining = dt;
			while (t.active && remaining > 0.0f)
			{
				float step = remaining < maxStepTime ? remaining : maxStepTime;
				AdvanceTornado(t, step);
				remaining -= step;

				// Only the enemy it was launched at can be hit, and only while that enemy is still there.
				if (t.active && m_enemy.active && t.enemyId == m_spawnCount && TornadoHits(t, EnemyBounds(m_enemy)))
				{
					t.active = false;            // a hit is used up: one projectile resolves at most once
					result.cast = JudgeCast(SkillId::Tornado);
				}
			}
		}

		size_t kept = 0;
		for (size_t i = 0; i < m_tornadoes.size(); ++i)
		{
			if (m_tornadoes[i].active)
				m_tornadoes[kept++] = m_tornadoes[i];
		}
		m_tornadoes.erase(m_tornadoes.begin() + kept, m_tornadoes.end());
	}

	UpdateResult PracticeSession::Update(float dt)
	{
		UpdateResult result = { false, false, false, CastOutcome::None };
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
		}
		else
		{
			m_enemy.x -= m_enemy.speed * dt;
		}

		// Projectiles are resolved before the leak test: a Tornado that reaches the enemy on the same update wins.
		UpdateTornadoes(dt, result);

		if (m_enemy.active && m_enemy.x <= HIT_LINE_X)  // "<=": a large dt may jump past the exact line
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
				m_tornadoes.clear();            // nothing keeps flying behind the Game Over screen
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
