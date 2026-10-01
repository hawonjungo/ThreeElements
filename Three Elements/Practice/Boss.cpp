#include "Boss.h"

#include <cmath>

using invoker::SkillId;

namespace practice
{
	namespace
	{
		// §32 M-6: the five Immortals, by the length of their combo (4, 5, 6, 7, 8), the combos Invoker players use
		// most. The first three have their own art (assets/enemies/<name>/, the body is given here in field pixels);
		// the last two are still an enemy of kEnemies drawn larger and tinted (3 dark wiz, 6 necromancer) until the
		// owner's art is there. The eight bosses of 1.2 / 1.5 are gone from the game; the tests keep them as
		// definitions of their own, to exercise the guided cue, the holds and the two phases.
		const BossDefinition kBosses[BOSS_COUNT] =
		{
			{ "RIMEFANG", 0, 1.0f, { 255, 255, 255 },
				{ SkillId::Tornado, SkillId::EMP, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 4,
				50.0f, 1.0f, false, {}, 0, 230.0f, 140.0f },
			{ "CINDERMAW", 0, 1.0f, { 255, 255, 255 },
				{ SkillId::Tornado, SkillId::EMP, SkillId::SunStrike, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 5,
				50.0f, 1.4f, false, {}, 0, 160.0f, 180.0f },
			{ "GRAVEHORN", 0, 1.0f, { 255, 255, 255 },
				{ SkillId::Tornado, SkillId::SunStrike, SkillId::ChaosMeteor, SkillId::DeafeningBlast, SkillId::ColdSnap,
				  SkillId::ForgeSpirit }, 6,
				45.0f, 1.2f, false, {}, 0, 130.0f, 205.0f },
			{ "VOLTARA", 3, 3.2f, { 190, 215, 255 },
				{ SkillId::Tornado, SkillId::EMP, SkillId::ChaosMeteor, SkillId::DeafeningBlast, SkillId::ChaosMeteor,
				  SkillId::DeafeningBlast, SkillId::IceWall }, 7,
				45.0f, 1.2f, false, {}, 0, 0.0f, 0.0f },
			{ "THE HOLLOW KING", 6, 2.4f, { 255, 235, 200 },
				{ SkillId::ForgeSpirit, SkillId::Alacrity, SkillId::IceWall, SkillId::ColdSnap, SkillId::Tornado,
				  SkillId::SunStrike, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 8,
				40.0f, 1.2f, false, {}, 0, 0.0f, 0.0f },
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

	HitGrade GradeQuick(float sincePrevious)
	{
		if (sincePrevious < 0.0f || sincePrevious > BOSS_QUICK_LIMIT)
			return HitGrade::Miss;
		if (sincePrevious <= BOSS_QUICK_PERFECT)
			return HitGrade::Perfect;
		if (sincePrevious <= BOSS_QUICK_GREAT)
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
		int index = boss >= 0 && boss < BOSS_COUNT ? boss : 0;
		Start(kBosses[index]);
		m_boss = index;
	}

	void BossSession::Start(const BossDefinition& def)
	{
		m_boss = -1;
		m_def = def;
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
		for (int i = 0; i < BOSS_MAX_COMBO; ++i)
			m_castTime[i] = 0.0f;
		m_comboPhase = 0;
		m_freezeLeft = m_slowLeft = m_confuseLeft = 0.0f;
		m_speed = Def().speed;
		m_window = Def().window;
		m_speedFactor = 1.0f;
		m_still = m_walkBack = m_contactBlocked = false;
		m_damageMultiplier = 1;

		m_projectiles.clear();
		m_pendingCount = 0;
	}

	void BossSession::StartImmortal(int boss, int playerHp, const invoker::InvokerState& invoker, float speedScale, float windowScale)
	{
		Start(boss);
		m_playerHp = playerHp;
		m_invoker = invoker;      // the run's orbs and D / F slots carry over (spec §28 O-4)
		m_speed = Def().speed * speedScale;
		m_window = Def().window * windowScale;
		if (m_window < BOSS_MIN_WINDOW)
			m_window = BOSS_MIN_WINDOW;
	}

	void BossSession::SetModifiers(float speedFactor, bool still, bool walkBack, bool contactBlocked, int damageMultiplier)
	{
		m_speedFactor = speedFactor;
		m_still = still;
		m_walkBack = walkBack;
		m_contactBlocked = contactBlocked;
		m_damageMultiplier = damageMultiplier > 1 ? damageMultiplier : 1;
	}

	void BossSession::PushBack(float px)
	{
		if (m_phase == BossPhase::Airborne)
			return;
		m_x = m_x + px < BOSS_START_X ? m_x + px : BOSS_START_X;
	}

	Bounds BossSession::Body() const
	{
		const BossDefinition& def = Def();
		const EnemyDefinition& e = GetEnemyDefinition(def.enemyDefinition);
		Bounds b;
		b.x = m_x;
		if (def.bodyWidth > 0.0f)  // its own art: the body is given
		{
			b.y = GROUND_LINE_Y - def.bodyHeight;
			b.w = def.bodyWidth;
			b.h = def.bodyHeight;
			return b;
		}
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
		if (!m_running || step < 0 || step >= ComboLength())
			return false;
		if (step == 0)  // the opener: a Tornado once it has hit, any other spell at once
			return Combo()[0] == SkillId::Tornado ? m_tornadoHit : true;
		return m_grades[step] != HitGrade::None;
	}

	HitGrade BossSession::StepGrade(int step) const
	{
		return m_running && step > 0 && step < ComboLength() ? m_grades[step] : HitGrade::None;
	}

	// B-16: a landing step is Sun Strike / Chaos Meteor / EMP / Deafening Blast after a Tornado in the same combo;
	// every other follow-up is a quick step. B-21 (1.9, combos of up to eight): a spell catches the landing once, so
	// the second Chaos Meteor or Deafening Blast after the same Tornado (the "Refresher" part of a long combo) is a
	// quick step.
	BossStepKind BossSession::StepKind(int step) const
	{
		if (step <= 0)
			return BossStepKind::Opener;
		const SkillId* combo = Combo();
		SkillId s = combo[step];
		if (BossSpellDelay(s) <= 0.0f && s != SkillId::DeafeningBlast)
			return BossStepKind::Quick;
		int tornado = -1;
		for (int i = 0; i < step; ++i)
			if (combo[i] == SkillId::Tornado)
				tornado = i;
		if (tornado < 0)
			return BossStepKind::Quick;
		for (int i = tornado + 1; i < step; ++i)
			if (combo[i] == s)
				return BossStepKind::Quick;
		return BossStepKind::Landing;
	}

	bool BossSession::QuickTiming(int step, float& left, float& total) const
	{
		if (m_state != BossState::Fighting || !m_running || step != m_castIndex || step >= ComboLength()
			|| StepKind(step) != BossStepKind::Quick)
			return false;
		total = BOSS_QUICK_LIMIT;
		left = BOSS_QUICK_LIMIT - (m_elapsed - m_castTime[step - 1]);
		return true;
	}

	void BossSession::ApplyHold(SkillId skill)
	{
		if (skill == SkillId::ColdSnap)
			m_freezeLeft = BOSS_FREEZE_TIME;
		else if (skill == SkillId::IceWall)
			m_slowLeft = BOSS_ICE_WALL_TIME;
		else if (skill == SkillId::GhostWalk)
			m_confuseLeft = BOSS_CONFUSE_TIME;
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
			float closing = TORNADO_SPEED * p.motion.dirX + (m_phase == BossPhase::Walking ? m_speed : 0.0f);
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
		float landing = 0.0f;
		if (m_state != BossState::Fighting || step < 1 || step >= ComboLength() || step < m_castIndex
			|| StepKind(step) != BossStepKind::Landing || !LandingTime(landing))
			return false;
		float delay = ImpactDelay(Combo()[step]);
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
			|| m_liftAttempt != m_attempt || m_castIndex >= ComboLength() || StepKind(m_castIndex) != BossStepKind::Landing)
			return false;
		float delay = ImpactDelay(Combo()[m_castIndex]);
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
		BossInputResult result = { false, { invoker::InvokerEvent::OrbAdded, SkillId::None }, false, ComboFail::None, HitGrade::None };
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
		const SkillId* combo = Combo();
		int length = ComboLength();
		int tag = 0;
		if (m_running)
		{
			if (m_castIndex < length && skill == combo[m_castIndex])
			{
				tag = m_attempt;
				int step = m_castIndex;
				m_castTime[step] = m_elapsed;
				if (StepKind(step) == BossStepKind::Quick)  // B-16: graded now, by how fast it followed
				{
					m_grades[step] = result.grade = GradeQuick(m_elapsed - m_castTime[step - 1]);
					if (m_grades[step] == HitGrade::Miss)
						m_missReasons[step] = ComboFail::TooLate;
				}
				++m_castIndex;
				ApplyHold(skill);
			}
			else if (m_castIndex >= length || skill == combo[m_castIndex - 1])
				tag = 0;  // the whole list is cast already, or a double tap of the last spell: ignored
			else
				Fail(ComboFail::WrongSpell, result.fail);
		}
		else if (skill == combo[0] && m_phase != BossPhase::Airborne)
		{
			++m_attempt;
			m_running = true;
			m_attemptStart = m_elapsed;
			m_castTime[0] = m_elapsed;
			ApplyHold(skill);
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
			int step = -1;
			for (int i = 1; i < ComboLength(); ++i)
			{
				if (Combo()[i] == skill && m_grades[i] == HitGrade::None && StepKind(i) == BossStepKind::Landing)
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
					impact.grade = GradeHit(m_elapsed - m_landTime, m_window);
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
		int followUps = ComboLength() - 1;
		int total = 0;
		ComboFail firstMiss = ComboFail::None;
		for (int i = 1; i < ComboLength(); ++i)
		{
			total += GradeScore(m_grades[i]);
			if (m_grades[i] == HitGrade::Miss && firstMiss == ComboFail::None)
				firstMiss = m_missReasons[i];
		}
		int damage = (total + followUps / 2) / followUps * m_damageMultiplier;  // x2 with an armed Refresher (§28 O-6)
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
		if (m_comboPhase == 0 && def.combo2Length > 0 && m_bossHp <= BOSS_PHASE2_HP)  // B-19
		{
			m_comboPhase = 1;
			result.phaseChanged = true;
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
		else if (m_freezeLeft <= 0.0f && m_confuseLeft <= 0.0f && !m_still)  // B-17 (and Eul's on an IMMORTAL): no move
		{
			if (m_walkBack)  // Blink Dagger on an IMMORTAL: it walks back, never past where it started
				m_x = m_x + m_speed * dt < BOSS_START_X ? m_x + m_speed * dt : BOSS_START_X;
			else
				m_x -= m_speed * m_speedFactor * (m_slowLeft > 0.0f ? BOSS_ICE_WALL_SPEED : 1.0f) * dt;
		}
		m_freezeLeft = m_freezeLeft > dt ? m_freezeLeft - dt : 0.0f;
		m_slowLeft = m_slowLeft > dt ? m_slowLeft - dt : 0.0f;
		m_confuseLeft = m_confuseLeft > dt ? m_confuseLeft - dt : 0.0f;

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
		// a quick step that was not cast in time is missed (B-16); a missed Tornado ends the attempt
		if (m_running && m_castIndex < ComboLength() && StepKind(m_castIndex) == BossStepKind::Quick
			&& m_elapsed - m_castTime[m_castIndex - 1] > BOSS_QUICK_LIMIT)
		{
			if (Combo()[m_castIndex] == SkillId::Tornado)
				Fail(ComboFail::TooLate, result.fail);
			else
			{
				m_grades[m_castIndex] = HitGrade::Miss;
				m_missReasons[m_castIndex] = ComboFail::TooLate;
				m_castTime[m_castIndex] = m_elapsed;  // the next quick step's time starts now
				++m_castIndex;
				result.quickMissed = true;
			}
		}
		if (m_running)
		{
			// the window after the landing has closed: the landing steps still open are missed (TOO LATE). The ones
			// never cast are passed over, so that the quick steps after them can still be cast (B-21).
			if (m_landed && m_elapsed > m_landTime + m_window)
			{
				for (int i = 1; i < ComboLength(); ++i)
				{
					if (m_grades[i] == HitGrade::None && StepKind(i) == BossStepKind::Landing)
					{
						m_grades[i] = HitGrade::Miss;
						m_missReasons[i] = ComboFail::TooLate;
					}
				}
				while (m_castIndex < ComboLength() && StepKind(m_castIndex) == BossStepKind::Landing)
				{
					m_castTime[m_castIndex] = m_elapsed;  // the next quick step's time starts now
					++m_castIndex;
				}
			}
			bool allGraded = true;
			for (int i = 1; i < ComboLength(); ++i)
				allGraded = allGraded && m_grades[i] != HitGrade::None;
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
			if (m_contactBlocked)
				result.contactBlocked = true;  // a Shield or a Black King Bar took it (IMMORTAL only)
			else
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
