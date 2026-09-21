#include "SpellEffects.h"

namespace practice
{
	namespace
	{
		using invoker::SkillId;

		// One definition per skill, in SkillId order: GetSpellDefinition indexes this array with the id (the same
		// contract as the Core catalog). Tests/PracticeTests.cpp checks that every entry sits at its own id.
		// Only Tornado is a projectile; the other nine are judged at once when they are cast.
		// The Tornado numbers are its current tuning values, identical to the TORNADO_* constants in Practice.h.
		const SpellDefinition kSpells[invoker::SKILL_COUNT] =
		{
			// skill                   resolve                 projectile { speed, hitRadius, maxDistance }
			{ SkillId::ColdSnap,       ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::GhostWalk,      ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::IceWall,        ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::EMP,            ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::Tornado,        ResolveType::OnContact, { 700.0f, 20.0f, 1200.0f } },
			{ SkillId::Alacrity,       ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::SunStrike,      ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::ForgeSpirit,    ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::ChaosMeteor,    ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } },
			{ SkillId::DeafeningBlast, ResolveType::OnCast,    { 0.0f, 0.0f, 0.0f } }
		};

		// Returned for SkillId::None and for any value outside 0..SKILL_COUNT-1.
		const SpellDefinition kNoSpell = { SkillId::None, ResolveType::OnCast, { 0.0f, 0.0f, 0.0f } };
	}

	const SpellDefinition& GetSpellDefinition(SkillId skill)
	{
		int index = static_cast<int>(skill);
		if (index < 0 || index >= invoker::SKILL_COUNT)
			return kNoSpell;
		return kSpells[index];
	}
}
