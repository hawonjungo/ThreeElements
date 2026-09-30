#include "Boss.h"

#include <cmath>

using invoker::SkillId;

namespace practice
{
	namespace
	{
		// B-9. The enemy indices are those of kEnemies in Practice.cpp (9 knight, 3 dark wiz, 8 kitsune).
		const BossDefinition kBosses[BOSS_COUNT] =
		{
			{ "STONE KNIGHT", 9, 3.0f, { 190, 200, 215 },
				{ SkillId::Tornado, SkillId::SunStrike, SkillId::None, SkillId::None }, 2, 40.0f, 1.2f, true },
			{ "DARK WIZARD", 3, 2.5f, { 200, 150, 255 },
				{ SkillId::Tornado, SkillId::ChaosMeteor, SkillId::DeafeningBlast, SkillId::None }, 3, 45.0f, 1.0f, false },
			{ "KITSUNE QUEEN", 8, 1.8f, { 255, 150, 120 },
				{ SkillId::Tornado, SkillId::EMP, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 4, 50.0f, 1.0f, false },
		};

		const float PROJECTILE_STEP_TIME = TORNADO_MAX_STEP / TORNADO_SPEED;  // hit test at least every 10 px
	}

	float BossSpellDelay(SkillId skill)
	{
		switch (skill)
		{
		case SkillId::SunStrike:   return BOSS_SUN_STRIKE_DELAY;
		case SkillId::ChaosMeteor: return BOSS_METEOR_DELAY;
		case SkillId::EMP:         return BOSS_EMP_DELAY;
		default:                   return 0.0f;
		}
	}

	float BossSpellRadius(SkillId skill)
	{
		switch (skill)
		{
		case SkillId::SunStrike:   return BOSS_SUN_STRIKE_RADIUS;
		case SkillId::ChaosMeteor: return BOSS_METEOR_RADIUS;
		case SkillId::EMP:         return BOSS_EMP_RADIUS;
		default:                   return 0.0f;
		}
	}

	float BossLiftHeight(float airTime)
	{
		const float rise = 0.3f;  // s going up
		const float fall = 0.3f;  // s coming down
		if (airTime <= 0.0f || airTime >= BOSS_LIFT_TIME)
			return 0.0f;
		float bob = 6.0f * std::sin(airTime * 6.0f);
		if (airTime < rise)
			return BOSS_LIFT_HEIGHT * (airTime / rise);
		if (airTime > BOSS_LIFT_TIME - fall)
			return BOSS_LIFT_HEIGHT * ((BOSS_LIFT_TIME - airTime) / fall);
		return BOSS_LIFT_HEIGHT + bob;
	}

	HitGrade GradeHit(float afterLanding, float window)
	{
		if (afterLanding < 0.0f || afterLanding > window)
			return HitGrade::Miss;
		if (afterLanding <= BOSS_PERFECT_TIME)
			return HitGrade::Perfect;
		if (afterLanding <= BOSS_GREAT_TIME)
			return HitGrade::Great;
		return HitGrade::Good;
	}

	int GradeScore(HitGrade grade)
	{
		switch (grade)
		{
		case HitGrade::Perfect: return BOSS_SCORE_PERFECT;
		case HitGrade::Great:   return BOSS_SCORE_GREAT;
		case HitGrade::Good:    return BOSS_SCORE_GOOD;
		default:                return 0;
		}
	}

	const BossDefinition& GetBossDefinition(int index)
	{
		if (index < 0 || index >= BOSS_COUNT)
			index = 0;
		return kBosses[index];
	}

	BossSession::BossSession()
	{
		Start(0);
		m_state = BossState::Lost;  // nothing is being fought until Start()
	}

	void BossSession::Start(int boss)
	{
		m_boss = boss >= 0 && boss < BOSS_COUNT ? boss : 0;
		m_state = BossState::Fighting;
		m_bossHp = BOSS_FULL_HP;
		m_playerHp = START_HP;
		m_elapsed = 0.0f;
		m_assisted = false;
		m_invoker.Reset();

		m_x = BOSS_START_X;
		m_phase = BossPhase::Walking;
		m_airTime = 0.0f;
		m_pushTarget = m_x;
		m_liftAttempt = 0;
		m_liftStart = 0.0f;

		m_attempt = 0;
		m_running = false;
		m_attemptStart = 0.0f;
		m_castIndex = 0;
		m_tornadoHit = false;
		for (int i = 0; i < BOSS_MAX_COMBO; ++i)
		{
			m_grades[i] = HitGrade::None;
			m_missReasons[i] = ComboFail::None;
		}
		m_landed = false;
		m_landTime = 0.0f;
		m_lastFail = ComboFail::None;
		m_lastDamage = 0;

		m_projectiles.clear();
		m_pendingCount = 0;
	}

	Bounds BossSession::Body() const
	{
		const BossDefinition& def = Def();
		const EnemyDefinition& e = GetEnemyDefinition(def.enemyDefinition);
		Bounds b;
		b.x = m_x;
		b.y = GROUND_LINE_Y - static_cast<float>(e.feetRow - e.bodyTop) * def.scale;
		b.w = static_cast<float>(e.bodyWidth) * def.scale;
		b.h = static_cast<float>(e.bodyBottom - e.bodyTop + 1) * def.scale;
		return b;
	}

	float BossSession::CenterX() const
	{
		Bounds b = Body();
		return b.x + b.w * 0.5f;
	}

	bool BossSession::StepDone(int step) const
	{
		if (!m_running || step < 0 || step >= Def().comboLength)
			return false;
		return step == 0 ? m_tornadoHit : m_grades[step] != HitGrade::None;
	}

	HitGrade BossSession::StepGrade(int step) const
	{
		return m_running && step > 0 && step < Def().comboLength ? m_grades[step] : HitGrade::None;
	}

	float BossSession::ImpactDelay(SkillId skill) const
	{
		float delay = BossSpellDelay(skill);
		if (delay > 0.0f)
			return delay;
		if (skill == SkillId::DeafeningBlast || skill == SkillId::Tornado)
		{
			float distance = m_x - PLAYER_CAST_X;
			return distance > 0.0f ? distance / TORNADO_SPEED : 0.0f;
		}
		return -1.0f;
	}

	bool BossSession::LandingTime(float& when) const
	{
		if (!m_running)
			return false;
		if (m_tornadoHit)
		{
			when = m_liftStart + BOSS_LIFT_TIME;
			return true;
		}
		// the attempt's Tornado is still on its way: predict when it meets the boss walking toward it
		for (const BossProjectile& p : m_projectiles)
		{
			if (p.skill != SkillId::Tornado || p.attempt != m_attempt || p.resolved || !p.motion.active)
				continue;
			float closing = TORNADO_SPEED * p.motion.dirX + (m_phase == BossPhase::Walking ? Def().speed : 0.0f);
			if (closing <= 0.0f)
				return false;
			float distance = Body().x - (p.motion.x + TORNADO_HIT_RADIUS);
			when = m_elapsed + (distance > 0.0f ? distance : 0.0f) / closing + BOSS_LIFT_TIME;
			return true;
		}
		return false;
	}

	bool BossSession::StepTiming(int step, float& untilIdeal, float& span) const
	{
		const BossDefinition& def = Def();
		float landing = 0.0f;
		if (m_state != BossState::Fighting || step < 1 || step >= def.comboLength || step < m_castIndex
			|| !LandingTime(landing))
			return false;
		float delay = ImpactDelay(def.combo[step]);
		if (delay < 0.0f)
			return false;
		float ideal = landing + BOSS_IDEAL_AFTER_LANDING - delay;
		untilIdeal = ideal - m_elapsed;
		span = ideal - m_attemptStart;
		if (span < 0.1f)
			span = 0.1f;
		return true;
	}

	bool BossSession::CueNow() const
	{
		const BossDefinition& def = Def();
		if (!def.guided || m_state != BossState::Fighting || !m_running || m_phase != BossPhase::Airborne
			|| m_liftAttempt != m_attempt || m_castIndex >= def.comboLength)
			return false;
		float delay = ImpactDelay(def.combo[m_castIndex]);
		if (delay < 0.0f)
			return false;
		float impact = m_airTime + delay;  // in "air time", the boss lands at BOSS_LIFT_TIME
		return impact >= BOSS_LIFT_TIME && impact <= BOSS_LIFT_TIME + BOSS_GREAT_TIME;
	}

	void BossSession::Fail(ComboFail reason, ComboFail& report)
	{
		if (!m_running)
			return;
		m_running = false;
		m_lastFail = reason;
		m_lastDamage = 0;
		if (report == ComboFail::None)
			report = reason;
	}

	BossInputResult BossSession::Input(invoker::InputAction action)
	{
		BossInputResult result = { false, { invoker::InvokerEvent::OrbAdded, SkillId::None }, false, ComboFail::None };
		if (m_state != BossState::Fighting)
			return result;
		result.accepted = true;
		result.invoker = m_invoker.Apply(action);
		if (result.invoker.event == invoker::InvokerEvent::Cast)
			Cast(result.invoker.skill, result);
		return result;
	}

	// B-7 steps 1 and 2: which attempt a cast belongs to, then what it sets in motion.
	void BossSession::Cast(SkillId skill, BossInputResult& result)
	{
		const BossDefinition& def = Def();
		int tag = 0;
		if (m_running)
		{
			if (m_castIndex < def.comboLength && skill == def.combo[m_castIndex])
			{
				tag = m_attempt;
				++m_castIndex;
			}
			else if (m_castIndex >= def.comboLength || skill == def.combo[m_castIndex - 1])
				tag = 0;  // the whole list is cast already, or a double tap of the last spell: ignored
			else
				Fail(ComboFail::WrongSpell, result.fail);
		}
		else if (skill == def.combo[0] && m_phase != BossPhase::Airborne)
		{
			++m_attempt;
			m_running = true;
			m_attemptStart = m_elapsed;
			m_castIndex = 1;
			m_tornadoHit = false;
			for (int i = 0; i < BOSS_MAX_COMBO; ++i)
			{
				m_grades[i] = HitGrade::None;
				m_missReasons[i] = ComboFail::None;
			}
			m_landed = false;
			m_lastFail = ComboFail::None;
			tag = m_attempt;
			result.attemptStarted = true;
		}

		if (skill == SkillId::Tornado || skill == SkillId::DeafeningBlast)
		{
			Bounds b = Body();
			BossProjectile p;
			p.skill = skill;
			p.motion = MakeTornado(PLAYER_CAST_X, PLAYER_CAST_Y, b.x + b.w * 0.5f - PLAYER_CAST_X,
				b.y + b.h * 0.5f - PLAYER_CAST_Y, 0);
			p.attempt = tag;
			p.resolved = false;
			m_projectiles.push_back(p);
		}
		else if (BossSpellDelay(skill) > 0.0f && m_pendingCount < BOSS_MAX_PENDING)
		{
			PendingSpell& p = m_pending[m_pendingCount++];
			p.skill = skill;
			p.x = CenterX();
			p.delay = p.left = BossSpellDelay(skill);
			p.attempt = tag;
		}
	}

	// B-7 step 3 and B-14: a follow-up spell reached the boss (or the ground near it) and gets its grade.
	void BossSession::JudgeImpact(SkillId skill, float x, int attempt, bool projectile, BossUpdateResult& result)
	{
		BossImpact impact = { skill, x, HitGrade::None, ComboFail::None };
		if (m_running && attempt == m_attempt)
		{
			const BossDefinition& def = Def();
			int step = -1;
			for (int i = 1; i < def.comboLength; ++i)
			{
				if (def.combo[i] == skill && m_grades[i] == HitGrade::None)
				{
					step = i;
					break;
				}
			}
			if (step > 0)
			{
				float reach = BossSpellRadius(skill) + Body().w * 0.5f;
				if (!m_landed)
				{
					impact.grade = HitGrade::Miss;
					impact.miss = ComboFail::TooEarly;
				}
				else if (!projectile && std::fabs(x - CenterX()) > reach)
				{
					impact.grade = HitGrade::Miss;
					impact.miss = ComboFail::Missed;
				}
				else
				{
					impact.grade = GradeHit(m_elapsed - m_landTime, def.window);
					if (impact.grade == HitGrade::Miss)
						impact.miss = ComboFail::TooLate;
				}
				m_grades[step] = impact.grade;
				m_missReasons[step] = impact.miss;
			}
		}
		if (result.impactCount < 4)
			result.impacts[result.impactCount++] = impact;
	}

	// B-7 step 4: every follow-up has its grade (or the window has closed): the damage is their average score.
	void BossSession::Resolve(BossUpdateResult& result)
	{
		const BossDefinition& def = Def();
		int followUps = def.comboLength - 1;
		int total = 0;
		ComboFail firstMiss = ComboFail::None;
		for (int i = 1; i < def.comboLength; ++i)
		{
			total += GradeScore(m_grades[i]);
			if (m_grades[i] == HitGrade::Miss && firstMiss == ComboFail::None)
				firstMiss = m_missReasons[i];
		}
		int damage = (total + followUps / 2) / followUps;
		if (damage <= 0)
		{
			Fail(firstMiss != ComboFail::None ? firstMiss : ComboFail::TooLate, result.fail);
			return;
		}
		m_running = false;
		m_lastFail = ComboFail::None;
		m_lastDamage = damage;
		result.comboComplete = true;
		result.damage = damage;
		m_bossHp -= damage;
		if (m_bossHp <= 0)
		{
			m_bossHp = 0;
			m_state = BossState::Won;
			result.won = true;
			return;
		}
		if (m_phase == BossPhase::Walking)
		{
			float target = m_x + BOSS_PUSHBACK;
			m_pushTarget = target > BOSS_RESET_X ? (m_x > BOSS_RESET_X ? m_x : BOSS_RESET_X) : target;
			m_phase = BossPhase::PushedBack;
		}
	}

	BossUpdateResult BossSession::Update(float dt)
	{
		BossUpdateResult result = {};
		if (m_state != BossState::Fighting)
			return result;
		if (dt < 0.0f)
			dt = 0.0f;
		if (dt > MAX_FRAME_TIME)
			dt = MAX_FRAME_TIME;
		const BossDefinition& def = Def();
		m_elapsed += dt;

		// ---- the boss moves: walks, floats, or slides back
		if (m_phase == BossPhase::Airborne)
		{
			m_airTime += dt;
			if (m_airTime >= BOSS_LIFT_TIME)
			{
				m_phase = BossPhase::Walking;
				m_airTime = 0.0f;
				result.landed = true;
				if (m_running && m_liftAttempt == m_attempt)
				{
					m_landed = true;
					m_landTime = m_elapsed;
				}
			}
		}
		else if (m_phase == BossPhase::PushedBack)
		{
			m_x += BOSS_PUSH_SPEED * dt;
			if (m_x >= m_pushTarget)
			{
				m_x = m_pushTarget;
				m_phase = BossPhase::Walking;
			}
		}
		else
			m_x -= def.speed * dt;

		// ---- projectiles: a Tornado lifts a grounded boss, a Deafening Blast is graded when it reaches it
		for (size_t i = 0; i < m_projectiles.size(); ++i)
		{
			BossProjectile& p = m_projectiles[i];
			float remaining = dt;
			while (p.motion.active && remaining > 0.0f)
			{
				float step = remaining < PROJECTILE_STEP_TIME ? remaining : PROJECTILE_STEP_TIME;
				AdvanceTornado(p.motion, step);
				remaining -= step;
				if (p.resolved || !p.motion.active || !TornadoHits(p.motion, Body()))
					continue;
				p.resolved = true;
				if (p.skill == SkillId::Tornado)
				{
					if (m_phase == BossPhase::Airborne)
						continue;  // invulnerable in the air: the Tornado passes through
					p.motion.active = false;
					m_phase = BossPhase::Airborne;
					m_airTime = 0.0f;
					m_liftAttempt = p.attempt;
					result.lifted = true;
					if (m_running && p.attempt == m_attempt)
					{
						m_tornadoHit = true;
						m_liftStart = m_elapsed;
					}
				}
				else
					JudgeImpact(p.skill, p.motion.x, p.attempt, true, result);  // in the air: TOO EARLY
			}
			// the running attempt's Tornado left without lifting the boss
			if (!p.motion.active && p.skill == SkillId::Tornado && !p.resolved && m_running && p.attempt == m_attempt)
				Fail(ComboFail::Missed, result.fail);
		}
		size_t kept = 0;
		for (size_t i = 0; i < m_projectiles.size(); ++i)
			if (m_projectiles[i].motion.active)
				m_projectiles[kept++] = m_projectiles[i];
		m_projectiles.erase(m_projectiles.begin() + kept, m_projectiles.end());

		// ---- delayed spells land
		int keptPending = 0;
		for (int i = 0; i < m_pendingCount; ++i)
		{
			PendingSpell p = m_pending[i];
			p.left -= dt;
			if (p.left > 0.0f)
				m_pending[keptPending++] = p;
			else
				JudgeImpact(p.skill, p.x, p.attempt, false, result);
		}
		m_pendingCount = keptPending;

		// ---- the attempt ends: every follow-up graded, or the window closed on the rest (TOO LATE)
		if (m_running)
		{
			bool allGraded = true;
			for (int i = 1; i < def.comboLength; ++i)
				allGraded = allGraded && m_grades[i] != HitGrade::None;
			if (!allGraded && m_landed && m_elapsed > m_landTime + def.window)
			{
				for (int i = 1; i < def.comboLength; ++i)
				{
					if (m_grades[i] == HitGrade::None)
					{
						m_grades[i] = HitGrade::Miss;
						m_missReasons[i] = ComboFail::TooLate;
					}
				}
				allGraded = true;
			}
			if (allGraded)
			{
				Resolve(result);
				if (m_state == BossState::Won)
					return result;
			}
		}

		// ---- the boss reaches the player (B-3)
		if (m_phase == BossPhase::Walking && m_x <= HIT_LINE_X)
		{
			result.playerHit = true;
			m_running = false;
			--m_playerHp;
			if (m_playerHp <= 0)
			{
				m_state = BossState::Lost;
				result.lost = true;
				return result;
			}
			m_pushTarget = BOSS_RESET_X;
			m_phase = BossPhase::PushedBack;
		}
		return result;
	}
}
