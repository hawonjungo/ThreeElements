#pragma once
#ifndef SPELL_EFFECTS_H_
#define SPELL_EFFECTS_H_

// Spell definitions: what each skill does once it is cast, as plain data (M1 of the spell architecture).
//
// One table, one entry per skill, looked up by invoker::SkillId. No SDL, no rendering, no clock, no globals,
// and nothing here is known to the Invoker Core (Practice -> Core, never the other way round).
//
// NOTE: at this stage PracticeSession does not read this table. Tornado still runs on the TORNADO_* constants
// and functions in Practice.h; the values below only describe it. Nothing else is derived from them yet.

#include "../Core/Invoker.h"

namespace practice
{
	// When a cast is judged (PracticeSession::JudgeCast).
	enum class ResolveType
	{
		OnCast,      // right away, when the key is pressed
		OnContact    // only when the spell's projectile reaches its target
	};

	// The projectile of an OnContact spell. All zero for an OnCast spell.
	struct ProjectileParams
	{
		float speed;        // px/s
		float hitRadius;    // px, hit circle around the projectile centre
		float maxDistance;  // px it may fly before it is removed
	};

	struct SpellDefinition
	{
		invoker::SkillId skill;
		ResolveType resolve;
		ProjectileParams projectile;
	};

	// The definition of `skill`. For SkillId::None, or any value that is not a skill, it returns the same
	// "no spell" definition every time (skill None, OnCast, zero projectile): safe, but never a real spell.
	const SpellDefinition& GetSpellDefinition(invoker::SkillId skill);
}

#endif // SPELL_EFFECTS_H_
