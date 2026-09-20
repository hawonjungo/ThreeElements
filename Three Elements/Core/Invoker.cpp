#include "Invoker.h"

namespace invoker
{
	namespace
	{
		// Same 10 recipes, names and icons as the previous Skill::spellMap / GameManager tables.
		// Must stay in SkillId order: GetSkillDefinition indexes this array with the id.
		const SkillDefinition kSkills[SKILL_COUNT] =
		{
			{ SkillId::ColdSnap,       { 3, 0, 0 }, "Cold Snap",       "assets/skill/ColdSnap.png" },
			{ SkillId::GhostWalk,      { 2, 1, 0 }, "Ghost Walk",      "assets/skill/GhostWalk.png" },
			{ SkillId::IceWall,        { 2, 0, 1 }, "Ice Wall",        "assets/skill/IceWall.png" },
			{ SkillId::EMP,            { 0, 3, 0 }, "EMP",             "assets/skill/EMP.png" },
			{ SkillId::Tornado,        { 1, 2, 0 }, "Tornado",         "assets/skill/Tornado.png" },
			{ SkillId::Alacrity,       { 0, 2, 1 }, "Alacrity",        "assets/skill/Alacrity.png" },
			{ SkillId::SunStrike,      { 0, 0, 3 }, "Sun Strike",      "assets/skill/SunStrike.png" },
			{ SkillId::ForgeSpirit,    { 1, 0, 2 }, "Forge Spirit",    "assets/skill/ForgeSpirit.png" },
			{ SkillId::ChaosMeteor,    { 0, 1, 2 }, "Chaos Meteor",    "assets/skill/Meteor.png" },
			{ SkillId::DeafeningBlast, { 1, 1, 1 }, "Deafening Blast", "assets/skill/Blast.png" }
		};
	}

	const SkillDefinition& GetSkillDefinition(SkillId id)
	{
		return kSkills[static_cast<int>(id)];
	}

	SkillId FindSkillByRecipe(const Recipe& recipe)
	{
		for (int i = 0; i < SKILL_COUNT; ++i)
		{
			const Recipe& r = kSkills[i].recipe;
			if (r.quas == recipe.quas && r.wex == recipe.wex && r.exort == recipe.exort)
				return kSkills[i].id;
		}
		return SkillId::None;
	}

	Recipe MakeRecipe(const std::vector<Orb>& orbs)
	{
		Recipe recipe = { 0, 0, 0 };
		for (size_t i = 0; i < orbs.size(); ++i)
		{
			switch (orbs[i])
			{
			case Orb::Quas:   ++recipe.quas;   break;
			case Orb::Wex:    ++recipe.wex;    break;
			case Orb::Exort:  ++recipe.exort;  break;
			}
		}
		return recipe;
	}

	char OrbLetter(Orb orb)
	{
		switch (orb)
		{
		case Orb::Quas:   return 'Q';
		case Orb::Wex:    return 'W';
		case Orb::Exort:  return 'E';
		}
		return '?';
	}

	std::string RecipeLetters(SkillId id)
	{
		std::string letters;
		if (id == SkillId::None)
			return letters;

		const Recipe& r = GetSkillDefinition(id).recipe;
		letters.append(static_cast<size_t>(r.exort), 'E');  // alphabetical: E, Q, W
		letters.append(static_cast<size_t>(r.quas), 'Q');
		letters.append(static_cast<size_t>(r.wex), 'W');
		return letters;
	}

	// ------------------------------------------------------------------ InvokerState

	InvokerState::InvokerState() : m_slotD(SkillId::None), m_slotF(SkillId::None)
	{
	}

	InvokerResult InvokerState::Apply(InputAction action)
	{
		switch (action)
		{
		case InputAction::Q:  AddOrb(Orb::Quas);   return { InvokerEvent::OrbAdded, SkillId::None };
		case InputAction::W:  AddOrb(Orb::Wex);    return { InvokerEvent::OrbAdded, SkillId::None };
		case InputAction::E:  AddOrb(Orb::Exort);  return { InvokerEvent::OrbAdded, SkillId::None };
		case InputAction::R:  return Invoke();
		case InputAction::D:  return Cast(Slot::D);
		case InputAction::F:  return Cast(Slot::F);
		}
		return { InvokerEvent::InvokeIgnored, SkillId::None };
	}

	void InvokerState::AddOrb(Orb orb)
	{
		m_orbs.push_back(orb);
		if (static_cast<int>(m_orbs.size()) > MAX_ORBS)
			m_orbs.erase(m_orbs.begin());  // the oldest orb drops out
	}

	InvokerResult InvokerState::Invoke()
	{
		// Only a full set of orbs invokes; otherwise nothing happens (orbs and slots stay as they are).
		if (static_cast<int>(m_orbs.size()) != MAX_ORBS)
			return { InvokerEvent::InvokeIgnored, SkillId::None };

		SkillId id = FindSkillByRecipe(MakeRecipe(m_orbs));
		if (id == SkillId::None)  // cannot happen: the 10 recipes cover every 3-orb combination
			return { InvokerEvent::InvokeIgnored, SkillId::None };

		// Slot rule, moved unchanged from the previous MainPlayer::saveSpellToSlot:
		// the spell in D moves to F unless it is the one being invoked again, then D takes the new spell.
		// Invoking the spell already in D changes nothing; invoking the one in F swaps D and F.
		if (m_slotD != SkillId::None && m_slotD != m_slotF && m_slotD != id)
			m_slotF = m_slotD;
		m_slotD = id;

		return { InvokerEvent::Invoked, id };
	}

	InvokerResult InvokerState::Cast(Slot slot) const
	{
		SkillId id = GetSlot(slot);
		if (id == SkillId::None)
			return { InvokerEvent::CastEmpty, SkillId::None };
		return { InvokerEvent::Cast, id };
	}

	void InvokerState::Reset()
	{
		m_orbs.clear();
		m_slotD = SkillId::None;
		m_slotF = SkillId::None;
	}

	int InvokerState::OrbCount() const
	{
		return static_cast<int>(m_orbs.size());
	}

	Orb InvokerState::GetOrb(int index) const
	{
		return m_orbs[static_cast<size_t>(index)];
	}

	SkillId InvokerState::GetSlot(Slot slot) const
	{
		return slot == Slot::D ? m_slotD : m_slotF;
	}
}
