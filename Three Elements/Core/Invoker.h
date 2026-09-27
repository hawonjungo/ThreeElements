#pragma once
#ifndef INVOKER_CORE_H_
#define INVOKER_CORE_H_

// Invoker core: the Q/W/E -> R -> D/F mechanic (see GAMEPLAY_SPEC.md sections 3-6, 20).
//
// Platform independent on purpose: no SDL, no rendering, no game loop, no clock, no globals.
// The presentation layer (MainPlayer / GameManager) turns keyboard events into InputAction
// values, feeds them to InvokerState and draws the result.

#include <string>
#include <vector>

namespace invoker
{
	enum class Orb { Quas, Wex, Exort };          // Q, W, E
	enum class InputAction { Q, W, E, R, D, F };  // logical keys (keyboard now, touch buttons later)
	enum class Slot { D, F };                     // D = newest invoked spell, F = the one before it

	// Values 0..SKILL_COUNT-1 double as indices into the skill catalog.
	enum class SkillId
	{
		None = -1,
		ColdSnap = 0,
		GhostWalk,
		IceWall,
		EMP,
		Tornado,
		Alacrity,
		SunStrike,
		ForgeSpirit,
		ChaosMeteor,
		DeafeningBlast
	};

	const int SKILL_COUNT = 10;
	const int MAX_ORBS = 3;

	// Normalised recipe: how many orbs of each kind. The order they were entered in is irrelevant.
	struct Recipe
	{
		int quas;
		int wex;
		int exort;
	};

	struct SkillDefinition
	{
		SkillId id;
		Recipe recipe;
		const char* name;  // display name
		const char* icon;  // asset path only; the texture belongs to the presentation layer
	};

	// ---- skill catalog (10 skills, immutable) ----
	const SkillDefinition& GetSkillDefinition(SkillId id);  // id must not be SkillId::None
	SkillId FindSkillByRecipe(const Recipe& recipe);        // SkillId::None if no skill matches

	// ---- helpers ----
	Recipe MakeRecipe(const std::vector<Orb>& orbs);  // normalise: count the orbs
	char OrbLetter(Orb orb);                          // 'Q', 'W' or 'E'
	std::string RecipeLetters(SkillId id);            // recipe letters in alphabetical order, e.g. "EQW"; "" for None

	// ---- invoker state ----
	enum class InvokerEvent
	{
		OrbAdded,       // Q/W/E
		Invoked,        // R with 3 orbs: `skill` is the spell that was invoked
		InvokeIgnored,  // R with fewer than 3 orbs: nothing happens, nothing is reset
		Cast,           // D/F on a filled slot: `skill` is the spell in that slot
		CastEmpty       // D/F on an empty slot
	};

	struct InvokerResult
	{
		InvokerEvent event;
		SkillId skill;  // SkillId::None unless event is Invoked or Cast
	};

	class InvokerState
	{
	public:
		InvokerState();

		// Single entry point for a logical key press.
		InvokerResult Apply(InputAction action);

		// Individual operations (Apply dispatches to these).
		void AddOrb(Orb orb);          // keeps the newest MAX_ORBS orbs
		InvokerResult Invoke();        // R
		InvokerResult Cast(Slot slot) const;  // D / F: reports the spell; slots are left untouched
		void Reset();                  // no orbs, both slots empty (new session)

		// Read access for the presentation layer.
		int OrbCount() const;
		Orb GetOrb(int index) const;   // 0 = oldest orb; the order is for display only
		SkillId GetSlot(Slot slot) const;

	private:
		std::vector<Orb> m_orbs;
		SkillId m_slotD;
		SkillId m_slotF;
	};
}

#endif // INVOKER_CORE_H_
