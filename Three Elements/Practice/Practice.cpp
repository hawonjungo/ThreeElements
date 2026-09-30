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
		float s = enemy.scale > 0.0f ? enemy.scale : 1.0f;  // PLAY elites and bosses are larger
		Bounds b;
		b.x = enemy.x;  // enemy.x is already the left edge of the visible body
		b.y = GROUND_LINE_Y - static_cast<float>(def.feetRow - def.bodyTop) * s;
		b.w = static_cast<float>(def.bodyWidth) * s;
		b.h = static_cast<float>(def.bodyBottom - def.bodyTop + 1) * s;
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

	float EliteChance(float elapsedSeconds)
	{
		if (elapsedSeconds <= 0.0f)
			return 0.0f;
		float c = PLAY_ELITE_CHANCE_MAX * elapsedSeconds / PLAY_ELITE_RAMP_TIME;
		return c > PLAY_ELITE_CHANCE_MAX ? PLAY_ELITE_CHANCE_MAX : c;
	}

	// ------------------------------------------------------------------ records

	BestUpdate MergeBests(BestStats& bests, const Stats& session)
	{
		BestUpdate changed = { false, false, false };
		if (session.assisted)  // played with the recipe hint: never a record
			return changed;
		if (session.score > bests.score)              { bests.score = session.score;               changed.score = true; }
		if (session.bestCombo > bests.combo)           { bests.combo = session.bestCombo;           changed.combo = true; }
		if (session.survivalTime > bests.survivalTime) { bests.survivalTime = session.survivalTime; changed.survivalTime = true; }
		return changed;
	}

	int InsertTopRun(TopRun* list, const TopRun& run)
	{
		if (run.survivalTime <= 0.0f)
			return 0;
		auto better = [](const TopRun& a, const TopRun& b)
		{
			return a.survivalTime > b.survivalTime || (a.survivalTime == b.survivalTime && a.score > b.score);
		};
		int pos = TOP_RUNS;
		while (pos > 0 && better(run, list[pos - 1]))
			--pos;
		if (pos >= TOP_RUNS)
			return 0;
		for (int i = TOP_RUNS - 1; i > pos; --i)
			list[i] = list[i - 1];
		list[pos] = run;
		return pos + 1;
	}

	int InsertPlayRun(PlayRun* list, const PlayRun& run)
	{
		if (run.score <= 0)
			return 0;
		auto better = [](const PlayRun& a, const PlayRun& b)
		{
			if (a.score != b.score)
				return a.score > b.score;
			if (a.stage != b.stage)
				return a.stage > b.stage;
			return a.time > b.time;
		};
		int pos = TOP_RUNS;
		while (pos > 0 && better(run, list[pos - 1]))
			--pos;
		if (pos >= TOP_RUNS)
			return 0;
		for (int i = TOP_RUNS - 1; i > pos; --i)
			list[i] = list[i - 1];
		list[pos] = run;
		return pos + 1;
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
		m_recordAtStart = record;                // what MarkAssisted() puts back if this run turns out assisted
		m_enemy = { false, 0, SkillId::None, 0.0f, 0.0f };
		m_frostLeft = m_doubleLeft = 0.0f;       // PLAY runes end with the session
		m_shield = false;
		for (int i = 0; i < ITEM_COUNT; ++i)     // item cooldowns and effects restart with every run
			m_itemCooldown[i] = m_itemCooldownTotal[i] = 0.0f;
		m_backLeft = m_stillLeft = m_bkbLeft = m_smokeLeft = 0.0f;
		m_runeChoiceCount = 0;
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
		result.kill = {};

		if (m_state != GameState::Playing || m_runeChoiceCount > 0)  // the rune choice pauses the run
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
				m_kill = {};
				result.cast = JudgeCast(result.invoker.skill);
				result.kill = m_kill;
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
			++m_stats.combo;
			if (!m_stats.assisted && m_stats.combo > m_stats.bestCombo)  // a hinted run sets no record
				m_stats.bestCombo = m_stats.combo;

			// PLAY chains (P3-3): a correct cast breaks the current skill; the enemy lives on until the last one
			if (m_enemy.chainStep + 1 < m_enemy.chainLength)
			{
				++m_enemy.chainStep;
				m_enemy.target = m_enemy.chain[m_enemy.chainStep];
				return CastOutcome::Correct;
			}

			m_kill = { true, m_enemy.kind, 1, 0, Rune::None };
			if (m_mode == SessionMode::Play)
			{
				m_kill.points = m_enemy.kind == EnemyKind::Boss ? PLAY_POINTS_BOSS
					: m_enemy.kind == EnemyKind::Elite ? PLAY_POINTS_ELITE : PLAY_POINTS_NORMAL;
				if (m_doubleLeft > 0.0f)
					m_kill.points *= 2;
				m_kill.gold = AddGold(m_enemy.kind == EnemyKind::Boss ? PLAY_GOLD_BOSS
					: m_enemy.kind == EnemyKind::Elite ? PLAY_GOLD_ELITE : 0);
				if (m_enemy.kind == EnemyKind::Boss)
				{
					++m_stats.bossesDefeated;
					int choices = static_cast<int>(GetItemDefinition(ItemId::Aghanim).level[0].value);
					int aghanim = EquippedLevel(m_inv, ItemId::Aghanim);
					if (aghanim > 0)  // §27 I-5: the player chooses (the run waits)
					{
						choices = static_cast<int>(GetItemDefinition(ItemId::Aghanim).level[aghanim - 1].value);
						m_runeChoiceCount = 0;
						for (int guard = 0; guard < 50 && m_runeChoiceCount < choices; ++guard)
						{
							Rune r = PickRune();
							bool seen = false;
							for (int k = 0; k < m_runeChoiceCount; ++k)
								seen = seen || m_runeChoices[k] == r;
							if (!seen)
								m_runeChoices[m_runeChoiceCount++] = r;
						}
						m_kill.runeChoice = true;
					}
					else
					{
						int before = m_stats.gold;
						m_kill.rune = ApplyRune(PickRune());
						m_kill.gold += m_stats.gold - before;  // Bounty
					}
				}
			}
			m_stats.score += m_kill.points;          // Survival: +1 per enemy, as always
			++m_stats.kills;
			m_enemy.active = false;              // the enemy disappears
			StartWaiting();
			return CastOutcome::Correct;
		}

		++m_stats.incorrectCasts;                // no damage, no HP loss, combo untouched, enemy keeps coming
		return CastOutcome::Incorrect;
	}

	Rune PracticeSession::PickRune()
	{
		Rune pool[5];
		int count = 0;
		if (m_stats.hp < PLAY_MAX_HP)
			pool[count++] = Rune::Regeneration;
		pool[count++] = Rune::Frost;
		pool[count++] = Rune::DoubleDamage;
		pool[count++] = Rune::Bounty;
		pool[count++] = Rune::Shield;
		return pool[NextRandom() % static_cast<unsigned>(count)];
	}

	Rune PracticeSession::ApplyRune(Rune rune)
	{
		switch (rune)
		{
		case Rune::Regeneration:
			if (m_stats.hp < PLAY_MAX_HP)
				++m_stats.hp;
			if (m_stats.hp > m_stats.maxHp)
				m_stats.maxHp = m_stats.hp;
			break;
		case Rune::Frost:        m_frostLeft = PLAY_FROST_TIME; break;
		case Rune::DoubleDamage: m_doubleLeft = PLAY_DOUBLE_TIME; break;
		case Rune::Bounty:       AddGold(PLAY_BOUNTY_GOLD); break;
		case Rune::Shield:       m_shield = true; break;
		default: break;
		}
		return rune;
	}

	int PracticeSession::AddGold(int gold)
	{
		int midas = EquippedLevel(m_inv, ItemId::Midas);
		if (midas > 0 && gold > 0)
			gold = static_cast<int>(gold * (1.0f + GetItemDefinition(ItemId::Midas).level[midas - 1].value) + 0.5f);
		m_stats.gold += gold;
		return gold;
	}

	Rune PracticeSession::ChooseRune(int i)
	{
		if (m_runeChoiceCount <= 0 || i < 0 || i >= m_runeChoiceCount)
			return Rune::None;
		Rune r = m_runeChoices[i];
		m_runeChoiceCount = 0;
		return ApplyRune(r);
	}

	// §27 I-4 / I-5: an item in a slot is used when it is ready and has something to do; nothing is spent otherwise.
	ItemUseResult PracticeSession::UseItem(int slot)
	{
		ItemUseResult result = { false, ItemId::Blink };
		if (m_mode != SessionMode::Play || m_state != GameState::Playing || m_runeChoiceCount > 0
			|| slot < 0 || slot >= ITEM_SLOTS || m_inv.slot[slot] == ITEM_NONE)
			return result;
		int index = m_inv.slot[slot];
		ItemId item = static_cast<ItemId>(index);
		const ItemDefinition& def = GetItemDefinition(item);
		int level = CurrentLevel(m_inv, item);
		result.item = item;
		if (def.kind == ItemKind::Passive || level <= 0 || m_itemCooldown[index] > 0.0f)
			return result;
		const ItemLevel& lv = def.level[level - 1];
		int left = m_enemy.chainLength - m_enemy.chainStep;  // skills of the chain still to break
		switch (item)
		{
		case ItemId::Blink:
			if (!m_enemy.active)
				return result;
			m_backLeft = lv.value;
			m_stillLeft = 0.0f;
			break;
		case ItemId::Euls:
			if (!m_enemy.active)
				return result;
			m_stillLeft = lv.value;
			m_backLeft = 0.0f;
			if (level >= 2)  // Wind Waker
				m_enemy.x = m_enemy.x + PLAY_EULS_PUSHBACK < SPAWN_X ? m_enemy.x + PLAY_EULS_PUSHBACK : SPAWN_X;
			break;
		case ItemId::Refresher:
		{
			if (!m_enemy.active || left <= 1)
				return result;
			int n = static_cast<int>(lv.value) < left - 1 ? static_cast<int>(lv.value) : left - 1;
			m_enemy.chainStep += n;
			m_enemy.target = m_enemy.chain[m_enemy.chainStep];
			break;
		}
		case ItemId::Bkb:
			m_bkbLeft = lv.value;
			break;
		case ItemId::Salve:
		case ItemId::Cheese:
			if (m_stats.hp >= PLAY_MAX_HP)
				return result;
			m_stats.hp += static_cast<int>(lv.value);
			if (m_stats.hp > PLAY_MAX_HP)
				m_stats.hp = PLAY_MAX_HP;
			if (m_stats.hp > m_stats.maxHp)
				m_stats.maxHp = m_stats.hp;
			break;
		case ItemId::Smoke:
		case ItemId::GreaterSmoke:
			m_smokeLeft = lv.value;
			break;
		default:
			return result;
		}
		if (def.kind == ItemKind::Consumable)
			--m_inv.count[index];
		else
		{
			int octarine = EquippedLevel(m_inv, ItemId::Octarine);
			float cut = octarine > 0 ? GetItemDefinition(ItemId::Octarine).level[octarine - 1].value : 0.0f;
			m_itemCooldown[index] = m_itemCooldownTotal[index] = lv.cooldown * (1.0f - cut);
		}
		result.used = true;
		return result;
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
					m_kill = {};
					result.cast = JudgeCast(SkillId::Tornado);
					if (m_kill.killed)
						result.kill = m_kill;
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

		if (m_runeChoiceCount > 0)  // Aghanim: nothing moves until the rune is chosen
			return result;

		m_stats.survivalTime += dt;
		m_frostLeft = m_frostLeft > dt ? m_frostLeft - dt : 0.0f;
		m_doubleLeft = m_doubleLeft > dt ? m_doubleLeft - dt : 0.0f;
		m_backLeft = m_backLeft > dt ? m_backLeft - dt : 0.0f;
		m_stillLeft = m_stillLeft > dt ? m_stillLeft - dt : 0.0f;
		m_bkbLeft = m_bkbLeft > dt ? m_bkbLeft - dt : 0.0f;
		m_smokeLeft = m_smokeLeft > dt ? m_smokeLeft - dt : 0.0f;
		for (int i = 0; i < ITEM_COUNT; ++i)
			m_itemCooldown[i] = m_itemCooldown[i] > dt ? m_itemCooldown[i] - dt : 0.0f;

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
			float speed = m_enemy.speed * (m_frostLeft > 0.0f ? PLAY_FROST_SPEED : 1.0f)
				* (m_smokeLeft > 0.0f ? PLAY_SMOKE_SPEED : 1.0f);
			if (m_stillLeft > 0.0f)
				speed = 0.0f;                    // Eul's: it stands still
			else if (m_backLeft > 0.0f)
				speed = -speed;                  // Blink: it walks back, never past its spawn point
			m_enemy.x -= speed * dt;
			if (m_enemy.x > SPAWN_X)
				m_enemy.x = SPAWN_X;
		}

		// Projectiles are resolved before the leak test: a Tornado that reaches the enemy on the same update wins.
		UpdateTornadoes(dt, result);

		if (m_enemy.active && m_enemy.x <= HIT_LINE_X)  // "<=": a large dt may jump past the exact line
		{
			m_enemy.active = false;              // the enemy disappears
			int damage = m_mode != SessionMode::Play ? 1 : m_enemy.kind == EnemyKind::Boss ? PLAY_LEAK_BOSS
				: m_enemy.kind == EnemyKind::Elite ? PLAY_LEAK_ELITE : PLAY_LEAK_NORMAL;
			if (m_bkbLeft > 0.0f)                // Black King Bar running: this leak costs nothing (§27 I-5)
			{
				damage = 0;
				result.shieldUsed = true;
			}
			else if (m_shield)                   // PLAY Shield rune: this leak costs nothing
			{
				m_shield = false;
				damage = 0;
				result.shieldUsed = true;
			}
			m_stats.hp -= damage;
			result.leakDamage = damage;
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
		m_enemy.kind = EnemyKind::Normal;
		m_enemy.chain[0] = def.targetSkill;
		m_enemy.chainLength = 1;
		m_enemy.chainStep = 0;
		m_enemy.scale = 1.0f;
		m_lastTarget = def.targetSkill;
		++m_spawnCount;

		if (m_mode != SessionMode::Play)
			return;
		// PLAY (P3-2): every 10th enemy is a boss (chain of 3), others may be elites (chain of 2)
		if (m_spawnCount % PLAY_BOSS_EVERY == 0)
			m_enemy.kind = EnemyKind::Boss;
		else if (static_cast<float>(NextRandom() % 1000u) / 1000.0f < EliteChance(m_stats.survivalTime))
			m_enemy.kind = EnemyKind::Elite;
		if (m_enemy.kind == EnemyKind::Normal)
			return;
		bool boss = m_enemy.kind == EnemyKind::Boss;
		m_enemy.chainLength = boss ? 3 : 2;
		m_enemy.speed *= boss ? PLAY_BOSS_SPEED : PLAY_ELITE_SPEED;
		m_enemy.scale = boss ? PLAY_BOSS_SCALE : PLAY_ELITE_SCALE;
		for (int k = 1; k < m_enemy.chainLength; ++k)  // the rest of the chain: random skills not used yet
		{
			SkillId options[invoker::SKILL_COUNT];
			int n = 0;
			for (int s = 0; s < invoker::SKILL_COUNT; ++s)
			{
				bool used = false;
				for (int j = 0; j < k; ++j)
					used = used || m_enemy.chain[j] == static_cast<SkillId>(s);
				if (!used)
					options[n++] = static_cast<SkillId>(s);
			}
			m_enemy.chain[k] = options[NextRandom() % static_cast<unsigned>(n)];
		}
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
