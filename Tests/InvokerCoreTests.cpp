// Tests for the Invoker Core (Three Elements/Core/Invoker.h/.cpp).
//
// Lightweight on purpose: no test framework, no SDL, no game code. This program compiles
// Core/Invoker.cpp directly, so it tests exactly the code the game uses.
// Exit code 0 = every check passed. How to run: see Tests/README.md.
//
// usage: InvokerCoreTests.exe [randomKeyCount [seed]]     (defaults: 300000, 12345)

#include "Core/Invoker.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace invoker;

// ---------------------------------------------------------------- tiny check helpers

static int g_checks = 0;
static int g_failed = 0;

static void Check(bool ok, const char* expr, int line)
{
	++g_checks;
	if (!ok)
	{
		++g_failed;
		printf("  FAIL line %d: %s\n", line, expr);
	}
}
#define CHECK(cond) Check((cond), #cond, __LINE__)

static void RunTest(const char* name, void (*test)())
{
	int checksBefore = g_checks;
	int failedBefore = g_failed;
	test();
	printf("[%-28s] %3d checks, %d failed\n", name, g_checks - checksBefore, g_failed - failedBefore);
}

static InputAction ActionOf(char c)
{
	switch (c)
	{
	case 'Q': return InputAction::Q;
	case 'W': return InputAction::W;
	case 'E': return InputAction::E;
	case 'R': return InputAction::R;
	case 'D': return InputAction::D;
	default:  return InputAction::F;
	}
}

static std::string OrbsOf(const InvokerState& s)  // orbs in entry order, e.g. "QWE"
{
	std::string orbs;
	for (int i = 0; i < s.OrbCount(); ++i)
		orbs += OrbLetter(s.GetOrb(i));
	return orbs;
}

static void Press(InvokerState& s, const char* keys)  // e.g. Press(s, "QQQR")
{
	for (; *keys; ++keys)
		s.Apply(ActionOf(*keys));
}

static SkillId D(const InvokerState& s) { return s.GetSlot(Slot::D); }
static SkillId F(const InvokerState& s) { return s.GetSlot(Slot::F); }

// ---------------------------------------------------------------- the ORIGINAL logic (regression oracle)
// Verbatim port of the code that used to live in MainPlayer (getElementComb / getCombineComb /
// saveSpellToSlot) and Skill::spellMap, before the Core was extracted. It works on plain strings and is
// only used to prove that the Core behaves exactly like the old implementation.

struct LegacyInvoker
{
	std::vector<char> elements;  // Q W E in press order
	std::string slotD;
	std::string slotF;
	std::map<std::string, std::string> spellMap;

	LegacyInvoker()
	{
		spellMap["QQQ"] = "COLD_SNAP";   spellMap["QQW"] = "GHOST_WALK";   spellMap["EQQ"] = "ICE_WALL";
		spellMap["WWW"] = "EMP";         spellMap["QWW"] = "TORNADO";      spellMap["EWW"] = "ALACRITY";
		spellMap["EEE"] = "SUN_STRIKE";  spellMap["EEQ"] = "FORGE_SPIRIT"; spellMap["EEW"] = "CHAOS_METEOR";
		spellMap["EQW"] = "DEAFENING_BLAST";
	}

	std::string getElementComb() { return std::string(elements.begin(), elements.end()); }

	void saveSpellToSlot(std::string combo)
	{
		if (!slotD.empty() && slotD != slotF && slotD != combo)
			slotF = slotD;
		slotD = combo;
	}

	void getCombineComb()
	{
		std::string combo = getElementComb();
		std::sort(combo.begin(), combo.end());
		if (spellMap.find(combo) != spellMap.end())
			saveSpellToSlot(combo);
	}

	void key(char c)  // the old handleKeyPress; D and F did nothing back then
	{
		if (c == 'Q' || c == 'W' || c == 'E')
			elements.push_back(c);
		else if (c == 'R')
		{
			getCombineComb();
			return;
		}
		if (elements.size() > 3)
			elements.erase(elements.begin());
	}
};

// ---------------------------------------------------------------- tests

// The catalog: 10 skills, unique recipes, every 3-orb combination covered.
static void TestCatalog()
{
	CHECK(SKILL_COUNT == 10);
	for (int i = 0; i < SKILL_COUNT; ++i)
	{
		const SkillDefinition& d = GetSkillDefinition(static_cast<SkillId>(i));
		CHECK(static_cast<int>(d.id) == i);
		CHECK(d.recipe.quas + d.recipe.wex + d.recipe.exort == MAX_ORBS);
		CHECK(d.name != nullptr && d.icon != nullptr);
		CHECK(FindSkillByRecipe(d.recipe) == d.id);
		for (int j = i + 1; j < SKILL_COUNT; ++j)
		{
			const Recipe& a = d.recipe;
			const Recipe& b = GetSkillDefinition(static_cast<SkillId>(j)).recipe;
			CHECK(!(a.quas == b.quas && a.wex == b.wex && a.exort == b.exort));  // unique
		}
	}

	int covered = 0;  // every multiset of size 3 over {Q,W,E} maps to a skill
	for (int q = 0; q <= 3; ++q)
		for (int w = 0; q + w <= 3; ++w)
		{
			Recipe r = { q, w, 3 - q - w };
			if (FindSkillByRecipe(r) != SkillId::None)
				++covered;
		}
	CHECK(covered == 10);

	Recipe twoOrbs = { 1, 1, 0 };
	CHECK(FindSkillByRecipe(twoOrbs) == SkillId::None);  // an incomplete combination is no spell
}

// The 10 recipes, names' letters and icon paths are identical to the tables the game used before.
static void TestLegacyCatalogParity()
{
	struct Old { const char* key; SkillId id; const char* icon; };
	const Old old[] = {
		{ "QQQ", SkillId::ColdSnap,       "assets/skill/ColdSnap.png" },
		{ "QQW", SkillId::GhostWalk,      "assets/skill/GhostWalk.png" },
		{ "EQQ", SkillId::IceWall,        "assets/skill/IceWall.png" },
		{ "WWW", SkillId::EMP,            "assets/skill/EMP.png" },
		{ "QWW", SkillId::Tornado,        "assets/skill/Tornado.png" },
		{ "EWW", SkillId::Alacrity,       "assets/skill/Alacrity.png" },
		{ "EEE", SkillId::SunStrike,      "assets/skill/SunStrike.png" },
		{ "EEQ", SkillId::ForgeSpirit,    "assets/skill/ForgeSpirit.png" },
		{ "EEW", SkillId::ChaosMeteor,    "assets/skill/Meteor.png" },
		{ "EQW", SkillId::DeafeningBlast, "assets/skill/Blast.png" } };
	for (const Old& o : old)
	{
		CHECK(RecipeLetters(o.id) == o.key);
		CHECK(std::string(GetSkillDefinition(o.id).icon) == o.icon);
	}
	CHECK(RecipeLetters(SkillId::None) == "");
}

// All 27 ordered triples resolve to the right skill regardless of order; the named cases of the spec.
static void TestAllRecipesAndPermutations()
{
	const char letters[3] = { 'Q', 'W', 'E' };
	LegacyInvoker legacyTable;
	int triples = 0;
	for (int a = 0; a < 3; ++a)
		for (int b = 0; b < 3; ++b)
			for (int c = 0; c < 3; ++c)
			{
				std::string keys;
				keys += letters[a];
				keys += letters[b];
				keys += letters[c];
				std::string sorted = keys;
				std::sort(sorted.begin(), sorted.end());

				InvokerState s;
				Press(s, keys.c_str());
				InvokerResult r = s.Invoke();
				CHECK(r.event == InvokerEvent::Invoked);
				CHECK(RecipeLetters(r.skill) == sorted);
				CHECK(legacyTable.spellMap.count(sorted) == 1);
				++triples;
			}
	CHECK(triples == 27);

	{ InvokerState s; Press(s, "QQQ"); CHECK(s.Invoke().skill == SkillId::ColdSnap); }
	{ InvokerState s; Press(s, "WWW"); CHECK(s.Invoke().skill == SkillId::EMP); }
	{ InvokerState s; Press(s, "EEE"); CHECK(s.Invoke().skill == SkillId::SunStrike); }

	const char* tornado[] = { "QWW", "WQW", "WWQ" };
	for (const char* k : tornado) { InvokerState s; Press(s, k); CHECK(s.Invoke().skill == SkillId::Tornado); }

	const char* blast[] = { "QWE", "QEW", "WQE", "WEQ", "EQW", "EWQ" };
	for (const char* k : blast) { InvokerState s; Press(s, k); CHECK(s.Invoke().skill == SkillId::DeafeningBlast); }

	const char* ghost[] = { "QQW", "QWQ", "WQQ" };  // the example of the spec
	for (const char* k : ghost) { InvokerState s; Press(s, k); CHECK(s.Invoke().skill == SkillId::GhostWalk); }

	const char* ice[] = { "QQE", "QEQ", "EQQ" };
	for (const char* k : ice) { InvokerState s; Press(s, k); CHECK(s.Invoke().skill == SkillId::IceWall); }
}

// Fewer than 3 orbs: R does nothing, resets nothing, never invokes an invalid spell.
static void TestIncompleteCombinations()
{
	InvokerState s;
	CHECK(s.Apply(InputAction::R).event == InvokerEvent::InvokeIgnored);  // 0 orbs
	CHECK(D(s) == SkillId::None && F(s) == SkillId::None && s.OrbCount() == 0);

	Press(s, "Q");
	CHECK(s.Apply(InputAction::R).event == InvokerEvent::InvokeIgnored);  // 1 orb
	Press(s, "W");
	CHECK(s.Apply(InputAction::R).event == InvokerEvent::InvokeIgnored);  // 2 orbs
	CHECK(OrbsOf(s) == "QW");                                             // orbs untouched
	CHECK(D(s) == SkillId::None && F(s) == SkillId::None);                // no invalid spell

	Press(s, "E");
	CHECK(s.Apply(InputAction::R).event == InvokerEvent::Invoked);
	CHECK(D(s) == SkillId::DeafeningBlast);
}

// The orbs are a rolling window of 3; the entry order is kept for display only.
static void TestRollingOrbs()
{
	InvokerState s;
	Press(s, "QQQW");
	CHECK(OrbsOf(s) == "QQW");
	CHECK(s.Invoke().skill == SkillId::GhostWalk);
	Press(s, "EEE");
	CHECK(OrbsOf(s) == "EEE");
}

// D holds the newest invoked spell, F the previous one; casting keeps the slots.
// Sequence: invoke A, invoke B, cast A, cast B, invoke C, cast B, cast C.
static void TestSlotOrder()
{
	InvokerState s;
	Press(s, "QQQ"); s.Apply(InputAction::R);                              // invoke A = Cold Snap
	CHECK(D(s) == SkillId::ColdSnap && F(s) == SkillId::None);
	Press(s, "WWW"); s.Apply(InputAction::R);                              // invoke B = EMP
	CHECK(D(s) == SkillId::EMP && F(s) == SkillId::ColdSnap);
	InvokerResult c = s.Apply(InputAction::F);                             // cast A
	CHECK(c.event == InvokerEvent::Cast && c.skill == SkillId::ColdSnap);
	c = s.Apply(InputAction::D);                                           // cast B
	CHECK(c.event == InvokerEvent::Cast && c.skill == SkillId::EMP);
	CHECK(D(s) == SkillId::EMP && F(s) == SkillId::ColdSnap);              // casting keeps the slots
	Press(s, "EEE"); s.Apply(InputAction::R);                              // invoke C = Sun Strike
	CHECK(D(s) == SkillId::SunStrike && F(s) == SkillId::EMP);             // A is gone
	CHECK(s.Apply(InputAction::F).skill == SkillId::EMP);                  // cast B
	CHECK(s.Apply(InputAction::D).skill == SkillId::SunStrike);            // cast C
}

// Invoking a spell that is already invoked.
static void TestDuplicateInvoke()
{
	InvokerState s;
	Press(s, "WWW"); s.Apply(InputAction::R);
	Press(s, "EEE"); s.Apply(InputAction::R);                              // D = Sun Strike, F = EMP

	Press(s, "EEE"); s.Apply(InputAction::R);                              // the spell already in D
	CHECK(D(s) == SkillId::SunStrike && F(s) == SkillId::EMP);             // both slots unchanged

	Press(s, "WWW"); s.Apply(InputAction::R);                              // the spell in F: D and F swap
	CHECK(D(s) == SkillId::EMP && F(s) == SkillId::SunStrike);

	Press(s, "QQQ"); s.Apply(InputAction::R);                              // a new spell drops the old F
	CHECK(D(s) == SkillId::ColdSnap && F(s) == SkillId::EMP);
}

// Casting an empty slot.
static void TestCastBehavior()
{
	InvokerState e;
	CHECK(e.Apply(InputAction::D).event == InvokerEvent::CastEmpty && e.Apply(InputAction::D).skill == SkillId::None);
	CHECK(e.Apply(InputAction::F).event == InvokerEvent::CastEmpty);
	Press(e, "QQQ"); e.Apply(InputAction::R);
	CHECK(e.Apply(InputAction::D).event == InvokerEvent::Cast);
	CHECK(e.Apply(InputAction::F).event == InvokerEvent::CastEmpty);       // only D is filled
}

static void TestReset()
{
	InvokerState s;
	Press(s, "QQQ"); s.Apply(InputAction::R);
	Press(s, "WWW"); s.Apply(InputAction::R);
	s.Reset();
	CHECK(s.OrbCount() == 0 && D(s) == SkillId::None && F(s) == SkillId::None);
	CHECK(s.Apply(InputAction::R).event == InvokerEvent::InvokeIgnored);
}

static void TestApplyMapping()
{
	InvokerState s;
	CHECK(s.Apply(InputAction::Q).event == InvokerEvent::OrbAdded);
	CHECK(s.Apply(InputAction::W).event == InvokerEvent::OrbAdded);
	CHECK(s.Apply(InputAction::E).event == InvokerEvent::OrbAdded);
	CHECK(OrbsOf(s) == "QWE");
}

// Random key presses through the ORIGINAL algorithm and through the Core must always agree:
// same orbs, same D, same F, after every single key.
static long g_randomKeys = 300000;
static unsigned g_seed = 12345;

static void TestRandomRegression()
{
	std::srand(g_seed);
	LegacyInvoker legacy;
	InvokerState core;
	const char keys[] = { 'Q', 'W', 'E', 'R', 'D', 'F' };
	long invokes = 0;
	long mismatches = 0;
	for (long n = 0; n < g_randomKeys; ++n)
	{
		char k = keys[std::rand() % 6];
		legacy.key(k);
		InvokerResult r = core.Apply(ActionOf(k));
		if (r.event == InvokerEvent::Invoked)
			++invokes;
		bool same = (OrbsOf(core) == legacy.getElementComb())
			&& (RecipeLetters(D(core)) == legacy.slotD)
			&& (RecipeLetters(F(core)) == legacy.slotF);
		if (!same)
		{
			++mismatches;
			if (mismatches < 5)
				printf("  mismatch at key #%ld ('%c')\n", n, k);
		}
	}
	CHECK(mismatches == 0);
	CHECK(invokes > g_randomKeys / 300);  // the random run really exercised invoking (>1000 at the default size)
	printf("  random regression: %ld keys (seed %u), %ld invokes, %ld mismatches vs the original logic\n",
		g_randomKeys, g_seed, invokes, mismatches);
}

int main(int argc, char** argv)
{
	if (argc > 1)
		g_randomKeys = std::atol(argv[1]);
	if (argc > 2)
		g_seed = static_cast<unsigned>(std::atol(argv[2]));

	RunTest("catalog", TestCatalog);
	RunTest("legacy catalog parity", TestLegacyCatalogParity);
	RunTest("all recipes + permutations", TestAllRecipesAndPermutations);
	RunTest("incomplete combinations", TestIncompleteCombinations);
	RunTest("rolling orbs", TestRollingOrbs);
	RunTest("slot order (D newest, F prev)", TestSlotOrder);
	RunTest("duplicate invoke", TestDuplicateInvoke);
	RunTest("cast behaviour", TestCastBehavior);
	RunTest("reset", TestReset);
	RunTest("apply() mapping", TestApplyMapping);
	RunTest("random regression vs original", TestRandomRegression);

	printf("\n%d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
