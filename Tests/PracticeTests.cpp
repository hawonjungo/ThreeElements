// Tests for the Practice layer (Three Elements/Practice/Practice.h/.cpp, Tutorial.*, Boss.*) on top of the Invoker Core.
//
// No test framework, no SDL, no window: PracticeSession takes time as `dt` and the seed as a parameter,
// so whole sessions run deterministically here. Exit code 0 = every check passed.
// How to run: see Tests/README.md.

#include "Practice/Practice.h"
#include "Practice/Tutorial.h"
#include "Practice/Boss.h"
#include "TouchLayout.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace practice;
using invoker::InputAction;
using invoker::Slot;
using invoker::SkillId;

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

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }
static bool NearF(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

static void RunTest(const char* name, void (*test)())
{
	int checksBefore = g_checks;
	int failedBefore = g_failed;
	test();
	printf("[%-34s] %3d checks, %d failed\n", name, g_checks - checksBefore, g_failed - failedBefore);
}

// ---------------------------------------------------------------- helpers that play the game

static void PressLetters(PracticeSession& s, const char* keys)  // e.g. "QQW"
{
	for (; *keys; ++keys)
	{
		InputAction a = InputAction::Q;
		if (*keys == 'W') a = InputAction::W;
		else if (*keys == 'E') a = InputAction::E;
		else if (*keys == 'R') a = InputAction::R;
		else if (*keys == 'D') a = InputAction::D;
		else if (*keys == 'F') a = InputAction::F;
		s.Input(a);
	}
}

// Q/W/E for the skill's recipe (in a deliberately shuffled order), then R.
static void InvokeSkill(PracticeSession& s, SkillId id)
{
	const invoker::Recipe& r = invoker::GetSkillDefinition(id).recipe;
	for (int i = 0; i < r.exort; ++i) s.Input(InputAction::E);
	for (int i = 0; i < r.wex; ++i) s.Input(InputAction::W);
	for (int i = 0; i < r.quas; ++i) s.Input(InputAction::Q);
	s.Input(InputAction::R);
}

static SkillId Different(SkillId id)  // any skill other than `id`, never Tornado (a Tornado answer is judged later, see ResolveTornado)
{
	int next = (static_cast<int>(id) + 1) % invoker::SKILL_COUNT;
	if (static_cast<SkillId>(next) == SkillId::Tornado)
		next = (next + 1) % invoker::SKILL_COUNT;
	if (static_cast<SkillId>(next) == id)
		next = (next + 1) % invoker::SKILL_COUNT;
	return static_cast<SkillId>(next);
}

static bool RunUntilEnemy(PracticeSession& s, float maxSeconds = 10.0f)
{
	for (float t = 0.0f; t < maxSeconds && !s.Enemy().active; t += 0.05f)
		s.Update(0.05f);
	return s.Enemy().active;
}

static UpdateResult RunUntilLeak(PracticeSession& s, float maxSeconds = 60.0f)
{
	UpdateResult r = { false, false, false };
	for (float t = 0.0f; t < maxSeconds; t += 0.05f)
	{
		r = s.Update(0.05f);
		if (r.leaked)
			break;
	}
	return r;
}

// Casting Tornado only launches a projectile; the answer is judged when it reaches the enemy. Runs the game
// until that has happened (or the projectile is gone). Does nothing for the other spells (already judged).
static void ResolveTornado(PracticeSession& s)
{
	for (float t = 0.0f; t < 5.0f && s.Enemy().active && s.ActiveTornadoCount() > 0; t += 0.02f)
		s.Update(0.02f);
}

// Answers the active enemy with its correct spell (cast from D) and waits until the answer is judged.
static void AnswerActiveEnemy(PracticeSession& s)
{
	InvokeSkill(s, s.Enemy().target);
	s.Input(InputAction::D);
	ResolveTornado(s);
}

// Waits for an enemy and kills it with the correct spell (cast from D).
static void Kill(PracticeSession& s)
{
	RunUntilEnemy(s);
	AnswerActiveEnemy(s);
}

// Lets enemies through until the session is over.
static void LoseAllHp(PracticeSession& s)
{
	for (int i = 0; i < START_HP + 1 && s.State() == GameState::Playing; ++i)
	{
		RunUntilEnemy(s);
		RunUntilLeak(s);
	}
}

static PracticeSession Started(unsigned seed = 1)
{
	PracticeSession s;
	s.Start(seed);
	return s;
}

// ---------------------------------------------------------------- tests

static void TestEnemyDefinitions()
{
	bool seen[invoker::SKILL_COUNT] = {};
	for (int i = 0; i < ENEMY_TYPE_COUNT; ++i)
	{
		const EnemyDefinition& d = GetEnemyDefinition(i);
		CHECK(d.id == i);
		CHECK(d.name != nullptr && d.sprite != nullptr);
		CHECK(d.frames > 0);
		CHECK(d.bodyLeft >= 0 && d.feetRow >= 0);
		CHECK(d.bodyWidth > 0 && d.bodyTop >= 0 && d.bodyTop <= d.bodyBottom && d.bodyBottom <= d.feetRow);  // hit box
		CHECK(d.speedMultiplier > 0.0f);
		CHECK(d.targetSkill != SkillId::None);
		int skill = static_cast<int>(d.targetSkill);
		CHECK(skill >= 0 && skill < invoker::SKILL_COUNT);
		if (skill >= 0 && skill < invoker::SKILL_COUNT)
		{
			CHECK(!seen[skill]);  // 10 enemy types <-> 10 skills, each skill exactly once
			seen[skill] = true;
		}
	}
	for (int i = 0; i < invoker::SKILL_COUNT; ++i)
		CHECK(seen[i]);           // all 10 skills are required by some enemy
}

static void TestInitialState()
{
	PracticeSession s;
	CHECK(s.State() == GameState::Ready);
	const Stats& st = s.GetStats();
	CHECK(st.hp == 3 && st.maxHp == 3 && START_HP == 3);
	CHECK(st.score == 0 && st.combo == 0 && st.bestCombo == 0);
	CHECK(st.correctCasts == 0 && st.incorrectCasts == 0 && st.TotalCasts() == 0);
	CHECK(Near(st.Accuracy(), 0.0));
	CHECK(st.survivalTime == 0.0f);
	CHECK(!s.Enemy().active && s.SpawnCount() == 0);

	// nothing happens until Start()
	InputResult in = s.Input(InputAction::Q);
	CHECK(!in.accepted && in.cast == CastOutcome::None);
	CHECK(s.Invoker().OrbCount() == 0);
	UpdateResult up = s.Update(1.0f);
	CHECK(!up.spawned && !up.leaked && !up.gameOver);
	CHECK(s.GetStats().survivalTime == 0.0f && !s.Enemy().active);
}

// 15. spawn flow
static void TestSpawnFlow()
{
	PracticeSession s = Started(7);
	CHECK(s.State() == GameState::Playing);
	CHECK(!s.Enemy().active && s.SpawnCount() == 0);

	for (int i = 0; i < 14; ++i)  // 1.4 s: still inside the 1.5 s initial delay
		s.Update(0.1f);
	CHECK(!s.Enemy().active);

	UpdateResult r = { false, false, false };
	for (int i = 0; i < 3 && !r.spawned; ++i)
		r = s.Update(0.1f);
	CHECK(r.spawned);
	CHECK(s.Enemy().active && s.SpawnCount() == 1);
	CHECK(s.Enemy().x == SPAWN_X);  // appears off-screen on the right
	CHECK(s.Enemy().definition >= 0 && s.Enemy().definition < ENEMY_TYPE_COUNT);
	CHECK(s.Enemy().target == GetEnemyDefinition(s.Enemy().definition).targetSkill);  // target comes from data
	CHECK(s.Enemy().speed >= START_ENEMY_SPEED && s.Enemy().speed <= MAX_ENEMY_SPEED);

	// it moves toward the player at speed * dt
	float x0 = s.Enemy().x;
	float speed = s.Enemy().speed;
	s.Update(0.05f);
	CHECK(NearF(s.Enemy().x, x0 - speed * 0.05f, 0.001f));

	// only one enemy at a time: no second spawn while this one is alive
	for (int i = 0; i < 30; ++i)  // 3 s, far less than the trip to the player
		s.Update(0.1f);
	CHECK(s.Enemy().active && s.SpawnCount() == 1);

	// after it is killed the next one comes after the delay, never with the same target twice in a row
	SkillId previous = s.Enemy().target;
	AnswerActiveEnemy(s);
	CHECK(!s.Enemy().active);
	s.Update(0.5f);
	CHECK(!s.Enemy().active);  // the delay has not passed yet
	CHECK(RunUntilEnemy(s));
	CHECK(s.SpawnCount() == 2);
	CHECK(s.Enemy().target != previous);

	// over many spawns: never the same target twice in a row, and every skill shows up
	bool shown[invoker::SKILL_COUNT] = {};
	SkillId last = SkillId::None;
	bool repeat = false;
	for (int i = 0; i < 400; ++i)
	{
		RunUntilEnemy(s);
		SkillId t = s.Enemy().target;
		if (t == last) repeat = true;
		last = t;
		shown[static_cast<int>(t)] = true;
		AnswerActiveEnemy(s);             // kill it and wait for the next one
	}
	CHECK(!repeat);
	bool all = true;
	for (int i = 0; i < invoker::SKILL_COUNT; ++i) all = all && shown[i];
	CHECK(all);

	// same seed -> same enemy order (deterministic and testable)
	PracticeSession a = Started(99), b = Started(99);
	bool same = true;
	for (int i = 0; i < 20; ++i)
	{
		Kill(a); Kill(b);
		RunUntilEnemy(a); RunUntilEnemy(b);
		if (a.Enemy().definition != b.Enemy().definition) same = false;
	}
	CHECK(same);
}

// 1, 4, 5. correct cast
static void TestCorrectCast()
{
	PracticeSession s = Started();
	CHECK(RunUntilEnemy(s));
	SkillId target = s.Enemy().target;

	InvokeSkill(s, target);
	InputResult r = s.Input(InputAction::D);
	if (target != SkillId::Tornado)                 // Tornado is judged when the projectile hits (Tornado tests)
		CHECK(r.accepted && r.cast == CastOutcome::Correct);
	ResolveTornado(s);
	CHECK(!s.Enemy().active);                       // 1. the challenge is cleared
	CHECK(s.GetStats().score == 1);                 // 4. score +1
	CHECK(s.GetStats().combo == 1);                 // 5. combo +1
	CHECK(s.GetStats().correctCasts == 1 && s.GetStats().incorrectCasts == 0);
	CHECK(s.GetStats().hp == 3);
	CHECK(s.State() == GameState::Playing);
	CHECK(s.Invoker().GetSlot(Slot::D) == target);  // the spell stays in its slot after casting

	// the next enemy comes after a delay
	CHECK(RunUntilEnemy(s));
	CHECK(s.SpawnCount() == 2);

	// casting from F works the same way
	target = s.Enemy().target;
	InvokeSkill(s, target);
	InvokeSkill(s, Different(target));              // target moves to F
	CHECK(s.Invoker().GetSlot(Slot::F) == target);
	r = s.Input(InputAction::F);
	if (target != SkillId::Tornado)
		CHECK(r.cast == CastOutcome::Correct);
	ResolveTornado(s);
	CHECK(!s.Enemy().active);
	CHECK(s.GetStats().score == 2 && s.GetStats().combo == 2);
}

// 2, 3. incorrect cast
static void TestIncorrectCast()
{
	PracticeSession s = Started();
	CHECK(RunUntilEnemy(s));
	SkillId target = s.Enemy().target;
	int definition = s.Enemy().definition;
	int hp = s.GetStats().hp;

	InvokeSkill(s, Different(target));
	InputResult r = s.Input(InputAction::D);
	CHECK(r.accepted && r.cast == CastOutcome::Incorrect);
	CHECK(s.Enemy().active);                        // 2. the enemy stays
	CHECK(s.Enemy().target == target && s.Enemy().definition == definition);
	CHECK(s.GetStats().hp == hp);                   // 3. no HP lost
	CHECK(s.GetStats().score == 0 && s.GetStats().combo == 0);
	CHECK(s.GetStats().incorrectCasts == 1 && s.GetStats().correctCasts == 0);

	// several wrong attempts are allowed and cost nothing but accuracy
	for (int i = 0; i < 5; ++i)
		s.Input(InputAction::D);
	CHECK(s.Enemy().active && s.GetStats().hp == hp && s.GetStats().incorrectCasts == 6);

	// the enemy keeps moving toward the player
	float x = s.Enemy().x;
	s.Update(0.1f);
	CHECK(s.Enemy().x < x);

	// and the player can still answer correctly
	InvokeSkill(s, target);
	r = s.Input(InputAction::D);
	if (target != SkillId::Tornado)
		CHECK(r.cast == CastOutcome::Correct);
	ResolveTornado(s);
	CHECK(!s.Enemy().active);
	CHECK(s.GetStats().score == 1 && s.GetStats().combo == 1);
}

// 5, 6. score, combo, best combo
static void TestScoreAndCombo()
{
	PracticeSession s = Started();
	for (int i = 1; i <= 3; ++i)
	{
		Kill(s);
		CHECK(s.GetStats().score == i && s.GetStats().combo == i && s.GetStats().bestCombo == i);
	}

	// a wrong cast does not reset the combo
	RunUntilEnemy(s);
	InvokeSkill(s, Different(s.Enemy().target));
	s.Input(InputAction::D);
	CHECK(s.GetStats().combo == 3 && s.GetStats().bestCombo == 3 && s.GetStats().score == 3);

	// a leak does: combo 0, best combo stays
	RunUntilLeak(s);
	CHECK(s.GetStats().combo == 0);
	CHECK(s.GetStats().bestCombo == 3);
	CHECK(s.GetStats().score == 3);

	// a lower streak does not lower or replace the best combo
	Kill(s); Kill(s);
	CHECK(s.GetStats().combo == 2 && s.GetStats().bestCombo == 3);

	// a longer streak raises it
	Kill(s); Kill(s);
	CHECK(s.GetStats().combo == 4 && s.GetStats().bestCombo == 4);
	CHECK(s.GetStats().score == 7);
}

// 7, 8. enemy reaches the player
static void TestLeak()
{
	PracticeSession s = Started();
	Kill(s);                                        // combo 1
	CHECK(s.GetStats().combo == 1);

	CHECK(RunUntilEnemy(s));
	float speed = s.Enemy().speed;
	float expected = (SPAWN_X - HIT_LINE_X) / speed;
	float elapsed = 0.0f;
	UpdateResult r = { false, false, false };
	while (!r.leaked && elapsed < 60.0f)
	{
		r = s.Update(0.02f);
		elapsed += 0.02f;
	}
	CHECK(r.leaked && !r.gameOver);
	CHECK(NearF(elapsed, expected, 0.05f));         // it takes about distance / speed
	CHECK(s.GetStats().hp == 2);                    // 7. HP -1
	CHECK(s.GetStats().combo == 0);                 // 8. combo resets
	CHECK(s.GetStats().score == 1);                 //    score is kept
	CHECK(!s.Enemy().active);                       //    the enemy is removed
	CHECK(s.State() == GameState::Playing);
	CHECK(s.GetStats().correctCasts == 1 && s.GetStats().incorrectCasts == 0);  // a leak is not a cast

	// the next challenge begins
	CHECK(RunUntilEnemy(s));
	CHECK(s.SpawnCount() == 3);

	// a large dt cannot jump the enemy past the player unnoticed: the line is tested with <=
	PracticeSession big = Started();
	RunUntilEnemy(big);
	UpdateResult ur = { false, false, false };
	for (int i = 0; i < 200 && !ur.leaked; ++i)
		ur = big.Update(1.0f);                      // clamped to MAX_FRAME_TIME
	CHECK(ur.leaked && big.GetStats().hp == 2);
}

// 9. Game Over
static void TestGameOver()
{
	PracticeSession s = Started();
	for (int leak = 1; leak <= 3; ++leak)
	{
		CHECK(RunUntilEnemy(s));
		UpdateResult r = RunUntilLeak(s);
		CHECK(r.leaked);
		CHECK(s.GetStats().hp == 3 - leak);
		CHECK(r.gameOver == (leak == 3));
	}
	CHECK(s.State() == GameState::GameOver);
	CHECK(s.GetStats().hp == 0);
	CHECK(!s.Enemy().active);                       // the last enemy was removed

	// frozen: no spawns, no time, no input
	float frozenTime = s.GetStats().survivalTime;
	int spawns = s.SpawnCount();
	for (int i = 0; i < 400; ++i)
		s.Update(0.05f);
	CHECK(!s.Enemy().active && s.SpawnCount() == spawns);
	CHECK(s.GetStats().survivalTime == frozenTime);
	int orbsBefore = s.Invoker().OrbCount();
	InputResult in = s.Input(InputAction::Q);
	CHECK(!in.accepted && s.Invoker().OrbCount() == orbsBefore);
	CHECK(s.GetStats().hp == 0 && s.State() == GameState::GameOver);
}

// 10, 11. casts that are not judged
static void TestUnjudgedCasts()
{
	PracticeSession s = Started();
	Kill(s);                                        // combo 1, score 1, one correct cast
	CHECK(RunUntilEnemy(s));                        // the spell is still in D; F is empty

	// empty F slot with an active enemy
	InputResult r = s.Input(InputAction::F);
	CHECK(r.accepted && r.invoker.event == invoker::InvokerEvent::CastEmpty);
	CHECK(r.cast == CastOutcome::None);
	CHECK(s.GetStats().incorrectCasts == 0 && s.GetStats().correctCasts == 1);
	CHECK(s.GetStats().hp == 3 && s.GetStats().combo == 1 && s.GetStats().score == 1);
	CHECK(s.Enemy().active);

	// a brand-new session: both slots empty
	PracticeSession fresh = Started();
	RunUntilEnemy(fresh);
	CHECK(fresh.Input(InputAction::D).cast == CastOutcome::None);
	CHECK(fresh.Input(InputAction::F).cast == CastOutcome::None);
	CHECK(fresh.GetStats().incorrectCasts == 0 && fresh.GetStats().TotalCasts() == 0);
	CHECK(fresh.GetStats().hp == 3 && fresh.GetStats().combo == 0);

	// a valid cast while no enemy is active
	PracticeSession idle = Started();
	CHECK(!idle.Enemy().active);
	InvokeSkill(idle, SkillId::Tornado);
	InputResult c = idle.Input(InputAction::D);
	CHECK(c.invoker.event == invoker::InvokerEvent::Cast && c.invoker.skill == SkillId::Tornado);  // the spell executes
	CHECK(c.cast == CastOutcome::None);
	CHECK(idle.GetStats().TotalCasts() == 0 && idle.GetStats().score == 0 && idle.GetStats().combo == 0);
	CHECK(idle.GetStats().hp == 3);
	CHECK(idle.Invoker().GetSlot(Slot::D) == SkillId::Tornado);  // and stays in its slot

	// also between two challenges, after a kill
	CHECK(RunUntilEnemy(idle));
	Kill(idle);
	CHECK(!idle.Enemy().active);
	int correct = idle.GetStats().correctCasts;
	InputResult between = idle.Input(InputAction::D);
	CHECK(between.cast == CastOutcome::None);
	CHECK(idle.GetStats().correctCasts == correct && idle.GetStats().incorrectCasts == 0);

	// Q/W/E and R without 3 orbs are not casts either
	PracticeSession orbs = Started();
	RunUntilEnemy(orbs);
	PressLetters(orbs, "QW");
	InputResult ig = orbs.Input(InputAction::R);
	CHECK(ig.invoker.event == invoker::InvokerEvent::InvokeIgnored && ig.cast == CastOutcome::None);
	CHECK(orbs.Invoker().OrbCount() == 2);          // orbs are not reset
	CHECK(orbs.GetStats().TotalCasts() == 0 && orbs.GetStats().hp == 3);
}

// 12. accuracy
static void TestAccuracy()
{
	PracticeSession s = Started();
	CHECK(Near(s.GetStats().Accuracy(), 0.0));      // no division by zero

	for (int i = 0; i < 10; ++i)
	{
		CHECK(RunUntilEnemy(s));
		if (i < 2)                                  // two wrong casts on the way
		{
			InvokeSkill(s, Different(s.Enemy().target));
			s.Input(InputAction::D);
		}
		AnswerActiveEnemy(s);
	}
	CHECK(s.GetStats().correctCasts == 10 && s.GetStats().incorrectCasts == 2);
	CHECK(s.GetStats().TotalCasts() == 12);
	CHECK(Near(s.GetStats().Accuracy(), 10.0 / 12.0));   // 83.3 %, not integer division
	CHECK(s.GetStats().Accuracy() > 0.83 && s.GetStats().Accuracy() < 0.84);

	// a leak is not a cast: accuracy does not change
	double before = s.GetStats().Accuracy();
	CHECK(RunUntilEnemy(s));
	RunUntilLeak(s);
	CHECK(Near(s.GetStats().Accuracy(), before));

	// direct arithmetic checks
	Stats st = { 3, 3, 0, 0, 0, 5, 2, 0.0f };
	CHECK(Near(st.Accuracy(), 5.0 / 7.0));
	st.correctCasts = 0; st.incorrectCasts = 4;
	CHECK(Near(st.Accuracy(), 0.0));
	st.correctCasts = 3; st.incorrectCasts = 0;
	CHECK(Near(st.Accuracy(), 1.0));
	st.correctCasts = 0; st.incorrectCasts = 0;
	CHECK(Near(st.Accuracy(), 0.0));
}

// 13. new session
static void TestNewSession()
{
	PracticeSession s = Started(5);
	Kill(s); Kill(s); Kill(s);
	RunUntilEnemy(s);
	InvokeSkill(s, Different(s.Enemy().target));
	s.Input(InputAction::D);                        // one wrong cast
	PressLetters(s, "QW");                          // two orbs left over
	RunUntilLeak(s);                                // one HP lost, combo reset
	RunUntilEnemy(s);                               // a live enemy
	CHECK(s.GetStats().score == 3 && s.GetStats().bestCombo == 3 && s.GetStats().hp == 2);
	CHECK(s.GetStats().survivalTime > 0.0f && s.Enemy().active);
	CHECK(s.Invoker().GetSlot(Slot::D) != SkillId::None && s.Invoker().OrbCount() > 0);

	// restart in the middle of a session
	s.Start(6);
	CHECK(s.State() == GameState::Playing);
	CHECK(s.GetStats().hp == 3 && s.GetStats().maxHp == 3);
	CHECK(s.GetStats().score == 0 && s.GetStats().combo == 0);
	CHECK(s.GetStats().bestCombo == 3);             // the record is kept across restarts (see TestBestComboAcrossRestarts)
	CHECK(s.GetStats().correctCasts == 0 && s.GetStats().incorrectCasts == 0);
	CHECK(Near(s.GetStats().Accuracy(), 0.0));
	CHECK(s.GetStats().survivalTime == 0.0f);
	CHECK(!s.Enemy().active && s.SpawnCount() == 0);
	CHECK(s.Invoker().OrbCount() == 0);
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::None && s.Invoker().GetSlot(Slot::F) == SkillId::None);

	// the spawn timer and the difficulty clock restarted too
	for (int i = 0; i < 10; ++i)
		s.Update(0.1f);
	CHECK(!s.Enemy().active);                       // 1.0 s: the initial delay is a fresh 1.5 s
	CHECK(RunUntilEnemy(s));
	CHECK(s.Enemy().speed < START_ENEMY_SPEED + 10.0f);  // early-game speed, not the speed of the old session

	// restart after Game Over
	LoseAllHp(s);
	CHECK(s.State() == GameState::GameOver);
	s.Start(8);
	CHECK(s.State() == GameState::Playing && s.GetStats().hp == 3);
	CHECK(s.GetStats().score == 0 && s.GetStats().survivalTime == 0.0f && !s.Enemy().active);
	CHECK(s.Invoker().OrbCount() == 0);
	CHECK(RunUntilEnemy(s));                        // and it can be played again
	Kill(s);
	CHECK(s.GetStats().score == 1);
}

// Best combo is a record for the whole application run: a new session resets the current combo, not the record.
static void TestBestComboAcrossRestarts()
{
	PracticeSession s;
	CHECK(s.GetStats().bestCombo == 0);             // no record yet
	s.Start(1);
	Kill(s); Kill(s); Kill(s);
	CHECK(s.GetStats().combo == 3 && s.GetStats().bestCombo == 3);

	s.Start(2);                                     // restart in the middle of a session
	CHECK(s.GetStats().combo == 0);                 // the current combo starts again
	CHECK(s.GetStats().bestCombo == 3);             // the record survives
	Kill(s); Kill(s);
	CHECK(s.GetStats().combo == 2 && s.GetStats().bestCombo == 3);  // below the record: unchanged
	Kill(s);
	CHECK(s.GetStats().combo == 3 && s.GetStats().bestCombo == 3);  // equal to the record: unchanged
	Kill(s);
	CHECK(s.GetStats().combo == 4 && s.GetStats().bestCombo == 4);  // beyond it: new record

	// a wrong cast changes neither combo nor record
	RunUntilEnemy(s);
	InvokeSkill(s, Different(s.Enemy().target));
	s.Input(InputAction::D);
	CHECK(s.GetStats().combo == 4 && s.GetStats().bestCombo == 4);

	LoseAllHp(s);                                   // Game Over keeps the record ...
	CHECK(s.State() == GameState::GameOver && s.GetStats().bestCombo == 4);
	s.Start(3);                                     // ... and so does the restart
	CHECK(s.GetStats().combo == 0 && s.GetStats().bestCombo == 4);

	Kill(s); Kill(s);                               // a leak resets the current combo, never the record
	RunUntilEnemy(s);
	RunUntilLeak(s);
	CHECK(s.GetStats().combo == 0 && s.GetStats().bestCombo == 4);

	s.ReturnToReady();                              // going back to Ready keeps it as well
	CHECK(s.GetStats().combo == 0 && s.GetStats().bestCombo == 4);
	s.Start(4);
	Kill(s);
	CHECK(s.GetStats().combo == 1 && s.GetStats().bestCombo == 4);

	// each PracticeSession object starts without a record (nothing is saved to disk)
	PracticeSession other;
	CHECK(other.GetStats().bestCombo == 0);
}

// Persistent records: MergeBests only ever raises a record (ties are not new records), and a saved best combo
// seeds the in-session record without ever lowering it (spec §13, P-7, EC-13).
static void TestPersistentBests()
{
	BestStats bests = { 0, 0, 0.0f };
	Stats st = { 0, 3, 5, 0, 4, 5, 1, 30.0f };      // score 5, best combo 4, survived 30 s
	BestUpdate u = MergeBests(bests, st);
	CHECK(u.score && u.combo && u.survivalTime && u.Any());
	CHECK(bests.score == 5 && bests.combo == 4 && NearF(bests.survivalTime, 30.0f, 1e-6f));

	u = MergeBests(bests, st);                      // the same session again: a tie is not a new record
	CHECK(!u.score && !u.combo && !u.survivalTime && !u.Any());

	Stats worse = { 0, 3, 2, 0, 1, 2, 0, 10.0f };
	u = MergeBests(bests, worse);                   // lower values never lower a record
	CHECK(!u.Any());
	CHECK(bests.score == 5 && bests.combo == 4 && NearF(bests.survivalTime, 30.0f, 1e-6f));

	Stats longer = { 0, 3, 3, 0, 2, 3, 0, 45.5f };  // only the survival time is better
	u = MergeBests(bests, longer);
	CHECK(!u.score && !u.combo && u.survivalTime);
	CHECK(bests.score == 5 && bests.combo == 4 && NearF(bests.survivalTime, 45.5f, 1e-6f));

	// RestoreBestCombo seeds the record, never lowers it, and a new session keeps it
	PracticeSession s;
	s.RestoreBestCombo(7);
	CHECK(s.GetStats().bestCombo == 7);
	s.RestoreBestCombo(3);
	CHECK(s.GetStats().bestCombo == 7);
	s.Start(1);
	CHECK(s.GetStats().combo == 0 && s.GetStats().bestCombo == 7);
	Kill(s);
	CHECK(s.GetStats().combo == 1 && s.GetStats().bestCombo == 7);
	s.ReturnToReady();
	CHECK(s.GetStats().bestCombo == 7);
}

// State transitions and the Enter / Esc rules.
static void TestStateTransitions()
{
	PracticeSession s;
	CHECK(s.State() == GameState::Ready);

	// Ready: Esc asks the application to quit, Enter starts a session
	CHECK(s.PressEscape());
	CHECK(s.State() == GameState::Ready);
	CHECK(s.PressEnter(1));
	CHECK(s.State() == GameState::Playing);

	// Playing: Enter is ignored, nothing of the running session is touched
	Kill(s);
	PressLetters(s, "QW");
	int spawns = s.SpawnCount();
	int orbs = s.Invoker().OrbCount();
	CHECK(!s.PressEnter(2));
	CHECK(s.State() == GameState::Playing && s.GetStats().score == 1);
	CHECK(s.SpawnCount() == spawns && s.Invoker().OrbCount() == orbs);

	// Playing: Esc goes back to Ready (no quit), the session is stopped and reset, the record is kept
	RunUntilEnemy(s);
	CHECK(s.Enemy().active);
	CHECK(!s.PressEscape());
	CHECK(s.State() == GameState::Ready);
	CHECK(!s.Enemy().active && s.SpawnCount() == 0);
	CHECK(s.GetStats().hp == 3 && s.GetStats().score == 0 && s.GetStats().combo == 0);
	CHECK(s.GetStats().correctCasts == 0 && s.GetStats().incorrectCasts == 0 && s.GetStats().survivalTime == 0.0f);
	CHECK(s.GetStats().bestCombo == 1);
	CHECK(s.Invoker().OrbCount() == 0 && s.Invoker().GetSlot(Slot::D) == SkillId::None);

	// Ready is inert: no input, no time, no enemy
	CHECK(!s.Input(InputAction::Q).accepted);
	for (int i = 0; i < 100; ++i)
		s.Update(0.1f);
	CHECK(!s.Enemy().active && s.SpawnCount() == 0 && s.GetStats().survivalTime == 0.0f);

	// Ready -> Playing again with a fresh session
	CHECK(s.PressEnter(3));
	CHECK(s.State() == GameState::Playing);
	CHECK(RunUntilEnemy(s));

	// Game Over: Esc goes back to Ready (no quit)
	LoseAllHp(s);
	CHECK(s.State() == GameState::GameOver);
	CHECK(!s.PressEscape());
	CHECK(s.State() == GameState::Ready && s.GetStats().hp == 3);

	// Game Over: Enter starts a new session
	CHECK(s.PressEnter(4));
	LoseAllHp(s);
	CHECK(s.State() == GameState::GameOver);
	CHECK(s.PressEnter(5));
	CHECK(s.State() == GameState::Playing && s.GetStats().hp == 3 && s.GetStats().score == 0);
	CHECK(!s.Enemy().active && s.SpawnCount() == 0);

	// Esc twice from Playing: first back to Ready, then a quit request
	CHECK(!s.PressEscape());
	CHECK(s.State() == GameState::Ready);
	CHECK(s.PressEscape());

	// ReturnToReady while already Ready is harmless
	s.ReturnToReady();
	CHECK(s.State() == GameState::Ready && s.GetStats().hp == 3);
}

// ======================================================================== Tornado

// A session whose current enemy asks for `wanted` (deterministic: tries seeds until it happens).
static PracticeSession StartedWithTarget(SkillId wanted)
{
	for (unsigned seed = 1; seed < 500; ++seed)
	{
		PracticeSession s = Started(seed);
		if (RunUntilEnemy(s) && s.Enemy().target == wanted)
			return s;
	}
	return Started();  // not reached: 10 enemies, 500 seeds
}

// A session whose current enemy does NOT ask for `unwanted`.
static PracticeSession StartedWithTargetNot(SkillId unwanted)
{
	PracticeSession s = Started(1);
	for (unsigned seed = 1; seed < 500; ++seed)
	{
		s = Started(seed);
		if (RunUntilEnemy(s) && s.Enemy().target != unwanted)
			break;
	}
	return s;
}

// Puts `spell` into D or F using the normal Invoke mechanics.
static void PutInSlot(PracticeSession& s, SkillId spell, Slot slot)
{
	InvokeSkill(s, spell);
	if (slot == Slot::F)
		InvokeSkill(s, Different(spell));  // `spell` moves from D to F
}

// The pure projectile functions: launch, direction, movement, collision, removal, animation frame.
static void TestTornadoFunctions()
{
	// launch: starts at the origin, direction is the normalised aim vector
	Tornado t = MakeTornado(100.0f, 200.0f, 3.0f, 4.0f, 7);
	CHECK(t.active && t.x == 100.0f && t.y == 200.0f && t.enemyId == 7);
	CHECK(NearF(t.dirX, 0.6f, 1e-6f) && NearF(t.dirY, 0.8f, 1e-6f));
	CHECK(NearF(t.dirX * t.dirX + t.dirY * t.dirY, 1.0f, 1e-6f));
	CHECK(t.travelled == 0.0f && t.animTime == 0.0f);

	// aim vector of any length gives the same direction; zero vector falls back to "right"
	Tornado longAim = MakeTornado(0.0f, 0.0f, 300.0f, 400.0f, 0);
	CHECK(NearF(longAim.dirX, 0.6f, 1e-6f) && NearF(longAim.dirY, 0.8f, 1e-6f));
	Tornado zeroAim = MakeTornado(5.0f, 5.0f, 0.0f, 0.0f, 0);
	CHECK(zeroAim.active && zeroAim.dirX == 1.0f && zeroAim.dirY == 0.0f);

	// movement: position += direction * speed * dt, independent of the animation
	AdvanceTornado(t, 0.05f);
	CHECK(NearF(t.x, 100.0f + 0.6f * TORNADO_SPEED * 0.05f, 1e-3f));
	CHECK(NearF(t.y, 200.0f + 0.8f * TORNADO_SPEED * 0.05f, 1e-3f));
	CHECK(NearF(t.travelled, TORNADO_SPEED * 0.05f, 1e-3f) && NearF(t.animTime, 0.05f, 1e-6f));
	CHECK(NearF(t.dirX, 0.6f, 1e-6f) && NearF(t.dirY, 0.8f, 1e-6f));  // never re-aimed
	float x = t.x;
	AdvanceTornado(t, -1.0f);                        // negative dt does nothing
	CHECK(t.x == x);

	// the same distance in different time slices ends in the same place
	Tornado a = MakeTornado(0.0f, 300.0f, 1.0f, 0.0f, 0), b = MakeTornado(0.0f, 300.0f, 1.0f, 0.0f, 0);
	for (int i = 0; i < 10; ++i) AdvanceTornado(a, 0.01f);
	for (int i = 0; i < 2; ++i) AdvanceTornado(b, 0.05f);
	CHECK(NearF(a.x, b.x, 1e-2f) && NearF(a.x, TORNADO_SPEED * 0.1f, 1e-2f));

	// removal: leaves the field, or flies too far; an inactive projectile neither moves nor hits
	Tornado out = MakeTornado(FIELD_WIDTH - 10.0f, 300.0f, 1.0f, 0.0f, 0);
	int steps = 0;
	while (out.active && steps < 1000) { AdvanceTornado(out, 0.01f); ++steps; }
	CHECK(!out.active && steps < 100);
	Tornado tired = MakeTornado(400.0f, 300.0f, 0.0f, 0.0f, 0);
	tired.travelled = TORNADO_MAX_DISTANCE - 1.0f;
	AdvanceTornado(tired, 0.01f);                    // 7 px: inside the field, but out of range
	CHECK(!tired.active);
	float xOut = out.x;
	AdvanceTornado(out, 1.0f);
	CHECK(out.x == xOut);
	Bounds anything = { out.x - 5.0f, out.y - 5.0f, 10.0f, 10.0f };
	CHECK(!TornadoHits(out, anything));

	// collision: circle against box
	Tornado c = MakeTornado(100.0f, 100.0f, 1.0f, 0.0f, 0);
	Bounds box = { 100.0f, 90.0f, 40.0f, 20.0f };
	CHECK(TornadoHits(c, box));                                   // centre inside the box
	c.x = box.x - TORNADO_HIT_RADIUS + 1.0f;
	CHECK(TornadoHits(c, box));                                   // just touching from the left
	c.x = box.x - TORNADO_HIT_RADIUS - 1.0f;
	CHECK(!TornadoHits(c, box));                                  // just too far
	c.x = 120.0f; c.y = box.y - TORNADO_HIT_RADIUS - 1.0f;
	CHECK(!TornadoHits(c, box));                                  // passes above
	c.y = box.y + box.h + TORNADO_HIT_RADIUS - 1.0f;
	CHECK(TornadoHits(c, box));                                   // grazes the bottom
	c.x = box.x - 15.0f; c.y = box.y - 15.0f;                     // diagonal: distance 21.2 to the corner > radius 20
	CHECK(!TornadoHits(c, box));
	c.x = box.x - 10.0f; c.y = box.y - 10.0f;                     // 14.1 to the corner
	CHECK(TornadoHits(c, box));

	// the enemy hit box comes from the definition
	ActiveEnemy e = { true, 0, GetEnemyDefinition(0).targetSkill, 500.0f, 100.0f };  // goblin
	Bounds gb = EnemyBounds(e);
	CHECK(gb.x == 500.0f && gb.w == 38.0f && gb.h == 38.0f && gb.y == GROUND_LINE_Y - 100.0f + 63.0f);
	CHECK(TornadoHits(MakeTornado(gb.x + 1.0f, gb.y + 1.0f, 1.0f, 0.0f, 0), gb));

	// animation: 16 frames at 10 fps, looping, every frame shown, none skipped
	CHECK(TORNADO_FRAME_COUNT == 16 && NearF(TORNADO_ANIM_FPS, 10.0f, 1e-6f));
	CHECK(TornadoFrame(0.0f) == 0 && TornadoFrame(0.05f) == 0 && TornadoFrame(0.11f) == 1);
	CHECK(TornadoFrame(1.55f) == 15 && TornadoFrame(1.61f) == 0);  // wraps after 1.6 s
	CHECK(TornadoFrame(-3.0f) == 0);
	for (int k = 0; k < 3 * TORNADO_FRAME_COUNT; ++k)   // 3 cycles, sampled in the middle of every frame: 0..15, 0..15, 0..15
		CHECK(TornadoFrame((static_cast<float>(k) + 0.5f) / TORNADO_ANIM_FPS) == k % TORNADO_FRAME_COUNT);
	bool seen[TORNADO_FRAME_COUNT] = {};
	int previousFrame = 0;
	bool orderly = true;
	for (float time = 0.0f; time < 3.3f; time += 0.01f)
	{
		int f = TornadoFrame(time);
		if (f < 0 || f >= TORNADO_FRAME_COUNT) { orderly = false; break; }
		seen[f] = true;
		if (f != previousFrame && f != (previousFrame + 1) % TORNADO_FRAME_COUNT) orderly = false;  // no jumps
		previousFrame = f;
	}
	bool allFrames = true;
	for (int i = 0; i < TORNADO_FRAME_COUNT; ++i) allFrames = allFrames && seen[i];
	CHECK(orderly && allFrames);
}

// Casting: what creates a projectile and what does not.
static void TestTornadoLaunch()
{
	// D slot
	PracticeSession s = StartedWithTargetNot(SkillId::Tornado);
	int hp = s.GetStats().hp;
	PutInSlot(s, SkillId::Tornado, Slot::D);
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::Tornado);
	CHECK(s.ActiveTornadoCount() == 0);                 // invoking alone launches nothing
	InputResult r = s.Input(InputAction::D);
	CHECK(r.accepted && r.invoker.event == invoker::InvokerEvent::Cast && r.invoker.skill == SkillId::Tornado);
	CHECK(r.tornadoLaunched);
	CHECK(r.cast == CastOutcome::None);                 // not judged at cast time
	CHECK(s.ActiveTornadoCount() == 1);                 // exactly one projectile
	CHECK(s.Enemy().active && s.GetStats().score == 0 && s.GetStats().combo == 0);  // the enemy does not die at once
	CHECK(s.GetStats().TotalCasts() == 0 && s.GetStats().hp == hp);
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::Tornado);  // the spell stays in its slot

	// it starts at the cast origin, aimed at the enemy's body centre, tagged with that enemy
	const Tornado* t = nullptr;
	if (s.ActiveTornadoCount() == 1) t = &s.GetTornado(0);
	CHECK(t != nullptr);
	if (t != nullptr)
	{
		CHECK(t->x == PLAYER_CAST_X && t->y == PLAYER_CAST_Y);
		Bounds body = EnemyBounds(s.Enemy());
		float dx = body.x + body.w * 0.5f - PLAYER_CAST_X, dy = body.y + body.h * 0.5f - PLAYER_CAST_Y;
		float len = std::sqrt(dx * dx + dy * dy);
		CHECK(NearF(t->dirX, dx / len, 1e-5f) && NearF(t->dirY, dy / len, 1e-5f));
		CHECK(NearF(t->dirX * t->dirX + t->dirY * t->dirY, 1.0f, 1e-5f));
		CHECK(t->dirX > 0.99f);                           // the enemy is far to the right: almost horizontal
		CHECK(t->enemyId == s.SpawnCount());
	}

	// F slot: same behaviour
	PracticeSession f = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(f, SkillId::Tornado, Slot::F);
	CHECK(f.Invoker().GetSlot(Slot::F) == SkillId::Tornado && f.Invoker().GetSlot(Slot::D) != SkillId::Tornado);
	InputResult rf = f.Input(InputAction::D);           // D holds another spell: no Tornado
	CHECK(!rf.tornadoLaunched && f.ActiveTornadoCount() == 0);
	InputResult rf2 = f.Input(InputAction::F);
	CHECK(rf2.tornadoLaunched && rf2.cast == CastOutcome::None);
	CHECK(f.ActiveTornadoCount() == 1);
	CHECK(f.Invoker().GetSlot(Slot::F) == SkillId::Tornado);

	// Tornado is recognised by the SkillId in the slot, not by the slot: swap D and F and it still works
	PracticeSession g = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(g, SkillId::Tornado, Slot::F);
	InvokeSkill(g, SkillId::Tornado);                   // invoking the spell in F swaps the slots
	CHECK(g.Invoker().GetSlot(Slot::D) == SkillId::Tornado);
	CHECK(g.Input(InputAction::D).tornadoLaunched && g.ActiveTornadoCount() == 1);

	// no other spell creates a projectile, whatever the enemy needs
	for (int id = 0; id < invoker::SKILL_COUNT; ++id)
	{
		SkillId spell = static_cast<SkillId>(id);
		if (spell == SkillId::Tornado)
			continue;
		PracticeSession o = Started(3);
		RunUntilEnemy(o);
		InvokeSkill(o, spell);
		InputResult ro = o.Input(InputAction::D);
		CHECK(!ro.tornadoLaunched && o.ActiveTornadoCount() == 0);
	}

	// nothing to aim at: no enemy, no projectile (and no penalty); an empty slot launches nothing either
	PracticeSession idle = Started();
	PutInSlot(idle, SkillId::Tornado, Slot::D);
	CHECK(!idle.Enemy().active);
	InputResult ri = idle.Input(InputAction::D);
	CHECK(ri.invoker.event == invoker::InvokerEvent::Cast && !ri.tornadoLaunched && idle.ActiveTornadoCount() == 0);
	CHECK(idle.GetStats().TotalCasts() == 0);
	PracticeSession empty = Started();
	RunUntilEnemy(empty);
	CHECK(!empty.Input(InputAction::D).tornadoLaunched && !empty.Input(InputAction::F).tornadoLaunched);

	// there is no gameplay cap on the number of projectiles: see TestTornadoManyProjectiles
}

// In flight: the direction is fixed at launch, the movement follows dt.
static void TestTornadoFlight()
{
	PracticeSession s = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(s, SkillId::Tornado, Slot::D);
	s.Input(InputAction::D);
	Tornado launched = s.GetTornado(0);
	CHECK(launched.active);

	float enemyX = s.Enemy().x;
	float elapsed = 0.0f;
	bool direction = true;
	for (int i = 0; i < 6; ++i)
	{
		s.Update(0.05f);
		elapsed += 0.05f;
		const Tornado& t = s.GetTornado(0);
		if (t.dirX != launched.dirX || t.dirY != launched.dirY)  // exactly the launch direction, every time
			direction = false;
	}
	CHECK(direction);
	CHECK(s.Enemy().x < enemyX - 10.0f);                 // the enemy moved meanwhile: the projectile did not follow it
	const Tornado& now = s.GetTornado(0);
	CHECK(now.active);
	CHECK(NearF(now.x, launched.x + launched.dirX * TORNADO_SPEED * elapsed, 0.05f));
	CHECK(NearF(now.y, launched.y + launched.dirY * TORNADO_SPEED * elapsed, 0.05f));
	CHECK(NearF(now.animTime, elapsed, 1e-4f));
	CHECK(TornadoFrame(now.animTime) == static_cast<int>(elapsed * TORNADO_ANIM_FPS));

	// its path is a straight line: the position always satisfies p = origin + dir * travelled
	CHECK(NearF(now.x, PLAYER_CAST_X + now.dirX * now.travelled, 0.05f));
	CHECK(NearF(now.y, PLAYER_CAST_Y + now.dirY * now.travelled, 0.05f));

	// the enemy is not stopped or damaged by the flight itself
	CHECK(s.Enemy().active && s.GetStats().hp == 3 && s.GetStats().TotalCasts() == 0);

	// time slicing does not matter: 0.4 s as 8 x 0.05 or as 4 x 0.1
	PracticeSession a = StartedWithTargetNot(SkillId::Tornado), b = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(a, SkillId::Tornado, Slot::D); PutInSlot(b, SkillId::Tornado, Slot::D);
	a.Input(InputAction::D); b.Input(InputAction::D);
	for (int i = 0; i < 8; ++i) a.Update(0.05f);
	for (int i = 0; i < 4; ++i) b.Update(0.1f);
	CHECK(NearF(a.GetTornado(0).x, b.GetTornado(0).x, 0.05f) && NearF(a.GetTornado(0).y, b.GetTornado(0).y, 0.05f));

	// a huge dt is clamped like everything else: no teleport through the enemy
	PracticeSession big = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(big, SkillId::Tornado, Slot::D);
	big.Input(InputAction::D);
	float bx = big.GetTornado(0).x;
	big.Update(10.0f);
	CHECK(big.GetTornado(0).x - bx <= TORNADO_SPEED * MAX_FRAME_TIME + 0.05f);
}

// Hit: the enemy is judged exactly once, through the normal scoring path.
static void TestTornadoHit()
{
	PracticeSession s = StartedWithTarget(SkillId::Tornado);
	PutInSlot(s, SkillId::Tornado, Slot::D);
	InputResult r = s.Input(InputAction::D);
	CHECK(r.tornadoLaunched && r.cast == CastOutcome::None);
	CHECK(s.Enemy().active && s.GetStats().score == 0);  // launching Tornado alone does not kill the enemy

	int correctEvents = 0, incorrectEvents = 0, updates = 0;
	while (s.Enemy().active && updates < 500)
	{
		UpdateResult u = s.Update(0.02f);
		if (u.cast == CastOutcome::Correct) ++correctEvents;
		if (u.cast == CastOutcome::Incorrect) ++incorrectEvents;
		++updates;
	}
	CHECK(updates > 5);                                  // it had to fly first: not instant
	CHECK(!s.Enemy().active);                            // the enemy was removed by the hit
	CHECK(correctEvents == 1 && incorrectEvents == 0);   // judged exactly once
	CHECK(s.GetStats().score == 1);                      // score once
	CHECK(s.GetStats().combo == 1 && s.GetStats().bestCombo == 1);  // combo once
	CHECK(s.GetStats().correctCasts == 1 && s.GetStats().incorrectCasts == 0);  // accuracy: one correct cast
	CHECK(Near(s.GetStats().Accuracy(), 1.0));
	CHECK(s.GetStats().hp == 3 && s.State() == GameState::Playing);
	CHECK(s.ActiveTornadoCount() == 0);                  // the projectile is gone
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::Tornado);  // the spell stays invoked

	// nothing is counted a second time afterwards
	for (int i = 0; i < 100; ++i)
		s.Update(0.02f);
	CHECK(s.GetStats().score == 1 && s.GetStats().combo == 1 && s.GetStats().correctCasts == 1);

	// from the F slot it is the same
	PracticeSession f = StartedWithTarget(SkillId::Tornado);
	PutInSlot(f, SkillId::Tornado, Slot::F);
	CHECK(f.Input(InputAction::F).tornadoLaunched);
	int events = 0;
	for (int i = 0; i < 500 && f.Enemy().active; ++i)
		if (f.Update(0.02f).cast == CastOutcome::Correct) ++events;
	CHECK(events == 1 && !f.Enemy().active && f.GetStats().score == 1 && f.GetStats().combo == 1);

	// two projectiles at the same enemy: still one result
	PracticeSession two = StartedWithTarget(SkillId::Tornado);
	PutInSlot(two, SkillId::Tornado, Slot::D);
	two.Input(InputAction::D);
	two.Update(0.02f);
	two.Input(InputAction::D);
	CHECK(two.ActiveTornadoCount() == 2);
	for (int i = 0; i < 200; ++i)
		two.Update(0.02f);
	CHECK(two.GetStats().score == 1 && two.GetStats().combo == 1 && two.GetStats().correctCasts == 1);
	CHECK(two.GetStats().incorrectCasts == 0 && two.ActiveTornadoCount() == 0);

	// the hit follows the existing rules: an enemy that needs another spell is a wrong answer, not a kill
	PracticeSession w = StartedWithTargetNot(SkillId::Tornado);
	int hp = w.GetStats().hp;
	PutInSlot(w, SkillId::Tornado, Slot::D);
	w.Input(InputAction::D);
	int wrong = 0, right = 0;
	for (int i = 0; i < 500 && w.ActiveTornadoCount() > 0; ++i)
	{
		UpdateResult u = w.Update(0.02f);
		if (u.cast == CastOutcome::Incorrect) ++wrong;
		if (u.cast == CastOutcome::Correct) ++right;
	}
	CHECK(wrong == 1 && right == 0);
	CHECK(w.Enemy().active);                             // still alive
	CHECK(w.GetStats().incorrectCasts == 1 && w.GetStats().correctCasts == 0);
	CHECK(w.GetStats().score == 0 && w.GetStats().combo == 0 && w.GetStats().hp == hp);
}

// Miss: nothing at all changes.
static void TestTornadoMiss()
{
	PracticeSession s = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(s, SkillId::Tornado, Slot::D);
	Kill(s);                                             // one earlier answer, so combo and score are non-zero
	CHECK(RunUntilEnemy(s));
	if (s.Enemy().target == SkillId::Tornado)            // want an enemy that would not need Tornado either way
		Kill(s), RunUntilEnemy(s);
	Stats before = s.GetStats();
	int spawns = s.SpawnCount();
	ActiveEnemy enemy = s.Enemy();
	int enemyId = s.SpawnCount();

	// aim straight up: the enemy is nowhere near the path
	CHECK(s.LaunchTornado(0.0f, -1.0f));
	CHECK(s.ActiveTornadoCount() == 1);
	int events = 0;
	float time = 0.0f;
	while (s.ActiveTornadoCount() > 0 && time < 5.0f)
	{
		if (s.Update(0.02f).cast != CastOutcome::None) ++events;
		time += 0.02f;
	}
	CHECK(s.ActiveTornadoCount() == 0);                  // removed after leaving the play area
	CHECK(events == 0);                                  // never judged
	CHECK(s.Enemy().active && s.SpawnCount() == spawns && enemyId == spawns);  // enemy untouched, no replacement
	CHECK(s.Enemy().target == enemy.target && s.Enemy().definition == enemy.definition);
	CHECK(s.Enemy().x < enemy.x);                        // it kept walking like any enemy
	CHECK(s.GetStats().hp == before.hp);
	CHECK(s.GetStats().combo == before.combo && s.GetStats().bestCombo == before.bestCombo);
	CHECK(s.GetStats().score == before.score);
	CHECK(s.GetStats().correctCasts == before.correctCasts && s.GetStats().incorrectCasts == before.incorrectCasts);
	CHECK(Near(s.GetStats().Accuracy(), before.Accuracy()));

	// aimed downwards, and to the left, out of the field: also just removed
	CHECK(s.LaunchTornado(0.0f, 1.0f));
	CHECK(s.LaunchTornado(-1.0f, 0.0f));
	for (float t2 = 0.0f; t2 < 3.0f && s.ActiveTornadoCount() > 0; t2 += 0.02f)
		s.Update(0.02f);
	CHECK(s.ActiveTornadoCount() == 0);
	CHECK(s.GetStats().hp == before.hp && s.GetStats().score == before.score && s.GetStats().combo == before.combo);
	CHECK(s.GetStats().TotalCasts() == before.TotalCasts());

	// the enemy is answered by another spell while the Tornado is on its way: the Tornado just disappears later
	PracticeSession k = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(k, SkillId::Tornado, Slot::D);
	CHECK(k.Input(InputAction::D).tornadoLaunched);
	InvokeSkill(k, k.Enemy().target);
	CHECK(k.Input(InputAction::D).cast == CastOutcome::Correct);  // this answer counts, once
	CHECK(!k.Enemy().active && k.GetStats().score == 1 && k.GetStats().combo == 1);
	Stats afterKill = k.GetStats();
	for (int i = 0; i < 200; ++i)
		k.Update(0.02f);
	CHECK(k.ActiveTornadoCount() == 0);
	CHECK(k.GetStats().score == afterKill.score && k.GetStats().combo == afterKill.combo);
	CHECK(k.GetStats().correctCasts == afterKill.correctCasts && k.GetStats().incorrectCasts == afterKill.incorrectCasts);
	CHECK(k.GetStats().hp == 3);
}

// A projectile only ever hits the enemy it was launched at, never a later one that happens to cross its path.
static void TestTornadoBoundToEnemy()
{
	PracticeSession s = Started(11);
	for (int i = 0; i < 300 && s.GetStats().survivalTime < 110.0f; ++i)
		Kill(s);                                          // late game: the pause between two enemies is at its minimum
	CHECK(s.GetStats().survivalTime >= 110.0f);
	CHECK(DifficultyAt(s.GetStats().survivalTime).challengeDelay == MIN_CHALLENGE_DELAY);

	RunUntilEnemy(s);
	for (int guard = 0; guard < 20 && s.Enemy().target == SkillId::Tornado; ++guard)  // bounded: never spin forever
	{
		Kill(s);
		RunUntilEnemy(s);
	}
	CHECK(s.Enemy().active && s.Enemy().target != SkillId::Tornado);
	InvokeSkill(s, SkillId::Tornado);
	InvokeSkill(s, s.Enemy().target);                     // D = its own spell, F = Tornado
	CHECK(s.Input(InputAction::F).tornadoLaunched);       // launched at the first enemy ...
	CHECK(s.Input(InputAction::D).cast == CastOutcome::Correct);  // ... which is answered at once (this counts, once)
	Stats after = s.GetStats();
	int spawnsBefore = s.SpawnCount();

	// the next enemy appears half a second later and walks straight through the Tornado's path
	bool crossed = false;
	for (float t = 0.0f; t < 1.6f; t += 0.02f)
	{
		s.Update(0.02f);
		if (s.SpawnCount() > spawnsBefore && s.ActiveTornadoCount() > 0)
			crossed = true;                               // the projectile was still flying when the new enemy was there
	}
	CHECK(crossed);
	CHECK(s.Enemy().active && s.SpawnCount() == spawnsBefore + 1);
	CHECK(s.GetStats().score == after.score && s.GetStats().combo == after.combo);
	CHECK(s.GetStats().correctCasts == after.correctCasts && s.GetStats().incorrectCasts == after.incorrectCasts);
	CHECK(s.GetStats().hp == after.hp);
}

// Projectiles disappear with the session.
// CONFIRMED rule B: a Tornado that hits an enemy needing another spell is one wrong cast, nothing more.
static void TestTornadoWrongTarget()
{
	// a session with a combo already running, facing an enemy that needs something else
	PracticeSession s = Started(7);
	Kill(s);
	Kill(s);
	for (int guard = 0; guard < 20; ++guard)
	{
		RunUntilEnemy(s);
		if (s.Enemy().target != SkillId::Tornado)
			break;
		AnswerActiveEnemy(s);
	}
	CHECK(s.Enemy().active && s.Enemy().target != SkillId::Tornado);
	CHECK(s.GetStats().combo >= 2 && s.GetStats().correctCasts >= 2);

	const Stats before = s.GetStats();
	const int spawn = s.SpawnCount();
	const int definition = s.Enemy().definition;
	const SkillId target = s.Enemy().target;
	PutInSlot(s, SkillId::Tornado, Slot::D);
	CHECK(s.Input(InputAction::D).tornadoLaunched);

	int wrong = 0, right = 0;
	for (int i = 0; i < 500 && s.ActiveTornadoCount() > 0; ++i)
	{
		UpdateResult u = s.Update(0.02f);
		CHECK(!u.leaked);
		if (u.cast == CastOutcome::Incorrect) ++wrong;
		if (u.cast == CastOutcome::Correct) ++right;
	}
	CHECK(wrong == 1 && right == 0);                                          // one wrong cast, judged exactly once
	CHECK(s.ActiveTornadoCount() == 0);                                       // the projectile is removed
	CHECK(s.Enemy().active && s.SpawnCount() == spawn);                       // the enemy is NOT removed, no new one
	CHECK(s.Enemy().definition == definition && s.Enemy().target == target); // still the same enemy
	CHECK(s.GetStats().hp == before.hp && s.State() == GameState::Playing);   // no HP loss
	CHECK(s.GetStats().score == before.score);                                // no score
	CHECK(s.GetStats().combo == before.combo && s.GetStats().bestCombo == before.bestCombo);  // a wrong cast does not break the combo
	CHECK(s.GetStats().correctCasts == before.correctCasts && s.GetStats().incorrectCasts == before.incorrectCasts + 1);
	CHECK(Near(s.GetStats().Accuracy(), static_cast<double>(before.correctCasts) / (before.TotalCasts() + 1)));  // accuracy drops

	// it keeps coming and can be answered afterwards (retry); that answer is judged normally
	float x = s.Enemy().x;
	s.Update(0.1f);
	CHECK(s.Enemy().x < x);
	AnswerActiveEnemy(s);
	CHECK(!s.Enemy().active && s.GetStats().score == before.score + 1 && s.GetStats().combo == before.combo + 1);
	CHECK(s.GetStats().incorrectCasts == before.incorrectCasts + 1);          // still only the one wrong cast

	// exactly what any other wrong spell does: same seed, same enemy, same numbers
	int compared = 0;
	for (unsigned seed = 1; seed < 40; ++seed)
	{
		PracticeSession a = Started(seed);
		PracticeSession b = Started(seed);
		RunUntilEnemy(a);
		RunUntilEnemy(b);
		if (a.Enemy().target == SkillId::Tornado)
			continue;
		++compared;
		PutInSlot(a, SkillId::Tornado, Slot::D);
		a.Input(InputAction::D);
		ResolveTornado(a);                                                     // wrong Tornado
		InvokeSkill(b, Different(b.Enemy().target));
		b.Input(InputAction::D);                                               // any other wrong spell
		const Stats& sa = a.GetStats();
		const Stats& sb = b.GetStats();
		CHECK(sa.hp == sb.hp && sa.score == sb.score && sa.combo == sb.combo && sa.bestCombo == sb.bestCombo);
		CHECK(sa.correctCasts == sb.correctCasts && sa.incorrectCasts == sb.incorrectCasts && Near(sa.Accuracy(), sb.Accuracy()));
		CHECK(a.Enemy().active && b.Enemy().active);
	}
	CHECK(compared > 10);

	// unlimited retries: every wrong hit is one more wrong cast, the enemy still stands
	PracticeSession r = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(r, SkillId::Tornado, Slot::D);
	for (int i = 1; i <= 3; ++i)
	{
		r.Input(InputAction::D);
		ResolveTornado(r);
		CHECK(r.GetStats().incorrectCasts == i && r.Enemy().active && r.GetStats().hp == 3 && r.GetStats().score == 0);
	}
	CHECK(r.GetStats().correctCasts == 0 && Near(r.GetStats().Accuracy(), 0.0));

	// a wrong-target enemy that is never answered correctly still costs HP by reaching the player (existing rule)
	PracticeSession l = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(l, SkillId::Tornado, Slot::D);
	l.Input(InputAction::D);
	ResolveTornado(l);
	CHECK(RunUntilLeak(l).leaked && l.GetStats().hp == 2 && l.GetStats().combo == 0);
}

// CONFIRMED rule C: no enemy, no projectile, and the cast does not exist for the statistics.
static void TestTornadoNoEnemy()
{
	// with some history, so "unchanged" means something
	PracticeSession s = Started(3);
	Kill(s);
	Kill(s);
	const Stats before = s.GetStats();
	CHECK(before.correctCasts == 2 && before.combo == 2);

	CHECK(!s.Enemy().active);                             // between two enemies
	PutInSlot(s, SkillId::Tornado, Slot::D);
	CHECK(!s.Enemy().active);
	InputResult r = s.Input(InputAction::D);
	CHECK(r.accepted && r.invoker.event == invoker::InvokerEvent::Cast && r.invoker.skill == SkillId::Tornado);
	CHECK(!r.tornadoLaunched && r.cast == CastOutcome::None);
	CHECK(s.ActiveTornadoCount() == 0);
	CHECK(!s.LaunchTornado(1.0f, 0.0f) && s.ActiveTornadoCount() == 0);  // the direct call agrees
	const Stats& after = s.GetStats();
	CHECK(after.hp == before.hp && after.score == before.score && after.combo == before.combo && after.bestCombo == before.bestCombo);
	CHECK(after.correctCasts == before.correctCasts && after.incorrectCasts == before.incorrectCasts);
	CHECK(Near(after.Accuracy(), before.Accuracy()));
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::Tornado);  // the spell is still invoked

	// same from F, and several times in a row
	PutInSlot(s, SkillId::Tornado, Slot::F);
	CHECK(s.Invoker().GetSlot(Slot::F) == SkillId::Tornado);
	for (int i = 0; i < 5; ++i)
		CHECK(!s.Input(InputAction::F).tornadoLaunched);
	CHECK(s.ActiveTornadoCount() == 0 && s.GetStats().TotalCasts() == before.TotalCasts());

	// nothing was left behind: the next enemy is not hit by a cast made while nobody was there
	RunUntilEnemy(s);
	for (int i = 0; i < 20; ++i)
		s.Update(0.02f);
	CHECK(s.GetStats().TotalCasts() == before.TotalCasts() && s.GetStats().score == before.score);

	// before the first enemy of a session as well
	PracticeSession first = Started(4);
	PutInSlot(first, SkillId::Tornado, Slot::D);
	CHECK(!first.Enemy().active && !first.Input(InputAction::D).tornadoLaunched);
	CHECK(first.ActiveTornadoCount() == 0 && first.GetStats().TotalCasts() == 0);
}

// CONFIRMED rule D: no gameplay limit on the number of projectiles; every valid cast makes one.
static void TestTornadoManyProjectiles()
{
	const int N = 25;                                     // far above the old limit of 4

	// aimed at an enemy that needs another spell: N casts, N projectiles alive together
	PracticeSession w = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(w, SkillId::Tornado, Slot::D);
	for (int i = 1; i <= N; ++i)
	{
		CHECK(w.Input(InputAction::D).tornadoLaunched);
		CHECK(w.ActiveTornadoCount() == i);
	}
	CHECK(w.GetStats().TotalCasts() == 0);                // nothing is judged until a projectile arrives
	for (int i = 0; i < 500 && w.ActiveTornadoCount() > 0; ++i)
		w.Update(0.02f);
	CHECK(w.ActiveTornadoCount() == 0);
	CHECK(w.GetStats().incorrectCasts == N && w.GetStats().correctCasts == 0);  // each projectile: one wrong cast
	CHECK(w.Enemy().active && w.GetStats().hp == 3);

	// aimed at the right enemy: the first hit wins, the others find nobody and change nothing
	PracticeSession c = StartedWithTarget(SkillId::Tornado);
	PutInSlot(c, SkillId::Tornado, Slot::D);
	for (int i = 1; i <= N; ++i)
	{
		c.Input(InputAction::D);
		CHECK(c.ActiveTornadoCount() == i);
	}
	int correct = 0;
	for (int i = 0; i < 500 && c.ActiveTornadoCount() > 0; ++i)
		if (c.Update(0.02f).cast == CastOutcome::Correct)
			++correct;
	CHECK(correct == 1 && c.GetStats().score == 1 && c.GetStats().combo == 1);
	CHECK(c.GetStats().correctCasts == 1 && c.GetStats().incorrectCasts == 0 && c.GetStats().hp == 3);
	CHECK(c.ActiveTornadoCount() == 0 && !c.Enemy().active);

	// projectiles launched at different moments coexist and move independently
	PracticeSession m = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(m, SkillId::Tornado, Slot::D);
	m.Input(InputAction::D);
	m.Update(0.04f);
	m.Input(InputAction::D);
	m.Update(0.04f);
	m.Input(InputAction::D);
	CHECK(m.ActiveTornadoCount() == 3);
	if (m.ActiveTornadoCount() == 3)                      // (an index outside the list would abort a Debug run)
	{
		CHECK(m.GetTornado(0).travelled > m.GetTornado(1).travelled && m.GetTornado(1).travelled > m.GetTornado(2).travelled);
		CHECK(m.GetTornado(0).animTime > m.GetTornado(1).animTime && m.GetTornado(2).animTime == 0.0f);
	}
}

// The animation of a live projectile follows its own clock (not its speed) and wraps 15 -> 0.
static void TestTornadoAnimationLoop()
{
	// the frame sequence over three cycles: 0..15, 0..15, 0..15
	int frames[3 * TORNADO_FRAME_COUNT];
	for (int k = 0; k < 3 * TORNADO_FRAME_COUNT; ++k)
		frames[k] = TornadoFrame((static_cast<float>(k) + 0.5f) / TORNADO_ANIM_FPS);
	for (int k = 0; k < 3 * TORNADO_FRAME_COUNT; ++k)
		CHECK(frames[k] == k % TORNADO_FRAME_COUNT);
	CHECK(frames[15] == 15 && frames[16] == 0 && frames[31] == 15 && frames[32] == 0);

	// a real projectile shows the frame of its own age: frame 0 at once, then one more every 0.1 s
	PracticeSession s = StartedWithTargetNot(SkillId::Tornado);
	PutInSlot(s, SkillId::Tornado, Slot::D);
	s.Input(InputAction::D);
	CHECK(s.ActiveTornadoCount() == 1);
	if (s.ActiveTornadoCount() == 1)
		CHECK(TornadoFrame(s.GetTornado(0).animTime) == 0);
	s.Update(0.1f);
	CHECK(s.ActiveTornadoCount() == 1 && TornadoFrame(s.GetTornado(0).animTime) == 1);
	s.Update(0.1f);
	CHECK(s.ActiveTornadoCount() == 1 && TornadoFrame(s.GetTornado(0).animTime) == 2);

	// one that is removed early (it hit, or left the field) simply never reaches the end of the cycle
	int lastFrame = 0;
	for (int i = 0; i < 500 && s.ActiveTornadoCount() > 0; ++i)
	{
		lastFrame = TornadoFrame(s.GetTornado(0).animTime);
		s.Update(0.02f);
	}
	CHECK(s.ActiveTornadoCount() == 0);
	CHECK(lastFrame > 2 && lastFrame < TORNADO_FRAME_COUNT);
}

static void TestTornadoLifetime()
{
	PracticeSession s = StartedWithTargetNot(SkillId::Tornado);
	CHECK(s.LaunchTornado(0.0f, -1.0f) && s.ActiveTornadoCount() == 1);
	s.Start(9);                                          // restart
	CHECK(s.ActiveTornadoCount() == 0);

	CHECK(!s.LaunchTornado(1.0f, 0.0f));                 // no enemy yet: nothing to aim at
	RunUntilEnemy(s);
	CHECK(s.LaunchTornado(0.0f, -1.0f) && s.ActiveTornadoCount() == 1);
	s.ReturnToReady();
	CHECK(s.ActiveTornadoCount() == 0);
	CHECK(!s.LaunchTornado(0.0f, -1.0f));                // Ready: nothing runs

	// Game Over removes them too (nothing keeps flying behind the Game Over screen)
	s.Start(4);
	for (int leak = 0; leak < START_HP - 1; ++leak)
	{
		RunUntilEnemy(s);
		RunUntilLeak(s);
	}
	RunUntilEnemy(s);
	for (int i = 0; i < 2000 && s.Enemy().x > HIT_LINE_X + 30.0f; ++i)
		s.Update(0.02f);
	CHECK(s.LaunchTornado(0.0f, -1.0f) && s.ActiveTornadoCount() == 1);  // up: it cannot reach the enemy
	for (int i = 0; i < 50 && s.State() == GameState::Playing; ++i)
		s.Update(0.02f);
	CHECK(s.State() == GameState::GameOver);
	CHECK(s.ActiveTornadoCount() == 0);
	CHECK(s.GetStats().hp == 0 && s.GetStats().score == 0 && s.GetStats().TotalCasts() == 0);
}

// 14. difficulty
static void TestDifficulty()
{
	DifficultyParams d0 = DifficultyAt(0.0f);
	CHECK(d0.enemySpeed == START_ENEMY_SPEED && d0.challengeDelay == START_CHALLENGE_DELAY);
	CHECK(DifficultyAt(-100.0f).enemySpeed == START_ENEMY_SPEED);            // negative time is clamped
	CHECK(DifficultyAt(1e9f).enemySpeed == MAX_ENEMY_SPEED);
	CHECK(DifficultyAt(1e9f).challengeDelay == MIN_CHALLENGE_DELAY);

	float prevSpeed = 0.0f;
	float prevDelay = 1e9f;
	bool inRange = true, monotonic = true, smooth = true;
	for (float t = 0.0f; t <= 10000.0f; t += 0.5f)
	{
		DifficultyParams d = DifficultyAt(t);
		if (d.enemySpeed < START_ENEMY_SPEED || d.enemySpeed > MAX_ENEMY_SPEED) inRange = false;
		if (d.challengeDelay < MIN_CHALLENGE_DELAY || d.challengeDelay > START_CHALLENGE_DELAY) inRange = false;
		if (d.enemySpeed < prevSpeed || d.challengeDelay > prevDelay) monotonic = false;  // never easier over time
		if (t > 0.0f && d.enemySpeed - prevSpeed > ENEMY_SPEED_GROWTH * 0.5f + 0.001f) smooth = false;  // no jumps
		prevSpeed = d.enemySpeed;
		prevDelay = d.challengeDelay;
	}
	CHECK(inRange);
	CHECK(monotonic);
	CHECK(smooth);
	CHECK(DifficultyAt(60.0f).enemySpeed > DifficultyAt(0.0f).enemySpeed);   // it really gets harder
	CHECK(DifficultyAt(60.0f).challengeDelay < DifficultyAt(0.0f).challengeDelay);

	// playable at both ends: a generous window at the start, still humanly possible at the cap
	float distance = SPAWN_X - HIT_LINE_X;
	CHECK(distance / START_ENEMY_SPEED >= 5.0f);
	CHECK(distance / MAX_ENEMY_SPEED >= 2.0f);
	CHECK(MIN_CHALLENGE_DELAY > 0.0f);
}

static void TestTimeStep()
{
	PracticeSession s = Started();
	RunUntilEnemy(s);
	float x = s.Enemy().x;
	float speed = s.Enemy().speed;

	s.Update(-1.0f);                                // negative dt: nothing moves
	CHECK(s.Enemy().x == x);

	s.Update(5.0f);                                 // huge dt (window hitch): clamped
	CHECK(NearF(s.Enemy().x, x - speed * MAX_FRAME_TIME, 0.01f));

	// movement does not depend on how the time is sliced
	PracticeSession a = Started(3), b = Started(3);
	RunUntilEnemy(a); RunUntilEnemy(b);
	float ax = a.Enemy().x, bx = b.Enemy().x;
	for (int i = 0; i < 10; ++i) a.Update(0.05f);   // 0.5 s in 10 steps
	for (int i = 0; i < 5; ++i) b.Update(0.1f);     // 0.5 s in 5 steps
	CHECK(NearF(a.Enemy().x, b.Enemy().x, 0.01f));
	CHECK(a.Enemy().x < ax && b.Enemy().x < bx);
}

static void TestInputOutsidePlaying()
{
	PracticeSession s;                              // Ready
	PressLetters(s, "QQQR");
	CHECK(s.Invoker().OrbCount() == 0 && s.Invoker().GetSlot(Slot::D) == SkillId::None);

	s.Start(1);                                     // Playing: the same keys work
	PressLetters(s, "QQQR");
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::ColdSnap);

	LoseAllHp(s);                                   // Game Over: ignored again
	CHECK(s.State() == GameState::GameOver);
	int orbs = s.Invoker().OrbCount();
	PressLetters(s, "WWW");
	CHECK(s.Invoker().OrbCount() == orbs);
}

// The Invoker Core behaves exactly as before when it is driven through the session.
static void TestInvokerThroughSession()
{
	PracticeSession s = Started();
	PressLetters(s, "QQW");  PressLetters(s, "R");
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::GhostWalk);
	PressLetters(s, "QWQ");  PressLetters(s, "R");   // permutations give the same spell: nothing changes
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::GhostWalk && s.Invoker().GetSlot(Slot::F) == SkillId::None);
	PressLetters(s, "WQQ");  PressLetters(s, "R");
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::GhostWalk && s.Invoker().GetSlot(Slot::F) == SkillId::None);

	PressLetters(s, "EEE");  PressLetters(s, "R");   // D = newest, F = previous
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::SunStrike && s.Invoker().GetSlot(Slot::F) == SkillId::GhostWalk);
	PressLetters(s, "EEE");  PressLetters(s, "R");   // re-invoking the spell in D keeps both slots
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::SunStrike && s.Invoker().GetSlot(Slot::F) == SkillId::GhostWalk);

	PressLetters(s, "QWE");  PressLetters(s, "R");   // Deafening Blast
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::DeafeningBlast);
	CHECK(s.Invoker().GetSlot(Slot::F) == SkillId::SunStrike);

	PracticeSession p = Started();                   // fewer than 3 orbs: no spell
	PressLetters(p, "QW");  PressLetters(p, "R");
	CHECK(p.Invoker().GetSlot(Slot::D) == SkillId::None && p.Invoker().OrbCount() == 2);
}

// ---------------------------------------------------------------- recipe hint (spec §17): a hinted run counts like any other

static void TestAssistedRuns()
{
	// MergeBests takes an assisted run's records (owner 2026-10-01: the hint no longer keeps a run out of the ranks)
	BestStats bests = { 3, 2, 10.0f };
	Stats st = { 0, 3, 50, 0, 40, 60, 0, 300.0f, true };
	BestUpdate u = MergeBests(bests, st);
	CHECK(u.score && u.combo && u.survivalTime);
	CHECK(bests.score == 50 && bests.combo == 40 && NearF(bests.survivalTime, 300.0f, 1e-6f));

	// outside Playing MarkAssisted does nothing; a new run starts unassisted
	PracticeSession s;
	s.MarkAssisted();
	CHECK(!s.GetStats().assisted);
	s.RestoreBestCombo(2);
	s.Start(5);
	CHECK(!s.GetStats().assisted);

	// turning the hint on marks the run and changes nothing else: the combo record keeps counting
	Kill(s); Kill(s); Kill(s);
	CHECK(s.GetStats().combo == 3 && s.GetStats().bestCombo == 3);
	s.MarkAssisted();
	CHECK(s.GetStats().assisted && s.GetStats().bestCombo == 3 && s.GetStats().combo == 3);
	Kill(s);
	CHECK(s.GetStats().combo == 4 && s.GetStats().bestCombo == 4 && s.GetStats().score == 4);
	BestStats record = { 0, 0, 0.0f };
	CHECK(MergeBests(record, s.GetStats()).Any() && record.score == 4 && record.combo == 4);

	// the mark ends with the run; the record stays
	s.Start(6);
	CHECK(!s.GetStats().assisted && s.GetStats().bestCombo == 4);
}

// ---------------------------------------------------------------- leaderboard order (survival time)

static void TestTopRuns()
{
	TopRun list[TOP_RUNS] = {};
	CHECK(InsertTopRun(list, { 0.0f, 5 }) == 0);          // no time survived: never listed
	CHECK(InsertTopRun(list, { 30.0f, 4 }) == 1);
	CHECK(InsertTopRun(list, { 50.0f, 2 }) == 1);         // longer time wins, whatever the score
	CHECK(InsertTopRun(list, { 40.0f, 9 }) == 2);
	CHECK(NearF(list[0].survivalTime, 50.0f, 1e-6f) && NearF(list[1].survivalTime, 40.0f, 1e-6f));
	CHECK(NearF(list[2].survivalTime, 30.0f, 1e-6f) && list[3].survivalTime == 0.0f);
	CHECK(InsertTopRun(list, { 40.0f, 12 }) == 2);        // same time, higher score: above
	CHECK(InsertTopRun(list, { 40.0f, 12 }) == 3);        // exact tie: below the existing one
	CHECK(list[1].score == 12 && list[2].score == 12 && list[3].score == 9);

	// fill up: the list keeps the best TOP_RUNS, a worse run does not get in
	for (int i = 0; i < 20; ++i)
		InsertTopRun(list, { 100.0f + i, i });
	CHECK(NearF(list[0].survivalTime, 119.0f, 1e-6f) && NearF(list[TOP_RUNS - 1].survivalTime, 110.0f, 1e-6f));
	CHECK(InsertTopRun(list, { 60.0f, 99 }) == 0);
	CHECK(InsertTopRun(list, { 115.5f, 0 }) == 5);
	bool sorted = true;
	for (int i = 1; i < TOP_RUNS; ++i)
		if (list[i].survivalTime > list[i - 1].survivalTime)
			sorted = false;
	CHECK(sorted);
}

// ---------------------------------------------------------------- tutorial (spec §24)

static InputAction ActionFor(char key)
{
	switch (key)
	{
	case 'Q': return InputAction::Q;
	case 'W': return InputAction::W;
	case 'E': return InputAction::E;
	case 'R': return InputAction::R;
	case 'D': return InputAction::D;
	default:  return InputAction::F;
	}
}

// Plays the tutorial from its current step up to (not into) the first step of `kind`, pressing NEXT on cards and
// the expected key on key steps; also gives the dummy time to walk. Returns false if it got stuck.
static bool TutorialAdvanceTo(TutorialSession& t, TutorialStepKind kind)
{
	for (int guard = 0; guard < 200 && t.Step().kind != kind; ++guard)
	{
		t.Update(0.05f);
		if (t.Step().kind == TutorialStepKind::Card)
			t.Next();
		else if (t.Step().kind == TutorialStepKind::Keys)
			t.Input(ActionFor(t.ExpectedKey()));
		else
			return false;
	}
	return t.Step().kind == kind;
}

// The script itself: 4 lessons, one guided spell (Sun Strike), a run, the end; text fits the screen.
static void TestTutorialScript()
{
	CHECK(TutorialStepCount() > 10);
	CHECK(GetTutorialStep(0).lesson == 1);
	CHECK(GetTutorialStep(TutorialStepCount() - 1).kind == TutorialStepKind::Done);
	int runSteps = 0, keySteps = 0, lastLesson = 1;
	bool lessonsInOrder = true, sunStrikeGuided = false, keysValid = true, linesFit = true;
	for (int i = 0; i < TutorialStepCount(); ++i)
	{
		const TutorialStep& s = GetTutorialStep(i);
		if (s.lesson < lastLesson || s.lesson > TUTORIAL_LESSON_COUNT)
			lessonsInOrder = false;
		lastLesson = s.lesson;
		if (s.kind == TutorialStepKind::Run)
			++runSteps;
		if (s.kind == TutorialStepKind::Keys)
		{
			++keySteps;
			if (std::strlen(s.keys) == 0 || std::strspn(s.keys, "QWERDF") != std::strlen(s.keys))
				keysValid = false;
			if (std::strcmp(s.keys, "EEE") == 0)
				sunStrikeGuided = true;
		}
		if (std::strlen(s.line1) > 40 || std::strlen(s.line2) > 40)
			linesFit = false;
	}
	CHECK(lessonsInOrder);
	CHECK(runSteps == 1);
	CHECK(keySteps >= 4);
	CHECK(keysValid);
	CHECK(sunStrikeGuided);
	CHECK(linesFit);

	// NEXT only works on cards; gameplay keys do nothing on a card
	TutorialSession t;
	t.Start(1);
	CHECK(t.StepIndex() == 0 && t.Step().kind == TutorialStepKind::Card);
	TutorialInputResult r = t.Input(InputAction::Q);
	CHECK(!r.accepted && !r.wrongKey && t.Invoker().OrbCount() == 0);
	CHECK(t.Next() && t.StepIndex() == 1);
	CHECK(t.Step().kind == TutorialStepKind::Keys && !t.Next() && t.StepIndex() == 1);
}

// A key step waits for exactly the expected key; wrong keys change nothing.
static void TestTutorialGuidedKeys()
{
	TutorialSession t;
	t.Start(1);
	t.Next();                                    // -> "PRESS Q, W AND E"
	CHECK(std::strcmp(t.Step().keys, "QWE") == 0 && t.ExpectedKey() == 'Q');

	TutorialInputResult r = t.Input(InputAction::E);  // wrong
	CHECK(!r.accepted && r.wrongKey && t.KeysDone() == 0 && t.Invoker().OrbCount() == 0);
	r = t.Input(InputAction::R);
	CHECK(r.wrongKey && t.Invoker().OrbCount() == 0);

	r = t.Input(InputAction::Q);
	CHECK(r.accepted && !r.stepAdvanced && t.KeysDone() == 1 && t.ExpectedKey() == 'W');
	CHECK(r.invoker.event == invoker::InvokerEvent::OrbAdded && t.Invoker().OrbCount() == 1);
	t.Input(InputAction::W);
	r = t.Input(InputAction::E);
	CHECK(r.accepted && r.stepAdvanced && t.Step().kind == TutorialStepKind::Card);  // the sequence is done
	CHECK(t.Invoker().OrbCount() == 3);

	t.Next();                                    // -> "PRESS Q ONCE MORE": the 4th orb rolls out the oldest
	t.Input(InputAction::Q);
	CHECK(t.Invoker().OrbCount() == 3);
	CHECK(t.Invoker().GetOrb(0) == invoker::Orb::Wex && t.Invoker().GetOrb(2) == invoker::Orb::Quas);
}

// Lesson 2: the dummy walks in and stops; E E E R puts Sun Strike in D; D defeats the dummy.
static void TestTutorialSunStrike()
{
	TutorialSession t;
	t.Start(3);
	while (t.Step().lesson < 2)
	{
		if (t.Step().kind == TutorialStepKind::Card) t.Next();
		else t.Input(ActionFor(t.ExpectedKey()));
	}
	CHECK(t.Enemy().active && t.Enemy().target == SkillId::SunStrike);
	CHECK(GetEnemyDefinition(t.Enemy().definition).targetSkill == SkillId::SunStrike);
	CHECK(t.Step().highlight & HIGHLIGHT_ENEMY);
	CHECK(t.Step().highlight & HIGHLIGHT_TARGET);
	for (int i = 0; i < 100; ++i)
		t.Update(0.05f);
	CHECK(NearF(t.Enemy().x, TUTORIAL_DUMMY_STOP_X, 0.01f));  // stopped, never reaches the player
	CHECK(t.Enemy().active);

	// to the "EEE" step
	while (t.Step().kind != TutorialStepKind::Keys) t.Next();
	CHECK(std::strcmp(t.Step().keys, "EEE") == 0);
	t.Input(InputAction::E); t.Input(InputAction::E); t.Input(InputAction::E);
	while (t.Step().kind != TutorialStepKind::Keys) t.Next();
	CHECK(t.ExpectedKey() == 'R');
	TutorialInputResult r = t.Input(InputAction::R);
	CHECK(r.invoker.event == invoker::InvokerEvent::Invoked && r.invoker.skill == SkillId::SunStrike);
	CHECK(t.Invoker().GetSlot(Slot::D) == SkillId::SunStrike);
	while (t.Step().kind != TutorialStepKind::Keys) t.Next();
	CHECK(t.ExpectedKey() == 'D');
	CHECK(!t.Input(InputAction::F).accepted);   // F would cast the empty slot: refused
	r = t.Input(InputAction::D);
	CHECK(r.cast == CastOutcome::Correct && r.stepAdvanced);
	CHECK(!t.Enemy().active);
	CHECK(t.Step().lesson == 2 && t.Step().kind == TutorialStepKind::Card);  // "RIGHT SPELL = ENEMY GONE"
}

// Lesson 4: free play against slow enemies; right spells win, wrong ones miss, three enemies end it.
static void TestTutorialRun()
{
	TutorialSession t;
	t.Start(7);
	CHECK(TutorialAdvanceTo(t, TutorialStepKind::Run));
	CHECK(t.Hearts() == START_HP && t.RunResolved() == 0 && !t.Enemy().active);

	SkillId previous = SkillId::None;
	for (int n = 0; n < TUTORIAL_RUN_ENEMIES; ++n)
	{
		bool spawned = false;
		for (int i = 0; i < 100 && !spawned; ++i)
			spawned = t.Update(0.05f).spawned;
		CHECK(spawned && t.Enemy().active);
		CHECK(NearF(t.Enemy().speed, TUTORIAL_RUN_SPEED, 0.01f));
		CHECK(t.Enemy().target != previous);     // never the same target twice in a row
		previous = t.Enemy().target;

		// a wrong spell first: a miss, the enemy stays
		SkillId wrong = t.Enemy().target == SkillId::ColdSnap ? SkillId::SunStrike : SkillId::ColdSnap;
		invoker::Recipe w = invoker::GetSkillDefinition(wrong).recipe;
		for (int k = 0; k < w.quas; ++k) t.Input(InputAction::Q);
		for (int k = 0; k < w.wex; ++k) t.Input(InputAction::W);
		for (int k = 0; k < w.exort; ++k) t.Input(InputAction::E);
		t.Input(InputAction::R);
		TutorialInputResult r = t.Input(InputAction::D);
		CHECK(r.accepted && r.cast == CastOutcome::Incorrect && t.Enemy().active);

		// then the right one (every spell, Tornado included, is judged when cast in the tutorial)
		invoker::Recipe ok = invoker::GetSkillDefinition(t.Enemy().target).recipe;
		for (int k = 0; k < ok.quas; ++k) t.Input(InputAction::Q);
		for (int k = 0; k < ok.wex; ++k) t.Input(InputAction::W);
		for (int k = 0; k < ok.exort; ++k) t.Input(InputAction::E);
		t.Input(InputAction::R);
		r = t.Input(InputAction::D);
		CHECK(r.cast == CastOutcome::Correct && !t.Enemy().active);
		CHECK(t.RunDefeated() == n + 1);
		CHECK(r.stepAdvanced == (n == TUTORIAL_RUN_ENEMIES - 1));
	}
	CHECK(t.IsDone() && t.Hearts() == START_HP);
	CHECK(!t.Next());                            // the end card waits for the menu choice
	CHECK(!t.Input(InputAction::Q).accepted);
}

// A leak costs a heart and counts as one enemy of the run; the run never ends in Game Over.
static void TestTutorialRunLeaks()
{
	TutorialSession t;
	t.Start(11);
	CHECK(TutorialAdvanceTo(t, TutorialStepKind::Run));
	int leaks = 0;
	bool finished = false;
	for (int i = 0; i < 20000 && !finished; ++i)
	{
		TutorialUpdateResult u = t.Update(0.05f);
		if (u.leaked)
		{
			++leaks;
			CHECK(t.Hearts() == START_HP - leaks);
		}
		finished = u.finished;
	}
	CHECK(finished && leaks == TUTORIAL_RUN_ENEMIES);
	CHECK(t.IsDone() && t.Hearts() == 0 && t.RunDefeated() == 0);

	// the tutorial starts over cleanly
	t.Start(12);
	CHECK(t.StepIndex() == 0 && !t.Enemy().active && t.Invoker().OrbCount() == 0 && t.Hearts() == START_HP);
}

// ---------------------------------------------------------------- Boss mode (spec §25)

struct BossTotals  // every flag seen while advancing, and the first failure
{
	bool lifted, landed, comboComplete, playerHit, won, lost;
	ComboFail fail;
	int damage;            // sum of the combos' damage
	int perfect, great, good, missed;
};

static void BossKeys(BossSession& s, const char* keys, BossTotals* totals = NULL)
{
	for (; *keys; ++keys)
	{
		InputAction a = InputAction::Q;
		if (*keys == 'W') a = InputAction::W;
		else if (*keys == 'E') a = InputAction::E;
		else if (*keys == 'R') a = InputAction::R;
		else if (*keys == 'D') a = InputAction::D;
		else if (*keys == 'F') a = InputAction::F;
		BossInputResult r = s.Input(a);
		if (totals != NULL && r.fail != ComboFail::None && totals->fail == ComboFail::None)
			totals->fail = r.fail;
	}
}

static void BossAdd(BossTotals& t, const BossUpdateResult& r)
{
	t.lifted |= r.lifted; t.landed |= r.landed; t.comboComplete |= r.comboComplete;
	t.playerHit |= r.playerHit; t.won |= r.won; t.lost |= r.lost;
	if (t.fail == ComboFail::None)
		t.fail = r.fail;
	t.damage += r.damage;
	for (int i = 0; i < r.impactCount; ++i)
	{
		switch (r.impacts[i].grade)
		{
		case HitGrade::Perfect: ++t.perfect; break;
		case HitGrade::Great:   ++t.great; break;
		case HitGrade::Good:    ++t.good; break;
		case HitGrade::Miss:    ++t.missed; break;
		default: break;
		}
	}
}

// Advances in 10 ms steps until `stop` says so (or `limit` seconds pass); false on the time limit.
static bool BossRunUntil(BossSession& s, BossTotals& t, bool (*stop)(const BossSession&, const BossTotals&), float limit)
{
	for (float time = 0.0f; time < limit; time += 0.01f)
	{
		if (stop(s, t))
			return true;
		BossAdd(t, s.Update(0.01f));
	}
	return stop(s, t);
}

static void BossWait(BossSession& s, BossTotals& t, float seconds)
{
	for (float time = 0.0f; time + 0.005f < seconds; time += 0.01f)
		BossAdd(t, s.Update(0.01f));
}

// Advances until the ideal moment to cast combo step `step` (its timing bar is empty).
static bool BossWaitIdeal(BossSession& s, BossTotals& t, int step)
{
	for (int i = 0; i < 600; ++i)
	{
		float until = 0.0f, span = 0.0f;
		if (s.StepTiming(step, until, span) && until <= 0.005f)
			return true;
		BossAdd(t, s.Update(0.01f));
	}
	return false;
}

static bool Lifted(const BossSession& s, const BossTotals& t) { return t.lifted && s.Phase() == BossPhase::Airborne; }
static bool AirAt(const BossSession& s, float airTime) { return s.Phase() == BossPhase::Airborne && s.AirTime() >= airTime; }
static bool Air11(const BossSession& s, const BossTotals&) { return AirAt(s, 1.1f); }
static bool Air15(const BossSession& s, const BossTotals&) { return AirAt(s, 1.5f); }
static bool Air24(const BossSession& s, const BossTotals&) { return AirAt(s, 2.4f); }
static bool ComboResolved(const BossSession&, const BossTotals& t) { return t.comboComplete || t.fail != ComboFail::None; }
static bool PlayerWasHit(const BossSession&, const BossTotals& t) { return t.playerHit; }
static bool FightLost(const BossSession& s, const BossTotals&) { return s.State() == BossState::Lost; }
static bool NearPlayer(const BossSession& s, const BossTotals&) { return s.X() < HIT_LINE_X + 10.0f; }
static bool ComboOrEnd(const BossSession& s, const BossTotals& t) { return t.comboComplete || s.State() != BossState::Fighting; }
static bool ReadyForTornado(const BossSession& s, const BossTotals&) { return s.Phase() == BossPhase::Walking && !s.AttemptRunning(); }

// The eight bosses of updates 1.2 and 1.5. They left the game in 1.9 (the Immortals took their place), but they
// exercise every part of the fight (the guided cue, quick steps, holds, two phases), so the tests keep them.
static const BossDefinition kClassic[8] =
{
	{ "STONE KNIGHT", 9, 3.0f, { 190, 200, 215 }, { SkillId::Tornado, SkillId::SunStrike }, 2, 40.0f, 1.2f, true, {}, 0, 0.0f, 0.0f },
	{ "DARK WIZARD", 3, 2.5f, { 200, 150, 255 },
		{ SkillId::Tornado, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 3, 45.0f, 1.0f, false, {}, 0, 0.0f, 0.0f },
	{ "KITSUNE QUEEN", 8, 1.8f, { 255, 150, 120 },
		{ SkillId::Tornado, SkillId::EMP, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 4, 50.0f, 1.0f, false, {}, 0, 0.0f, 0.0f },
	{ "FROST TROLL", 0, 3.0f, { 150, 210, 255 }, { SkillId::ColdSnap, SkillId::SunStrike }, 2, 45.0f, 1.0f, false, {}, 0, 0.0f, 0.0f },
	{ "GLACIER GOLEM", 1, 2.6f, { 190, 225, 255 },
		{ SkillId::IceWall, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 3, 50.0f, 1.0f, false, {}, 0, 0.0f, 0.0f },
	{ "FIRE IMP", 2, 2.0f, { 255, 170, 110 },
		{ SkillId::ColdSnap, SkillId::Alacrity, SkillId::ForgeSpirit }, 3, 55.0f, 1.0f, false, {}, 0, 0.0f, 0.0f },
	{ "SHADOW ASSASSIN", 6, 2.0f, { 170, 130, 220 },
		{ SkillId::GhostWalk, SkillId::Tornado, SkillId::SunStrike, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 5,
		50.0f, 1.2f, false, {}, 0, 0.0f, 0.0f },
	{ "ARCHON", 4, 3.4f, { 255, 225, 120 },
		{ SkillId::Tornado, SkillId::EMP, SkillId::SunStrike, SkillId::ChaosMeteor, SkillId::DeafeningBlast }, 5,
		50.0f, 1.4f, false,
		{ SkillId::IceWall, SkillId::ColdSnap, SkillId::ForgeSpirit, SkillId::Alacrity }, 4, 0.0f, 0.0f },
};

// Plays the combo of the fight once: every landing step at its ideal moment, every quick step at once.
static void BossPlayCombo(BossSession& s, BossTotals& t)
{
	BossRunUntil(s, t, ReadyForTornado, 8.0f);
	int length = s.ComboLength();
	for (int step = 0; step < length && s.State() == BossState::Fighting; ++step)
	{
		std::string keys = invoker::RecipeLetters(s.Combo()[step]) + "R";
		BossKeys(s, keys.c_str());
		if (s.StepKind(step) == BossStepKind::Landing)
			BossWaitIdeal(s, t, step);
		BossKeys(s, "D", &t);
	}
	t.comboComplete = false;
	BossRunUntil(s, t, ComboOrEnd, 8.0f);
}

// §32 M-6: the five Immortals, combos of 4 to 8 spells, each beaten by one perfect combo.
static void TestBossDefinitions()
{
	CHECK(BOSS_COUNT == 5 && BOSS_MAX_COMBO == 8);
	const char* names[BOSS_COUNT] = { "RIMEFANG", "CINDERMAW", "GRAVEHORN", "VOLTARA", "THE HOLLOW KING" };
	for (int i = 0; i < BOSS_COUNT; ++i)
	{
		const BossDefinition& d = GetBossDefinition(i);
		CHECK(std::strcmp(d.name, names[i]) == 0);
		CHECK(d.comboLength == 4 + i && d.combo2Length == 0 && !d.guided);
		CHECK(d.combo[d.comboLength - 1] == (i == 2 ? SkillId::ForgeSpirit : i == 3 ? SkillId::IceWall : SkillId::DeafeningBlast));
		CHECK(d.speed > 0.0f && d.window > BOSS_GREAT_TIME);
		CHECK(d.enemyDefinition >= 0 && d.enemyDefinition < ENEMY_TYPE_COUNT);
		CHECK(d.bodyWidth > 100.0f && d.bodyHeight > 100.0f);  // each has its own art, with its body in field pixels
		BossSession s;
		s.Start(i);
		CHECK(s.BossIndex() == i && s.ComboLength() == 4 + i);
		Bounds body = s.Body();
		CHECK(body.w > 60.0f && body.h > 80.0f && NearF(body.y + body.h, GROUND_LINE_Y, 1.0f));
		BossTotals t = {};
		BossPlayCombo(s, t);
		CHECK(t.fail == ComboFail::None && t.comboComplete && t.damage == 100 && t.won && s.State() == BossState::Won);
	}
	CHECK(GetBossDefinition(0).combo[0] == SkillId::Tornado && GetBossDefinition(0).combo[1] == SkillId::EMP);
	CHECK(GetBossDefinition(4).combo[0] == SkillId::ForgeSpirit && GetBossDefinition(4).combo[4] == SkillId::Tornado);
	// B-21: a spell catches the landing once; the repeated Chaos Meteor / Deafening Blast are quick steps
	BossSession v;
	v.Start(3);  // Tornado, EMP, Chaos Meteor, Deafening Blast, Chaos Meteor, Deafening Blast, Ice Wall
	CHECK(v.StepKind(0) == BossStepKind::Opener && v.StepKind(1) == BossStepKind::Landing
		&& v.StepKind(2) == BossStepKind::Landing && v.StepKind(3) == BossStepKind::Landing);
	CHECK(v.StepKind(4) == BossStepKind::Quick && v.StepKind(5) == BossStepKind::Quick && v.StepKind(6) == BossStepKind::Quick);
	v.Start(4);  // Forge Spirit, Alacrity, Ice Wall, Cold Snap, Tornado, Sun Strike, Chaos Meteor, Deafening Blast
	CHECK(v.StepKind(1) == BossStepKind::Quick && v.StepKind(4) == BossStepKind::Quick);  // the Tornado itself is quick
	CHECK(v.StepKind(5) == BossStepKind::Landing && v.StepKind(6) == BossStepKind::Landing && v.StepKind(7) == BossStepKind::Landing);
	// a definition of the caller's own
	v.Start(kClassic[0]);
	CHECK(v.BossIndex() == -1 && v.Def().guided && v.ComboLength() == 2 && v.Speed() == kClassic[0].speed);
	CHECK(kClassic[0].guided && !kClassic[1].guided && !kClassic[2].guided);
	CHECK(kClassic[0].combo[1] == SkillId::SunStrike);
	CHECK(kClassic[2].comboLength == 4 && kClassic[2].combo[1] == SkillId::EMP);
	// B-5 timings
	CHECK(NearF(BossSpellDelay(SkillId::SunStrike), 1.7f, 1e-6f));
	CHECK(NearF(BossSpellDelay(SkillId::ChaosMeteor), 1.3f, 1e-6f));
	CHECK(NearF(BossSpellDelay(SkillId::EMP), 2.9f, 1e-6f));
	CHECK(BossSpellDelay(SkillId::Tornado) == 0.0f && BossSpellDelay(SkillId::ColdSnap) == 0.0f);
	CHECK(BossSpellRadius(SkillId::DeafeningBlast) == 0.0f && BossSpellRadius(SkillId::EMP) > BossSpellRadius(SkillId::SunStrike));
	CHECK(BossLiftHeight(0.0f) == 0.0f && BossLiftHeight(BOSS_LIFT_TIME) == 0.0f);
	CHECK(BossLiftHeight(1.2f) > BOSS_LIFT_HEIGHT * 0.8f);
}

// B-21: Gravehorn (Tornado, Sun Strike, Chaos Meteor, Deafening Blast, Cold Snap, Forge Spirit). The landing steps
// never cast are missed when the window closes, and the quick steps after them can still be cast.
static void TestBossLongCombo()
{
	BossSession s;
	s.Start(2);
	BossTotals t = {};
	BossKeys(s, "QWWR" "D");
	CHECK(s.AttemptRunning() && s.CastSteps() == 1);
	CHECK(BossRunUntil(s, t, Lifted, 3.0f));
	CHECK(BossRunUntil(s, t, [](const BossSession& b, const BossTotals&) { return b.CastSteps() == 4; }, 6.0f));
	CHECK(t.landed && t.fail == ComboFail::None && s.AttemptRunning());
	CHECK(s.StepGrade(1) == HitGrade::Miss && s.StepGrade(2) == HitGrade::Miss && s.StepGrade(3) == HitGrade::Miss);
	CHECK(s.StepGrade(4) == HitGrade::None);
	float left = 0.0f, total = 0.0f;
	CHECK(s.QuickTiming(4, left, total) && left > BOSS_QUICK_LIMIT - 0.05f);  // its time starts when they are passed
	BossKeys(s, "QQQR");
	CHECK(s.Input(InputAction::D).grade == HitGrade::Perfect && s.FreezeLeft() == BOSS_FREEZE_TIME);
	BossKeys(s, "QEER");
	CHECK(s.Input(InputAction::D).grade == HitGrade::Perfect);
	CHECK(BossRunUntil(s, t, ComboResolved, 1.0f));
	CHECK(t.comboComplete && t.damage == 40 && s.BossHp() == 60);  // (0 + 0 + 0 + 100 + 100) / 5

	// Voltara: the second Chaos Meteor and Deafening Blast are graded when cast, also while the first ones still fly
	BossSession v;
	v.Start(3);
	BossTotals tv = {};
	BossKeys(v, "QWWR" "D");
	BossKeys(v, "WWWR");
	CHECK(BossWaitIdeal(v, tv, 1));
	BossKeys(v, "D", &tv);
	BossKeys(v, "WEER");
	CHECK(BossWaitIdeal(v, tv, 2));
	BossKeys(v, "D", &tv);
	BossKeys(v, "QWER");
	CHECK(BossWaitIdeal(v, tv, 3));
	BossKeys(v, "D", &tv);
	CHECK(v.CastSteps() == 4 && v.StepGrade(3) == HitGrade::None);
	BossKeys(v, "WEER");
	CHECK(v.Input(InputAction::D).grade == HitGrade::Perfect && v.CastSteps() == 5);
	BossWait(v, tv, BOSS_QUICK_GREAT - 0.2f);
	BossKeys(v, "QWER");
	CHECK(v.Input(InputAction::D).grade == HitGrade::Great);
	BossWait(v, tv, BOSS_QUICK_LIMIT + 0.1f);  // Ice Wall never comes: TOO LATE, and the combo is over
	CHECK(tv.comboComplete && tv.fail == ComboFail::None);
	CHECK(tv.damage == (100 + 100 + 100 + 100 + 60 + 0 + 3) / 6);
}

// B-14: the grade of a spell by how long after the landing it hit, and its score.
static void TestBossGrades()
{
	CHECK(GradeHit(-0.01f, 1.0f) == HitGrade::Miss);   // still in the air
	CHECK(GradeHit(0.0f, 1.0f) == HitGrade::Perfect);
	CHECK(GradeHit(BOSS_PERFECT_TIME, 1.0f) == HitGrade::Perfect);
	CHECK(GradeHit(BOSS_PERFECT_TIME + 0.01f, 1.0f) == HitGrade::Great);
	CHECK(GradeHit(BOSS_GREAT_TIME, 1.0f) == HitGrade::Great);
	CHECK(GradeHit(BOSS_GREAT_TIME + 0.01f, 1.0f) == HitGrade::Good);
	CHECK(GradeHit(1.0f, 1.0f) == HitGrade::Good);
	CHECK(GradeHit(1.01f, 1.0f) == HitGrade::Miss);    // the window has closed
	CHECK(GradeScore(HitGrade::Perfect) == 100 && GradeScore(HitGrade::Great) == 60 && GradeScore(HitGrade::Good) == 35);
	CHECK(GradeScore(HitGrade::Miss) == 0 && GradeScore(HitGrade::None) == 0);
	CHECK(BOSS_IDEAL_AFTER_LANDING > 0.0f && BOSS_IDEAL_AFTER_LANDING < BOSS_PERFECT_TIME);
}

static void TestBossStart()
{
	BossSession s;
	CHECK(s.State() != BossState::Fighting);  // nothing is fought before Start()
	CHECK(!s.Input(InputAction::Q).accepted);
	s.Start(1);
	CHECK(s.State() == BossState::Fighting && s.BossIndex() == 1);
	CHECK(s.BossHp() == BOSS_FULL_HP && s.PlayerHp() == START_HP && s.Elapsed() == 0.0f);
	CHECK(s.X() == BOSS_START_X && s.Phase() == BossPhase::Walking && !s.AttemptRunning());
	float until = 0.0f, span = 0.0f;
	CHECK(!s.StepTiming(1, until, span));  // no attempt, no bar
	float x = s.X();
	s.Update(1.0f);  // clamped to MAX_FRAME_TIME
	CHECK(NearF(s.X(), x - GetBossDefinition(1).speed * MAX_FRAME_TIME, 1e-3f));
	s.MarkAssisted();
	CHECK(s.Assisted());
	s.Start(kClassic[1]);
	CHECK(!s.Assisted());
}

// Boss 1: Tornado -> Sun Strike cast at the ideal moment: PERFECT, 100 % in one combo.
static void TestBossPerfectCombo()
{
	BossSession s;
	s.Start(kClassic[0]);
	BossTotals t = {};
	BossKeys(s, "QWWR" "EEER");  // D = Sun Strike, F = Tornado
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::SunStrike && s.Invoker().GetSlot(Slot::F) == SkillId::Tornado);
	BossInputResult r = s.Input(InputAction::F);
	CHECK(r.attemptStarted && s.AttemptRunning() && s.CastSteps() == 1);
	CHECK(s.ProjectileCount() == 1);
	// the timing bar already runs while the Tornado flies (predicted landing) ...
	float predicted = 0.0f, span = 0.0f;
	CHECK(s.StepTiming(1, predicted, span) && span > 1.0f && predicted > 0.0f);
	float castAtPredicted = s.Elapsed() + predicted;
	CHECK(BossRunUntil(s, t, Lifted, 3.0f));
	CHECK(s.StepDone(0) && !s.StepDone(1));
	// ... and the real one after the hit agrees with the prediction
	float until = 0.0f;
	CHECK(s.StepTiming(1, until, span));
	CHECK(NearF(s.Elapsed() + until, castAtPredicted, 0.03f));
	CHECK(NearF(until, BOSS_LIFT_TIME + BOSS_IDEAL_AFTER_LANDING - BOSS_SUN_STRIKE_DELAY - s.AirTime(), 0.011f));
	CHECK(!s.CueNow());  // right after the lift, 1.7 s later the boss is still in the air
	CHECK(BossWaitIdeal(s, t, 1));
	CHECK(s.CueNow());
	float xAtCast = s.CenterX();
	s.Input(InputAction::D);
	CHECK(!s.StepTiming(1, until, span));  // cast: its bar is gone
	CHECK(s.PendingCount() == 1 && NearF(s.GetPending(0).x, xAtCast, 1e-3f));
	CHECK(BossRunUntil(s, t, ComboResolved, 4.0f));
	CHECK(t.landed && t.comboComplete && t.fail == ComboFail::None);
	CHECK(t.perfect == 1 && t.damage == 100);
	CHECK(t.won && s.State() == BossState::Won && s.BossHp() == 0);
}

// GREAT and GOOD take part of the boss's HP; the boss is pushed back after a combo.
static void TestBossPartialGrades()
{
	{
		BossSession s;
		s.Start(kClassic[0]);
		BossTotals t = {};
		BossKeys(s, "QWWR" "EEER" "F");
		CHECK(BossRunUntil(s, t, Air11, 3.0f));   // lands 1.1 + 1.7 - 2.5 = 0.3 s after: GREAT
		s.Input(InputAction::D);
		CHECK(BossRunUntil(s, t, ComboResolved, 4.0f));
		CHECK(t.great == 1 && t.damage == 60 && s.BossHp() == 40 && s.LastDamage() == 60);
		CHECK(s.Phase() == BossPhase::PushedBack);
		float xDone = s.X();
		BossWait(s, t, 1.0f);
		// pushed back 200 px, but never past BOSS_RESET_X (a boss already beyond it stays where it is)
		CHECK(s.Phase() == BossPhase::Walking && s.X() >= xDone - 50.0f);
		CHECK(s.X() <= (xDone > BOSS_RESET_X ? xDone : BOSS_RESET_X));
	}
	{
		BossSession s;
		s.Start(kClassic[0]);
		BossTotals t = {};
		BossKeys(s, "QWWR" "EEER" "F");
		CHECK(BossRunUntil(s, t, Air15, 3.0f));   // 0.7 s after the landing: GOOD
		s.Input(InputAction::D);
		CHECK(BossRunUntil(s, t, ComboResolved, 4.0f));
		CHECK(t.good == 1 && t.damage == 35 && s.BossHp() == 65);
	}
}

static void TestBossTiming()
{
	// too early: Sun Strike right after the lift lands while the boss is in the air
	{
		BossSession s;
		s.Start(kClassic[0]);
		BossTotals t = {};
		BossKeys(s, "QWWR" "EEER" "F");
		CHECK(BossRunUntil(s, t, Lifted, 3.0f));
		s.Input(InputAction::D);
		BossWait(s, t, 2.0f);
		CHECK(t.fail == ComboFail::TooEarly && s.LastFail() == ComboFail::TooEarly && t.missed == 1);
		CHECK(s.BossHp() == BOSS_FULL_HP && !s.AttemptRunning() && !t.comboComplete);
	}
	// too late: cast just before landing; 1.7 s later the 1.2 s window has closed
	{
		BossSession s;
		s.Start(kClassic[0]);
		BossTotals t = {};
		BossKeys(s, "QWWR" "EEER" "F");
		CHECK(BossRunUntil(s, t, Air24, 5.0f));
		s.Input(InputAction::D);
		BossWait(s, t, 2.0f);
		CHECK(t.landed && t.fail == ComboFail::TooLate && s.BossHp() == BOSS_FULL_HP);
	}
	// nothing cast after the Tornado: the window closes, TOO LATE
	{
		BossSession s;
		s.Start(kClassic[0]);
		BossTotals t = {};
		BossKeys(s, "QWWR" "EEER" "F");
		BossWait(s, t, 5.0f);
		CHECK(t.fail == ComboFail::TooLate && !s.AttemptRunning());
	}
	// a spell that lands before the attempt's Tornado has even hit is too early as well
	{
		BossSession s;
		s.Start(kClassic[0]);
		BossTotals t = {};
		BossKeys(s, "QWWR" "EEER" "FD");
		BossWait(s, t, 5.0f);
		CHECK(t.fail == ComboFail::TooEarly);
	}
	// the guided cue never shows on bosses 2 and 3
	{
		BossSession s;
		s.Start(kClassic[1]);
		BossTotals t = {};
		BossKeys(s, "WEER" "QWWR" "D");  // D = Tornado
		bool cue = false;
		for (int i = 0; i < 400; ++i) { BossAdd(t, s.Update(0.01f)); cue |= s.CueNow(); }
		CHECK(t.lifted && !cue);
	}
}

static void TestBossWrongSpell()
{
	BossSession s;
	s.Start(kClassic[0]);
	BossTotals t = {};
	// before any attempt: other spells do nothing, and are not failures
	BossKeys(s, "QQQR" "D", &t);  // Cold Snap
	CHECK(t.fail == ComboFail::None && !s.AttemptRunning());
	// a double tap of Tornado is ignored
	BossKeys(s, "QWWR" "D" "D", &t);
	CHECK(t.fail == ComboFail::None && s.AttemptRunning() && s.CastSteps() == 1);
	// Cold Snap (now in F) out of order: WRONG SPELL ends the attempt at once
	BossInputResult r = s.Input(InputAction::F);
	CHECK(r.fail == ComboFail::WrongSpell && !s.AttemptRunning() && s.LastFail() == ComboFail::WrongSpell);
	// the failed attempt's Tornado still lifts the boss, but nothing counts
	CHECK(BossRunUntil(s, t, Lifted, 3.0f));
	CHECK(!s.StepDone(0));
	// a Tornado cast while the boss is in the air starts nothing and passes through
	r = s.Input(InputAction::D);
	CHECK(!r.attemptStarted && !s.AttemptRunning());
	float air = s.AirTime();
	BossWait(s, t, 0.5f);
	CHECK(s.Phase() == BossPhase::Airborne && NearF(s.AirTime(), air + 0.5f, 0.02f));
}

// Boss 3: Tornado -> EMP -> Chaos Meteor -> Deafening Blast, each cast when its timing bar runs out.
static void TestBossFourSpellCombo()
{
	BossSession s;
	s.Start(kClassic[2]);
	BossTotals t = {};
	BossKeys(s, "QWWR" "WWWR", &t);  // D = EMP, F = Tornado
	s.Input(InputAction::F);
	CHECK(BossWaitIdeal(s, t, 1));   // EMP (2.9 s) is cast while the Tornado is still flying
	CHECK(!t.lifted);
	BossKeys(s, "D", &t);
	BossKeys(s, "WEER", &t);         // D = Chaos Meteor, F = EMP
	CHECK(BossWaitIdeal(s, t, 2));
	BossKeys(s, "D", &t);
	BossKeys(s, "QWER", &t);         // D = Deafening Blast
	CHECK(BossWaitIdeal(s, t, 3));
	BossKeys(s, "D", &t);
	CHECK(t.fail == ComboFail::None && s.CastSteps() == 4);
	CHECK(BossRunUntil(s, t, ComboResolved, 4.0f));
	CHECK(t.comboComplete && t.perfect == 3 && t.damage == 100 && t.won);

	// one spell too early only loses its own share: EMP and Blast perfect, Meteor in the air -> (100+0+100)/3
	BossSession p;
	p.Start(kClassic[2]);
	BossTotals tp = {};
	BossKeys(p, "QWWR" "WWWR", &tp);
	p.Input(InputAction::F);
	CHECK(BossWaitIdeal(p, tp, 1));
	BossKeys(p, "D" "WEER" "D", &tp);  // Meteor at once: it lands long before the boss comes down
	BossKeys(p, "QWER", &tp);
	CHECK(p.AttemptRunning());
	CHECK(BossWaitIdeal(p, tp, 3));
	BossKeys(p, "D", &tp);
	CHECK(BossRunUntil(p, tp, ComboResolved, 4.0f));
	CHECK(tp.comboComplete && tp.missed == 1 && tp.perfect == 2 && tp.damage == 67 && p.BossHp() == 33);

	// the same spells in the wrong order fail at once
	BossSession w;
	w.Start(kClassic[2]);
	BossTotals tw = {};
	BossKeys(w, "QWWR" "WEER" "F" "D", &tw);  // Tornado, then Meteor before EMP
	CHECK(tw.fail == ComboFail::WrongSpell);
}

// Three GOOD combos (35 % each) beat boss 1.
static void TestBossWin()
{
	BossSession s;
	s.Start(kClassic[0]);
	BossTotals t = {};
	BossKeys(s, "QWWR" "EEER");  // D = Sun Strike, F = Tornado for the whole fight
	for (int combo = 0; combo < 3; ++combo)
	{
		CHECK(BossRunUntil(s, t, ReadyForTornado, 3.0f));
		s.Input(InputAction::F);
		CHECK(BossRunUntil(s, t, Air15, 3.0f));
		s.Input(InputAction::D);
		t.comboComplete = false;
		CHECK(BossRunUntil(s, t, ComboOrEnd, 4.0f));
	}
	CHECK(t.good == 3 && t.damage == 105);
	CHECK(t.won && s.State() == BossState::Won && s.BossHp() == 0);
	CHECK(s.PlayerHp() == START_HP && t.fail == ComboFail::None);
	float time = s.Elapsed();
	CHECK(time > 10.0f && time < 25.0f);
	CHECK(!s.Input(InputAction::Q).accepted);
	s.Update(1.0f);
	CHECK(s.Elapsed() == time);  // the clock stops with the fight
}

static void TestBossLose()
{
	BossSession s;
	s.Start(kClassic[0]);
	BossTotals t = {};
	float speed = kClassic[0].speed;
	CHECK(BossRunUntil(s, t, PlayerWasHit, (BOSS_START_X - HIT_LINE_X) / speed + 1.0f));
	CHECK(s.PlayerHp() == START_HP - 1 && s.Phase() == BossPhase::PushedBack);
	BossWait(s, t, 1.5f);
	float slide = (BOSS_RESET_X - HIT_LINE_X) / BOSS_PUSH_SPEED;
	CHECK(NearF(s.X(), BOSS_RESET_X - speed * (1.5f - slide), 3.0f));
	CHECK(BossRunUntil(s, t, FightLost, 60.0f));
	CHECK(t.lost && s.PlayerHp() == 0 && s.BossHp() == BOSS_FULL_HP);
	// a running attempt ends (no damage) when the boss reaches the player
	BossSession a;
	a.Start(kClassic[0]);
	BossTotals ta = {};
	BossKeys(a, "QWWR" "EEER");
	CHECK(BossRunUntil(a, ta, NearPlayer, 30.0f));
	a.Input(InputAction::F);                 // lifted almost at once, lands a step from the player
	CHECK(a.AttemptRunning());
	CHECK(BossRunUntil(a, ta, PlayerWasHit, BOSS_LIFT_TIME + 1.0f));
	CHECK(ta.landed && !a.AttemptRunning() && a.PlayerHp() == START_HP - 1);
}


// ---------------------------------------------------------------- Boss mode, update 1.5 (spec §25 B-16..B-20)

static void TestBossAllSkills()
{
	bool used[invoker::SKILL_COUNT] = {};
	for (int i = 0; i < 8; ++i)
	{
		const BossDefinition& d = kClassic[i];
		for (int k = 0; k < d.comboLength; ++k)
			used[static_cast<int>(d.combo[k])] = true;
		for (int k = 0; k < d.combo2Length; ++k)
			used[static_cast<int>(d.combo2[k])] = true;
		CHECK(d.combo2Length == 0 || i == 7);  // only the last one has two phases
	}
	for (int s = 0; s < invoker::SKILL_COUNT; ++s)
		CHECK(used[s]);  // every one of the ten skills is needed by one of them
	CHECK(kClassic[7].combo2Length == 4);
	// B-16: quick grades
	CHECK(GradeQuick(0.0f) == HitGrade::Perfect && GradeQuick(BOSS_QUICK_PERFECT) == HitGrade::Perfect);
	CHECK(GradeQuick(BOSS_QUICK_PERFECT + 0.01f) == HitGrade::Great && GradeQuick(BOSS_QUICK_GREAT) == HitGrade::Great);
	CHECK(GradeQuick(BOSS_QUICK_GREAT + 0.01f) == HitGrade::Good && GradeQuick(BOSS_QUICK_LIMIT) == HitGrade::Good);
	CHECK(GradeQuick(BOSS_QUICK_LIMIT + 0.01f) == HitGrade::Miss);
	// step kinds: landing only after a Tornado
	BossSession s;
	s.Start(kClassic[3]);  // Cold Snap -> Sun Strike
	CHECK(s.StepKind(0) == BossStepKind::Opener && s.StepKind(1) == BossStepKind::Quick);
	s.Start(kClassic[6]);  // Ghost Walk -> Tornado -> Sun Strike -> Chaos Meteor -> Deafening Blast
	CHECK(s.StepKind(1) == BossStepKind::Quick && s.StepKind(2) == BossStepKind::Landing && s.StepKind(4) == BossStepKind::Landing);
	s.Start(kClassic[0]);
	CHECK(s.StepKind(1) == BossStepKind::Landing);
}

// Frost Troll: Cold Snap freezes the boss, Sun Strike right after it is a PERFECT quick step.
static void TestBossQuickCombo()
{
	BossSession s;
	s.Start(kClassic[3]);
	BossTotals t = {};
	BossKeys(s, "QQQR" "EEER");  // D = Sun Strike, F = Cold Snap
	BossWait(s, t, 1.0f);
	float x = s.X();
	BossInputResult r = s.Input(InputAction::F);
	CHECK(r.attemptStarted && s.AttemptRunning() && s.StepDone(0) && s.FreezeLeft() == BOSS_FREEZE_TIME);
	float left = 0.0f, total = 0.0f;
	CHECK(s.QuickTiming(1, left, total) && total == BOSS_QUICK_LIMIT && NearF(left, BOSS_QUICK_LIMIT, 1e-3f));
	BossWait(s, t, 0.5f);
	CHECK(s.X() == x);  // frozen: it does not move
	CHECK(s.QuickTiming(1, left, total) && NearF(left, BOSS_QUICK_LIMIT - 0.5f, 0.02f));
	r = s.Input(InputAction::D);
	CHECK(r.grade == HitGrade::Perfect && !s.QuickTiming(1, left, total));
	CHECK(BossRunUntil(s, t, ComboResolved, 1.0f));
	CHECK(t.comboComplete && t.damage == 100 && t.won);

	// too slow: the quick step runs out (TOO LATE), nothing else to score -> the attempt fails
	BossSession late;
	late.Start(kClassic[3]);
	BossTotals tl = {};
	BossKeys(late, "QQQR" "EEER" "F");
	BossWait(late, tl, BOSS_QUICK_LIMIT + 0.2f);
	CHECK(tl.fail == ComboFail::TooLate && !late.AttemptRunning() && late.BossHp() == BOSS_FULL_HP);

	// outside a combo Cold Snap does nothing to the boss (Stone Knight needs Tornado)
	BossSession other;
	other.Start(kClassic[0]);
	BossKeys(other, "QQQR" "D");
	CHECK(other.FreezeLeft() == 0.0f && !other.AttemptRunning());
}

// Fire Imp: Cold Snap -> Alacrity (after 1.5 s: GREAT) -> Forge Spirit (at once: PERFECT) = 80 %.
static void TestBossQuickGrades()
{
	BossSession s;
	s.Start(kClassic[5]);
	BossTotals t = {};
	BossKeys(s, "WWER" "QQQR");  // D = Cold Snap, F = Alacrity
	s.Input(InputAction::D);
	BossWait(s, t, 1.5f);
	CHECK(s.Input(InputAction::F).grade == HitGrade::Great);
	BossKeys(s, "QEER");         // D = Forge Spirit
	CHECK(s.Input(InputAction::D).grade == HitGrade::Perfect);
	CHECK(BossRunUntil(s, t, ComboResolved, 1.0f));
	CHECK(t.comboComplete && t.damage == 80 && s.BossHp() == 20);

	// Glacier Golem: Ice Wall slows the boss; a missed quick step only loses its share
	BossSession g;
	g.Start(kClassic[4]);
	BossTotals tg = {};
	BossKeys(g, "WEER" "QQER");  // D = Ice Wall, F = Chaos Meteor
	float x0 = g.X();
	BossWait(g, tg, 0.5f);
	float normal = x0 - g.X();
	g.Input(InputAction::D);
	CHECK(g.SlowLeft() == BOSS_ICE_WALL_TIME);
	x0 = g.X();
	BossWait(g, tg, 0.5f);
	CHECK(NearF(x0 - g.X(), normal * BOSS_ICE_WALL_SPEED, 0.5f));
	BossWait(g, tg, BOSS_QUICK_LIMIT);   // Chaos Meteor never comes: TOO LATE, the combo goes on
	CHECK(tg.fail == ComboFail::None && g.AttemptRunning() && g.CastSteps() == 2);
	BossKeys(g, "QWER");                 // D = Deafening Blast
	CHECK(g.Input(InputAction::D).grade == HitGrade::Perfect);
	CHECK(BossRunUntil(g, tg, ComboResolved, 1.0f));
	CHECK(tg.comboComplete && tg.damage == 50);  // (0 + 100) / 2
}

// Shadow Assassin: Ghost Walk -> Tornado (quick) -> Sun Strike -> Chaos Meteor -> Deafening Blast (landing steps).
static void TestBossFiveSpellCombo()
{
	BossSession s;
	s.Start(kClassic[6]);
	BossTotals t = {};
	BossKeys(s, "QQWR" "QWWR");  // D = Tornado, F = Ghost Walk
	BossWait(s, t, 0.5f);
	float x = s.X();
	s.Input(InputAction::F);
	CHECK(s.ConfuseLeft() == BOSS_CONFUSE_TIME);
	BossWait(s, t, 0.3f);
	CHECK(s.X() == x);           // it lost the player and stands still
	CHECK(s.Input(InputAction::D).grade == HitGrade::Perfect);  // Tornado, quick
	BossKeys(s, "EEER");
	CHECK(BossWaitIdeal(s, t, 2));
	BossKeys(s, "D", &t);        // Sun Strike
	BossKeys(s, "WEER");
	CHECK(BossWaitIdeal(s, t, 3));
	BossKeys(s, "D", &t);        // Chaos Meteor
	BossKeys(s, "QWER");
	CHECK(BossWaitIdeal(s, t, 4));
	BossKeys(s, "D", &t);        // Deafening Blast
	CHECK(t.fail == ComboFail::None && s.CastSteps() == 5);
	CHECK(BossRunUntil(s, t, ComboResolved, 4.0f));
	CHECK(t.comboComplete && t.perfect == 3 && t.damage == 100 && t.won);
}

// Archon: at 50 % or less the combo changes to the second phase for good.
static void TestBossPhases()
{
	BossSession s;
	s.Start(kClassic[7]);
	BossTotals t = {};
	CHECK(s.ComboPhase() == 0 && s.ComboLength() == 5 && s.Combo()[0] == SkillId::Tornado);
	// two attempts with only EMP landed: (100 + 0 + 0 + 0) / 4 = 25 % each
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		BossKeys(s, "QWWR" "WWWR");  // D = EMP, F = Tornado
		CHECK(BossRunUntil(s, t, ReadyForTornado, 6.0f));
		s.Input(InputAction::F);
		CHECK(BossWaitIdeal(s, t, 1));
		BossKeys(s, "D", &t);
		t.comboComplete = false;
		CHECK(BossRunUntil(s, t, ComboOrEnd, 6.0f));
	}
	CHECK(s.BossHp() == 50 && s.ComboPhase() == 1 && t.damage == 50);
	CHECK(s.ComboLength() == 4 && s.Combo()[0] == SkillId::IceWall && s.Combo()[3] == SkillId::Alacrity);
	CHECK(s.StepKind(1) == BossStepKind::Quick && s.StepKind(3) == BossStepKind::Quick);
	// phase 2, all quick and all at once: Ice Wall -> Cold Snap -> Forge Spirit -> Alacrity
	CHECK(BossRunUntil(s, t, ReadyForTornado, 6.0f));
	BossKeys(s, "QQER" "D", &t);
	BossKeys(s, "QQQR" "D", &t);
	BossKeys(s, "QEER" "D", &t);
	BossKeys(s, "WWER" "D", &t);
	CHECK(BossRunUntil(s, t, [](const BossSession& b, const BossTotals&) { return b.State() != BossState::Fighting; }, 2.0f));
	CHECK(s.State() == BossState::Won && t.fail == ComboFail::None);
}

// ---------------------------------------------------------------- PLAY mode (spec §26)

// Invokes `skill` into D and casts it; a Tornado is followed until its projectile is judged.
static CastOutcome PlayCast(PracticeSession& s, SkillId skill, KillReport* kill = NULL)
{
	std::string letters = invoker::RecipeLetters(skill);
	PressLetters(s, letters.c_str());
	PressLetters(s, "R");
	InputResult r = s.Input(InputAction::D);
	if (kill != NULL)
		*kill = r.kill;
	if (skill != SkillId::Tornado)
		return r.cast;
	for (int i = 0; i < 300; ++i)
	{
		UpdateResult u = s.Update(0.01f);
		if (u.cast != CastOutcome::None)
		{
			if (kill != NULL)
				*kill = u.kill;
			return u.cast;
		}
	}
	return CastOutcome::None;
}

static KillReport ImmortalWin(PracticeSession& s);

// Waits for the next enemy. An Immortal that comes in between (§32 M-3: by chance) is beaten on the way.
static void PlayWaitSpawn(PracticeSession& s)
{
	for (int i = 0; i < 400 && !s.Enemy().active && s.State() == GameState::Playing; ++i)
	{
		if (s.ImmortalActive())
			ImmortalWin(s);
		else if (s.RuneChoiceCount() > 0)
			s.ChooseRune(0);
		else
			s.Update(0.05f);
	}
}

// Kills the active enemy by casting its whole chain; returns the report of the finishing cast.
static KillReport PlayKill(PracticeSession& s)
{
	KillReport kill = {};
	for (int guard = 0; guard < PLAY_MAX_CHAIN && s.Enemy().active; ++guard)
		PlayCast(s, s.Enemy().target, &kill);
	return kill;
}

static SkillId WrongSkillFor(SkillId target)
{
	for (int i = 0; i < invoker::SKILL_COUNT; ++i)
	{
		SkillId s = static_cast<SkillId>(i);
		if (s != target && s != SkillId::Tornado)
			return s;
	}
	return SkillId::None;
}

static void TestPlayTables()
{
	// §32 M-2: the first 14 are normal; 15, 25, 35 ... elites; 20, 30, 40 ... overlords
	for (int n = 1; n <= 14; ++n)
		CHECK(PlayEnemyKind(n) == EnemyKind::Normal);
	for (int n = 15; n <= 200; ++n)
	{
		EnemyKind expected = n % 10 == 0 ? EnemyKind::Overlord : n % 10 == 5 ? EnemyKind::Elite : EnemyKind::Normal;
		CHECK(PlayEnemyKind(n) == expected);
	}
	CHECK(ImmortalTier(0) == 1 && ImmortalTier(1) == 2 && ImmortalTier(2) == 2 && ImmortalTier(3) == 3 && ImmortalTier(4) == 3);
	CHECK(ImmortalDropCount(0) == 1 && ImmortalDropCount(2) == 1 && ImmortalDropCount(3) == 2 && ImmortalDropCount(4) == 2);
	PlayRun list[TOP_RUNS] = {};
	CHECK(InsertPlayRun(list, { 0, 30.0f, 1 }) == 0);          // no score: not listed
	CHECK(InsertPlayRun(list, { 12, 60.0f, 2 }) == 1);
	CHECK(InsertPlayRun(list, { 20, 50.0f, 2 }) == 1);
	CHECK(InsertPlayRun(list, { 12, 70.0f, 3 }) == 2);         // same score, further stage ranks higher
	CHECK(InsertPlayRun(list, { 12, 80.0f, 2 }) == 3);         // same score and stage: the longer run
	CHECK(list[0].score == 20 && list[1].stage == 3 && list[2].time == 80.0f && list[3].time == 60.0f);
	for (int i = 0; i < TOP_RUNS; ++i)
		InsertPlayRun(list, { 100 + i, 10.0f, 1 });
	CHECK(InsertPlayRun(list, { 5, 10.0f, 1 }) == 0);          // below the whole list
}

static void TestPlaySurvivalUnchanged()
{
	PracticeSession s;
	CHECK(s.Mode() == SessionMode::Survival);
	s.Start(7);
	PlayWaitSpawn(s);
	CHECK(s.Enemy().kind == EnemyKind::Normal && s.Enemy().chainLength == 1
		&& s.Enemy().scale == GetEnemyDefinition(s.Enemy().definition).size);
	KillReport kill = PlayKill(s);
	CHECK(kill.killed && kill.points == 1 && kill.gold == 0 && kill.rune == Rune::None);
	CHECK(s.GetStats().score == 1 && s.GetStats().gold == 0 && s.GetStats().kills == 1);
	s.SetMode(SessionMode::Play);  // ignored while Playing
	CHECK(s.Mode() == SessionMode::Survival);
}

// §32 M-2: 14 normal enemies, the 15th an elite (chain of 2), the 20th an overlord (chain of 3), slower and larger.
static void TestPlayBossCadence()
{
	PracticeSession s;
	s.SetMode(SessionMode::Play);
	CHECK(s.Mode() == SessionMode::Play);
	s.Start(11);
	PlayWaitSpawn(s);
	for (int n = 1; n < PLAY_OVERLORD_FIRST; ++n)
	{
		const ActiveEnemy& e = s.Enemy();
		float size = GetEnemyDefinition(e.definition).size;
		CHECK(s.SpawnCount() == n && e.kind == (n == PLAY_ELITE_FIRST ? EnemyKind::Elite : EnemyKind::Normal));
		if (e.kind == EnemyKind::Elite)
		{
			CHECK(e.chainLength == 2 && e.chain[0] != e.chain[1]);
			CHECK(e.scale == size * PLAY_ELITE_SCALE);
		}
		else
			CHECK(e.chainLength == 1 && e.scale == size);
		PlayKill(s);
		PlayWaitSpawn(s);
	}
	const ActiveEnemy& boss = s.Enemy();
	CHECK(s.SpawnCount() == PLAY_OVERLORD_FIRST && boss.kind == EnemyKind::Overlord);
	CHECK(boss.chainLength == 3 && boss.chainStep == 0 && boss.target == boss.chain[0]);
	CHECK(boss.chain[0] != boss.chain[1] && boss.chain[0] != boss.chain[2] && boss.chain[1] != boss.chain[2]);
	CHECK(boss.scale == GetEnemyDefinition(boss.definition).size * PLAY_OVERLORD_SCALE);
	CHECK(boss.speed <= DifficultyAt(s.GetStats().survivalTime).enemySpeed * PLAY_OVERLORD_SPEED + 0.5f);
	// the enemies are drawn in whole sizes (M-8): none is tiny, and a normal one stays smaller than the player
	// (about 106 px), so that an elite, an overlord and an Immortal stand out (owner, 1.9.3)
	CHECK(PLAY_ELITE_SCALE == 1.4f && PLAY_OVERLORD_SCALE == 2.0f);
	for (int i = 0; i < ENEMY_TYPE_COUNT; ++i)
	{
		const EnemyDefinition& d = GetEnemyDefinition(i);
		float height = static_cast<float>(d.bodyBottom - d.bodyTop + 1) * d.size;
		CHECK(d.size == 1.0f || d.size == 2.0f || d.size == 3.0f);
		CHECK(height >= 60.0f && height <= 105.0f);
	}
}

// Seeds differ in whether an Immortal came before the first overlord (it leaves a rune and takes time); the tests
// that need a plain run up to the overlord take the first seed from `seed` on without one.
static void PlayToBossWith(PracticeSession& s, unsigned seed, const Inventory& inv);
static void PlayToBoss(PracticeSession& s, unsigned seed)
{
	PlayToBossWith(s, seed, EmptyInventory());
}

// A chain: right casts break it one skill at a time, a wrong cast keeps the progress, the last one kills.
static void TestPlayChain()
{
	PracticeSession s;
	PlayToBoss(s, 21);
	CHECK(s.Enemy().kind == EnemyKind::Overlord);
	int scoreBefore = s.GetStats().score;
	int correctBefore = s.GetStats().correctCasts;
	SkillId first = s.Enemy().chain[0], second = s.Enemy().chain[1], third = s.Enemy().chain[2];

	KillReport kill = {};
	CHECK(PlayCast(s, first, &kill) == CastOutcome::Correct);
	CHECK(!kill.killed && s.Enemy().active && s.Enemy().chainStep == 1 && s.Enemy().target == second);
	CHECK(s.GetStats().score == scoreBefore && s.GetStats().correctCasts == correctBefore + 1);

	CHECK(PlayCast(s, WrongSkillFor(second)) == CastOutcome::Incorrect);  // a MISS: progress kept
	CHECK(s.Enemy().active && s.Enemy().chainStep == 1 && s.Enemy().target == second);

	CHECK(PlayCast(s, second) == CastOutcome::Correct && s.Enemy().chainStep == 2 && s.Enemy().target == third);
	CHECK(PlayCast(s, third, &kill) == CastOutcome::Correct);
	CHECK(kill.killed && kill.kind == EnemyKind::Overlord && !s.Enemy().active);
	CHECK(kill.points == PLAY_POINTS_OVERLORD || kill.points == 2 * PLAY_POINTS_OVERLORD);
	CHECK(kill.rune != Rune::None && kill.gold >= PLAY_GOLD_OVERLORD);
	CHECK(s.GetStats().bossesDefeated == 1 && s.GetStats().gold >= PLAY_GOLD_OVERLORD);
	CHECK(s.GetStats().score == scoreBefore + kill.points);
	// §32 M-3: the elite before it added 10 %, the overlord adds 20 %, and the roll is reported
	CHECK(kill.immortalChance == PLAY_IMMORTAL_CHANCE_ELITE + PLAY_IMMORTAL_CHANCE_OVERLORD);
	CHECK(s.ImmortalChance() == kill.immortalChance && s.ImmortalDue() == kill.immortalComing);
}

// A boss that reaches the player costs 3 lives: from 3, Game Over.
static void TestPlayLeakDamage()
{
	PracticeSession s;
	PlayToBoss(s, 31);
	CHECK(s.GetStats().hp == START_HP);
	UpdateResult leak = {};
	for (int i = 0; i < 3000 && !leak.leaked; ++i)
		leak = s.Update(0.01f);
	CHECK(leak.leaked && leak.leakDamage == PLAY_LEAK_OVERLORD && leak.gameOver);
	CHECK(s.GetStats().hp == 0 && s.State() == GameState::GameOver);

	// a Shield takes the leak instead
	PracticeSession h;
	PlayToBoss(h, 31);
	h.ApplyRune(Rune::Shield);
	CHECK(h.HasShield());
	leak = {};
	for (int i = 0; i < 3000 && !leak.leaked; ++i)
		leak = h.Update(0.01f);
	CHECK(leak.leaked && leak.shieldUsed && leak.leakDamage == 0 && !leak.gameOver);
	CHECK(h.GetStats().hp == START_HP && !h.HasShield());
}

static void TestPlayRunes()
{
	PracticeSession s;
	s.SetMode(SessionMode::Play);
	s.Start(41);
	// Regeneration: +1 life up to 5, and the HP row grows with it
	for (int i = 0; i < 4; ++i)
		s.ApplyRune(Rune::Regeneration);
	CHECK(s.GetStats().hp == PLAY_MAX_HP && s.GetStats().maxHp == PLAY_MAX_HP);
	// Bounty: gold
	s.ApplyRune(Rune::Bounty);
	CHECK(s.GetStats().gold == PLAY_BOUNTY_GOLD);
	// Frost: enemies at 60 % while it lasts
	PlayWaitSpawn(s);
	float x0 = s.Enemy().x;
	s.Update(0.05f);
	float normal = x0 - s.Enemy().x;
	s.ApplyRune(Rune::Frost);
	x0 = s.Enemy().x;
	s.Update(0.05f);
	CHECK(NearF(x0 - s.Enemy().x, normal * PLAY_FROST_SPEED, 0.05f));
	CHECK(NearF(s.FrostLeft(), PLAY_FROST_TIME - 0.05f, 1e-3f));
	// Double Damage: the next kill scores twice
	s.ApplyRune(Rune::DoubleDamage);
	KillReport kill = PlayKill(s);
	CHECK(kill.killed && kill.points == 2 * PLAY_POINTS_NORMAL);
	// the timers run out
	for (int i = 0; i < 300; ++i)
		s.Update(0.1f);
	CHECK(s.FrostLeft() == 0.0f && s.DoubleLeft() == 0.0f);
	// a new session clears them
	s.ApplyRune(Rune::Shield);
	s.Start(42);
	CHECK(!s.HasShield() && s.GetStats().gold == 0 && s.GetStats().hp == START_HP);
}


// ---------------------------------------------------------------- shop and items (spec §27)

static Inventory LoadoutWith(ItemId a, int levelA, ItemId b = ItemId::Salve, int countB = 0)
{
	Inventory inv = EmptyInventory();
	int gold = 1000000;
	for (int m = 0; m < MATERIAL_COUNT; ++m)  // the upgrades need materials since 1.6 (§28 O-11)
		inv.material[m] = 9;
	for (int i = 0; i < levelA; ++i)
		Buy(inv, a, gold);
	for (int i = 0; i < countB; ++i)
		Buy(inv, b, gold);
	return inv;
}

static void PlayStartWith(PracticeSession& s, unsigned seed, const Inventory& inv)
{
	s.SetMode(SessionMode::Play);
	s.SetLoadout(inv);
	s.Start(seed);
	PlayWaitSpawn(s);
}

static void PlayToBossWith(PracticeSession& s, unsigned seed, const Inventory& inv)
{
	for (unsigned attempt = 0; attempt < 50; ++attempt)
	{
		PlayStartWith(s, seed + attempt * 1000u, inv);
		while (s.Enemy().kind != EnemyKind::Overlord && s.State() == GameState::Playing)
		{
			PlayKill(s);
			PlayWaitSpawn(s);
		}
		if (s.GetStats().immortalsBeaten == 0 && s.Enemy().kind == EnemyKind::Overlord)
			return;
	}
}

static void TestItemShop()
{
	CHECK(GetItemDefinition(ItemId::Blink).levels == 2 && GetItemDefinition(ItemId::Salve).levels == 1);
	for (int i = 0; i < ITEM_COUNT; ++i)
	{
		const ItemDefinition& d = GetItemDefinition(i);
		CHECK(static_cast<int>(d.id) == i && d.level[0].price > 0);
		if (d.levels == 2)
			CHECK(d.level[1].price > d.level[0].price);  // the upgrade costs more (I-2)
		CHECK((d.kind == ItemKind::Active) == (d.level[0].cooldown > 0.0f));
	}
	Inventory inv = EmptyInventory();
	int gold = 1000;
	CHECK(NextPrice(inv, ItemId::Blink) == 1500 && !Buy(inv, ItemId::Blink, gold) && gold == 1000);
	gold = 2000;
	CHECK(Buy(inv, ItemId::Blink, gold) && gold == 500 && inv.level[static_cast<int>(ItemId::Blink)] == 1);
	CHECK(SlotOf(inv, ItemId::Blink) == 0);  // a new item goes into the first free slot
	CHECK(NextPrice(inv, ItemId::Blink) == 4000);
	gold = 4000;
	AddMaterial(inv, Material::PointBooster);  // the upgrade needs one (tested in TestMaterialShop)
	CHECK(Buy(inv, ItemId::Blink, gold) && gold == 0 && CurrentLevel(inv, ItemId::Blink) == 2);
	gold = 100000;
	CHECK(NextPrice(inv, ItemId::Blink) == 0 && !Buy(inv, ItemId::Blink, gold) && gold == 100000);  // maxed
	for (int i = 0; i < ITEM_MAX_STACK; ++i)
		CHECK(Buy(inv, ItemId::Salve, gold));
	CHECK(inv.count[static_cast<int>(ItemId::Salve)] == ITEM_MAX_STACK && NextPrice(inv, ItemId::Salve) == 0);
	CHECK(!Buy(inv, ItemId::Salve, gold));
	CHECK(SlotOf(inv, ItemId::Salve) == 1);
	// six slots at most; unequip frees one; passives only count while equipped
	Buy(inv, ItemId::Euls, gold); Buy(inv, ItemId::Bkb, gold); Buy(inv, ItemId::Midas, gold); Buy(inv, ItemId::Octarine, gold);
	AddMaterial(inv, Material::PointBooster);
	CHECK(Buy(inv, ItemId::Aghanim, gold) && !IsEquipped(inv, ItemId::Aghanim) && EquippedLevel(inv, ItemId::Aghanim) == 0);
	Unequip(inv, ItemId::Salve);
	CHECK(SlotOf(inv, ItemId::Salve) == ITEM_NONE && Equip(inv, ItemId::Aghanim) && SlotOf(inv, ItemId::Aghanim) == 1);
	CHECK(EquippedLevel(inv, ItemId::Aghanim) == 1 && EquippedLevel(inv, ItemId::Midas) == 1);
	CHECK(!Equip(inv, ItemId::Cheese));  // not owned
}

static void TestItemUse()
{
	// only in PLAY
	PracticeSession sv;
	sv.SetLoadout(LoadoutWith(ItemId::Blink, 1));
	sv.Start(3);
	PlayWaitSpawn(sv);
	CHECK(!sv.UseItem(0).used);

	// Blink: the enemy walks back; the cooldown runs; a second use waits for it
	PracticeSession s;
	PlayStartWith(s, 5, LoadoutWith(ItemId::Blink, 1));
	CHECK(!s.UseItem(1).used && !s.UseItem(-1).used);  // an empty slot / no slot
	float x0 = s.Enemy().x;
	s.Update(0.2f);
	CHECK(s.Enemy().x < x0);
	CHECK(s.UseItem(0).used && s.ItemCooldown(ItemId::Blink) == 40.0f);
	x0 = s.Enemy().x;
	s.Update(0.1f); s.Update(0.1f);
	CHECK(s.Enemy().x > x0 && s.Enemy().x <= SPAWN_X);
	CHECK(!s.UseItem(0).used && NearF(s.ItemCooldown(ItemId::Blink), 39.8f, 1e-3f));

	// Eul's: still; Wind Waker pushes back 150 px
	PracticeSession e;
	PlayStartWith(e, 5, LoadoutWith(ItemId::Euls, 1));
	e.Update(0.3f); e.Update(0.3f); e.Update(0.3f);
	x0 = e.Enemy().x;
	CHECK(e.UseItem(0).used);
	e.Update(0.1f); e.Update(0.1f);
	CHECK(e.Enemy().x == x0);
	PracticeSession w;
	PlayStartWith(w, 5, LoadoutWith(ItemId::Euls, 2));
	for (int i = 0; i < 30; ++i)  // far enough from the spawn point for the whole push
		w.Update(0.1f);
	x0 = w.Enemy().x;
	CHECK(w.UseItem(0).used && NearF(w.Enemy().x, x0 + PLAY_EULS_PUSHBACK, 1e-3f) && w.ItemCooldown(ItemId::Euls) == 20.0f);

	// Octarine cuts the cooldown by 25 %
	Inventory oct = LoadoutWith(ItemId::Blink, 1);
	int gold = 100000;
	Buy(oct, ItemId::Octarine, gold);
	PracticeSession o;
	PlayStartWith(o, 5, oct);
	CHECK(o.UseItem(0).used && NearF(o.ItemCooldown(ItemId::Blink), 30.0f, 1e-3f));
	CHECK(!o.UseItem(1).used);  // a passive in a slot does nothing when pressed

	// Salve / Cheese: lives up to 5, units used up; nothing is spent at 5
	PracticeSession h;
	PlayStartWith(h, 5, LoadoutWith(ItemId::Cheese, 2, ItemId::Salve, 2));
	CHECK(h.Loadout().slot[0] == static_cast<int>(ItemId::Cheese));
	CHECK(h.UseItem(0).used && h.GetStats().hp == PLAY_MAX_HP && h.Loadout().count[static_cast<int>(ItemId::Cheese)] == 1);
	CHECK(!h.UseItem(0).used && h.Loadout().count[static_cast<int>(ItemId::Cheese)] == 1);  // 5 lives: nothing spent
	CHECK(!h.UseItem(1).used && h.Loadout().count[static_cast<int>(ItemId::Salve)] == 2);

	// Smoke: half speed
	PracticeSession m;
	PlayStartWith(m, 5, LoadoutWith(ItemId::Smoke, 1, ItemId::Smoke, 0));
	x0 = m.Enemy().x;
	m.Update(0.05f);
	float normal = x0 - m.Enemy().x;
	CHECK(m.UseItem(0).used && m.SmokeLeft() == 8.0f);
	x0 = m.Enemy().x;
	m.Update(0.05f);
	CHECK(NearF(x0 - m.Enemy().x, normal * PLAY_SMOKE_SPEED, 0.05f));
}

static void TestItemBossItems()
{
	// Refresher: a boss chain of 3 goes to its last skill; not usable on a single skill
	PracticeSession r;
	PlayStartWith(r, 21, LoadoutWith(ItemId::Refresher, 1));
	CHECK(!r.UseItem(0).used);  // the first enemy needs one skill
	PlayToBossWith(r, 21, LoadoutWith(ItemId::Refresher, 1));
	CHECK(r.Enemy().kind == EnemyKind::Overlord);
	CHECK(r.UseItem(0).used && r.Enemy().chainStep == 2 && r.Enemy().target == r.Enemy().chain[2]);
	KillReport kill = PlayKill(r);
	CHECK(kill.killed && kill.kind == EnemyKind::Overlord);

	// Black King Bar: a boss reaching the player costs nothing while it lasts
	PracticeSession b;
	PlayToBossWith(b, 31, LoadoutWith(ItemId::Bkb, 1));
	float x = b.Enemy().x;
	UpdateResult leak = {};
	// wait until the boss is about 2 s from the player, then BKB (5 s)
	while (!leak.leaked && (b.Enemy().x - HIT_LINE_X) / b.Enemy().speed > 2.0f)
		leak = b.Update(0.01f);
	CHECK(b.UseItem(0).used && b.BkbLeft() == 5.0f);
	for (int i = 0; i < 500 && !leak.leaked; ++i)
		leak = b.Update(0.01f);
	CHECK(x > HIT_LINE_X && leak.leaked && leak.shieldUsed && leak.leakDamage == 0 && b.GetStats().hp == START_HP);

	// Hand of Midas: +50 % gold (a boss gives 20 -> 30)
	PracticeSession g;
	PlayToBossWith(g, 21, LoadoutWith(ItemId::Midas, 1));
	int before = g.GetStats().gold;
	kill = PlayKill(g);
	CHECK(kill.killed);
	CHECK(kill.gold >= 30 && g.GetStats().gold - before == kill.gold);

	// Aghanim's Scepter: the run waits for a rune choice (1 of 2)
	PracticeSession a;
	PlayToBossWith(a, 21, LoadoutWith(ItemId::Aghanim, 1));
	kill = PlayKill(a);
	CHECK(kill.killed && kill.runeChoice && kill.rune == Rune::None);
	CHECK(a.RuneChoiceCount() == 2 && a.RuneChoice(0) != a.RuneChoice(1));
	float t = a.GetStats().survivalTime;
	a.Update(0.1f);
	CHECK(a.GetStats().survivalTime == t && !a.Input(InputAction::Q).accepted && !a.UseItem(0).used);
	Rune chosen = a.RuneChoice(1);
	CHECK(a.ChooseRune(1) == chosen && a.RuneChoiceCount() == 0 && a.ChooseRune(0) == Rune::None);
	a.Update(0.1f);
	CHECK(a.GetStats().survivalTime > t);
	// Aghanim's Blessing: 1 of 3
	PracticeSession ab;
	PlayToBossWith(ab, 21, LoadoutWith(ItemId::Aghanim, 2));
	PlayKill(ab);
	CHECK(ab.RuneChoiceCount() == 3);
}


// ---------------------------------------------------------------- IMMORTAL and materials (spec §28, §32)

// Plays on (every enemy killed at once, the first rune taken) until the IMMORTAL warning starts; returns that update.
static UpdateResult PlayOnToImmortal(PracticeSession& s)
{
	for (int guard = 0; guard < 200000 && s.State() == GameState::Playing; ++guard)
	{
		if (s.RuneChoiceCount() > 0)
			s.ChooseRune(0);
		else if (s.Enemy().active)
			PlayKill(s);
		else
		{
			UpdateResult u = s.Update(0.05f);
			if (u.immortalWarning)
				return u;
		}
	}
	return {};
}

static UpdateResult PlayToImmortal(PracticeSession& s, unsigned seed, const Inventory& inv = EmptyInventory())
{
	s.SetMode(SessionMode::Play);
	s.SetLoadout(inv);
	s.Start(seed);
	return PlayOnToImmortal(s);
}

// The same, with the first seed from `seed` on that reaches the Immortal without a Shield or a Frost rune running
// (the overlords on the way leave runes at random, and those two change what the fight tests measure).
static UpdateResult PlayToImmortalClean(PracticeSession& s, unsigned seed, const Inventory& inv = EmptyInventory())
{
	UpdateResult warn = {};
	for (unsigned attempt = 0; attempt < 50; ++attempt)
	{
		warn = PlayToImmortal(s, seed + attempt * 1000u, inv);
		if (warn.immortalWarning && !s.HasShield() && s.FrostLeft() == 0.0f)
			break;
	}
	return warn;
}

static bool ImmortalWaitFight(PracticeSession& s)  // the warning
{
	for (int i = 0; i < 600 && !s.ImmortalActive() && s.State() == GameState::Playing; ++i)
		s.Update(0.01f);
	return s.ImmortalActive();
}

// Plays the immortal's combo once, every landing step at its ideal moment and every quick step at once.
// Returns the update in which the combo resolved (or the fight ended).
static UpdateResult ImmortalCombo(PracticeSession& s)
{
	UpdateResult u = {};
	const BossSession& b = s.Immortal();
	for (int i = 0; i < 1000 && s.ImmortalActive() && (b.Phase() != BossPhase::Walking || b.AttemptRunning()); ++i)
		u = s.Update(0.01f);
	int length = b.ComboLength();
	for (int step = 0; step < length && s.ImmortalActive(); ++step)
	{
		InvokeSkill(s, b.Combo()[step]);  // into D
		if (b.StepKind(step) == BossStepKind::Landing)
		{
			for (int i = 0; i < 800; ++i)
			{
				float until = 0.0f, span = 0.0f;
				if (b.StepTiming(step, until, span) && until <= 0.005f)
					break;
				s.Update(0.01f);
			}
		}
		s.Input(InputAction::D);
	}
	for (int i = 0; i < 800 && s.ImmortalActive(); ++i)
	{
		u = s.Update(0.01f);
		if (u.boss.comboComplete || u.boss.fail != ComboFail::None)
			break;
	}
	return u;
}

static KillReport ImmortalWin(PracticeSession& s)
{
	for (int combo = 0; combo < 6 && s.ImmortalActive(); ++combo)
	{
		UpdateResult u = ImmortalCombo(s);
		if (u.kill.killed)
			return u.kill;
	}
	return {};
}

static void TestImmortalTables()
{
	// M-7: the first three drop their own material
	CHECK(MaterialOfBoss(0) == Material::PointBooster && MaterialOfBoss(1) == Material::MysticStaff
		&& MaterialOfBoss(2) == Material::SacredRelic);
	CHECK(std::strcmp(MaterialName(Material::PointBooster), "POINT BOOSTER") == 0
		&& std::strcmp(MaterialName(Material::MysticStaff), "MYSTIC STAFF") == 0
		&& std::strcmp(MaterialName(Material::SacredRelic), "SACRED RELIC") == 0);
	CHECK(PLAY_IMMORTAL_CHANCE_ELITE == 10 && PLAY_IMMORTAL_CHANCE_OVERLORD == 20 && PLAY_IMMORTAL_WARNING == 4.0f);
	// a scaled immortal: faster, a shorter window that never drops below BOSS_MIN_WINDOW; lives and slots come in
	invoker::InvokerState inv;
	inv.Apply(InputAction::E); inv.Apply(InputAction::E); inv.Apply(InputAction::E); inv.Apply(InputAction::R);
	BossSession b;
	b.StartImmortal(1, 5, inv, 1.3f, 0.9f);
	CHECK(b.State() == BossState::Fighting && b.PlayerHp() == 5 && b.Invoker().GetSlot(Slot::D) == SkillId::SunStrike);
	CHECK(NearF(b.Speed(), GetBossDefinition(1).speed * 1.3f, 1e-4f) && NearF(b.Window(), GetBossDefinition(1).window * 0.9f, 1e-5f));
	b.StartImmortal(1, 3, inv, 2.0f, 0.1f);
	CHECK(b.Window() == BOSS_MIN_WINDOW);
	b.Start(1);
	CHECK(b.Speed() == GetBossDefinition(1).speed && b.Window() == GetBossDefinition(1).window && b.PlayerHp() == START_HP);
	// the damage multiplier: a GOOD combo (Tornado -> Sun Strike) takes 70 % instead of 35 %
	BossSession d;
	d.Start(kClassic[0]);
	d.SetModifiers(1.0f, false, false, false, 2);
	BossTotals t = {};
	BossKeys(d, "QWWR" "EEER" "F");
	CHECK(BossRunUntil(d, t, Air15, 3.0f));
	d.Input(InputAction::D);
	CHECK(BossRunUntil(d, t, ComboResolved, 4.0f));
	CHECK(t.good == 1 && t.damage == 70 && d.BossHp() == 30);
}

// O-11: Aghanim's Scepter and every level-2 upgrade need a material, which the purchase uses up.
static void TestMaterialShop()
{
	Inventory inv = EmptyInventory();
	int gold = 1000000;
	CHECK(inv.material[0] == 0 && inv.material[1] == 0 && inv.material[2] == 0);
	const ItemId gold1[] = { ItemId::Blink, ItemId::Refresher, ItemId::Euls, ItemId::Bkb, ItemId::Midas, ItemId::Octarine,
		ItemId::Salve, ItemId::Cheese, ItemId::Smoke, ItemId::GreaterSmoke };
	for (ItemId id : gold1)  // level 1 (and consumables): gold only
		CHECK(RequiredMaterial(inv, id) == MATERIAL_NONE && CanBuy(inv, id, gold));
	CHECK(RequiredMaterial(inv, ItemId::Aghanim) == static_cast<int>(Material::PointBooster));
	CHECK(!CanBuy(inv, ItemId::Aghanim, gold) && !Buy(inv, ItemId::Aghanim, gold) && gold == 1000000);
	AddMaterial(inv, Material::PointBooster);
	CHECK(Buy(inv, ItemId::Aghanim, gold) && inv.material[0] == 0 && gold == 1000000 - 4000);
	CHECK(RequiredMaterial(inv, ItemId::Aghanim) == static_cast<int>(Material::SacredRelic));

	for (ItemId id : { ItemId::Blink, ItemId::Refresher, ItemId::Euls, ItemId::Bkb, ItemId::Midas, ItemId::Octarine })
		Buy(inv, id, gold);
	CHECK(RequiredMaterial(inv, ItemId::Blink) == static_cast<int>(Material::PointBooster)
		&& RequiredMaterial(inv, ItemId::Euls) == static_cast<int>(Material::PointBooster));
	CHECK(RequiredMaterial(inv, ItemId::Bkb) == static_cast<int>(Material::MysticStaff)
		&& RequiredMaterial(inv, ItemId::Midas) == static_cast<int>(Material::MysticStaff)
		&& RequiredMaterial(inv, ItemId::Octarine) == static_cast<int>(Material::MysticStaff));
	CHECK(RequiredMaterial(inv, ItemId::Refresher) == static_cast<int>(Material::SacredRelic));
	int before = gold;
	CHECK(!Buy(inv, ItemId::Bkb, gold) && gold == before && CurrentLevel(inv, ItemId::Bkb) == 1);  // gold is not enough
	AddMaterial(inv, Material::PointBooster);  // the wrong material for BKB II
	CHECK(!Buy(inv, ItemId::Bkb, gold) && inv.material[0] == 1);
	AddMaterial(inv, Material::MysticStaff);
	CHECK(Buy(inv, ItemId::Bkb, gold) && CurrentLevel(inv, ItemId::Bkb) == 2 && inv.material[1] == 0 && inv.material[0] == 1);
	CHECK(RequiredMaterial(inv, ItemId::Bkb) == MATERIAL_NONE && !CanBuy(inv, ItemId::Bkb, gold));  // nothing left to buy
	CHECK(Buy(inv, ItemId::Blink, gold) && inv.material[0] == 0 && !Buy(inv, ItemId::Euls, gold));
	for (int i = 0; i < MATERIAL_MAX + 5; ++i)
		AddMaterial(inv, Material::SacredRelic);
	CHECK(inv.material[2] == MATERIAL_MAX);
	CHECK(Buy(inv, ItemId::Refresher, gold) && Buy(inv, ItemId::Aghanim, gold) && inv.material[2] == MATERIAL_MAX - 2);
}

// M-3, M-4, O-3, O-4: the first Immortal of a run is Rimefang, after a 4 s warning; it takes no enemy number; lives
// and slots carry over.
static void TestImmortalWarning()
{
	PracticeSession s;
	UpdateResult warn = PlayToImmortalClean(s, 61);
	CHECK(warn.immortalWarning && warn.immortalBoss == 0 && warn.immortalTier == 1 && !warn.spawned);
	int count = s.SpawnCount();
	CHECK(count >= PLAY_ELITE_FIRST && count <= 45 && count % 5 == 0);  // right after an elite or an overlord
	CHECK(s.GetStats().kills == count);                                  // an extra: every enemy so far was a kill
	CHECK(s.ImmortalChance() == 0 && !s.ImmortalDue());                  // it has come: the chance starts again
	CHECK(s.ImmortalWarningLeft() == PLAY_IMMORTAL_WARNING && !s.ImmortalActive() && !s.Enemy().active);
	CHECK(s.ImmortalBoss() == 0 && s.ImmortalTierNow() == 1 && s.DefeatedBy() == -1);
	// the keys still work during the warning: invoke ahead, casts do nothing
	InvokeSkill(s, SkillId::ChaosMeteor);
	InvokeSkill(s, SkillId::Tornado);
	InputResult r = s.Input(InputAction::D);
	CHECK(r.accepted && !r.immortal && !r.tornadoLaunched && r.cast == CastOutcome::None);
	CHECK(!s.UseItem(0).used);
	int hp = s.GetStats().hp;
	float t0 = s.GetStats().survivalTime;
	UpdateResult u = {};
	bool spawned = false;
	for (int i = 0; i < 600 && !u.immortalFight; ++i)
	{
		u = s.Update(0.01f);
		spawned = spawned || u.spawned || s.Enemy().active;
	}
	CHECK(u.immortalFight && !spawned && s.ImmortalActive() && s.ImmortalWarningLeft() == 0.0f);
	CHECK(NearF(s.GetStats().survivalTime - t0, PLAY_IMMORTAL_WARNING, 0.011f));  // the run's clock keeps going
	const BossSession& b = s.Immortal();
	CHECK(b.State() == BossState::Fighting && b.BossIndex() == 0 && b.BossHp() == BOSS_FULL_HP && b.PlayerHp() == hp);
	CHECK(b.Speed() == GetBossDefinition(0).speed && b.Window() == GetBossDefinition(0).window);  // not scaled yet
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::Tornado && s.Invoker().GetSlot(Slot::F) == SkillId::ChaosMeteor);
	// the fight has the keys
	r = s.Input(InputAction::D);
	CHECK(r.accepted && r.immortal && r.boss.attemptStarted && b.AttemptRunning() && b.ProjectileCount() == 1);
	CHECK(s.ActiveTornadoCount() == 0 && !s.Enemy().active && s.SpawnCount() == count);

	// Survival never has one, nor elites or overlords
	PracticeSession v;
	v.Start(61);
	bool any = false;
	for (int guard = 0; guard < 100000 && v.SpawnCount() < 46 && v.State() == GameState::Playing; ++guard)
	{
		if (v.Enemy().active)
		{
			any = any || v.Enemy().kind != EnemyKind::Normal;
			PlayKill(v);
		}
		else
			any = any || v.Update(0.05f).immortalWarning;
	}
	CHECK(v.SpawnCount() == 46 && !any && !v.ImmortalActive() && v.ImmortalChance() == 0);
}

// O-8, M-7: the reward, the material, and PLAY goes on with the difficulty where it was (O-5).
static void TestImmortalWin()
{
	PracticeSession s;
	PlayToImmortalClean(s, 62);
	int count = s.SpawnCount();
	float clockBefore = s.GetStats().survivalTime;
	CHECK(ImmortalWaitFight(s));
	Stats before = s.GetStats();
	UpdateResult u = ImmortalCombo(s);
	CHECK(u.immortalUpdated && u.boss.comboComplete && u.boss.damage == 100 && u.boss.won);
	const KillReport& kill = u.kill;
	CHECK(kill.killed && kill.immortal && kill.kind == EnemyKind::Overlord);
	CHECK(kill.points == PLAY_IMMORTAL_POINTS || kill.points == 2 * PLAY_IMMORTAL_POINTS);
	CHECK(kill.gold >= PLAY_IMMORTAL_GOLD && kill.rune != Rune::None && !kill.runeChoice);
	CHECK(kill.materialCount == 1 && kill.materials[0] == Material::PointBooster);
	CHECK(kill.immortalChance == 0 && !kill.immortalComing);  // beating an Immortal adds nothing to the chance
	const Stats& st = s.GetStats();
	CHECK(st.score == before.score + kill.points && st.gold == before.gold + kill.gold);
	CHECK(st.bossesDefeated == before.bossesDefeated + 1 && st.immortalsBeaten == 1 && st.kills == before.kills + 1);
	CHECK(st.correctCasts == before.correctCasts + 1 && st.combo == before.combo + 1 && st.incorrectCasts == before.incorrectCasts);
	CHECK(s.Loadout().material[0] == 1 && s.Loadout().material[1] == 0 && s.Loadout().material[2] == 0);
	CHECK(!s.ImmortalActive() && s.State() == GameState::Playing && s.DefeatedBy() == -1 && s.ImmortalChance() == 0);
	CHECK(s.Invoker().GetSlot(Slot::D) == SkillId::DeafeningBlast);  // the slots come back out of the fight
	// the next enemy: the next number of the schedule, as fast as if the warning and the fight had taken no time
	float clockAfter = st.survivalTime;
	CHECK(clockAfter - clockBefore > PLAY_IMMORTAL_WARNING + 2.0f);
	while (!s.Enemy().active && s.State() == GameState::Playing)
		s.Update(0.05f);
	CHECK(s.Enemy().active && s.SpawnCount() == count + 1 && s.Enemy().kind == PlayEnemyKind(count + 1));
	float expected = DifficultyAt(clockBefore + (s.GetStats().survivalTime - clockAfter)).enemySpeed;
	CHECK(NearF(s.Enemy().speed, expected, 1.0f));
	KillReport next = PlayKill(s);
	CHECK(next.killed && !next.immortal && next.materialCount == 0);

	// a broken combo is one wrong cast; a new run starts clean
	PracticeSession w;
	PlayToImmortalClean(w, 62);
	CHECK(ImmortalWaitFight(w));
	int wrong = w.GetStats().incorrectCasts;
	InvokeSkill(w, SkillId::ColdSnap);
	InvokeSkill(w, SkillId::Tornado);
	w.Input(InputAction::D);
	InputResult r = w.Input(InputAction::F);  // Cold Snap out of order
	CHECK(r.immortal && r.boss.fail == ComboFail::WrongSpell && w.GetStats().incorrectCasts == wrong + 1);
	w.Start(63);
	CHECK(!w.ImmortalActive() && w.ImmortalWarningLeft() == 0.0f && w.Loadout().material[0] == 0 && w.ImmortalChance() == 0);
}

// O-4, O-9: contact costs 1 life and knocks it back; at 0 lives the run ends and names the boss.
static void TestImmortalContact()
{
	PracticeSession s;
	PlayToImmortalClean(s, 63);
	CHECK(ImmortalWaitFight(s));
	int hp = s.GetStats().hp;
	UpdateResult u = {};
	for (int i = 0; i < 4000 && !u.leaked; ++i)
		u = s.Update(0.01f);
	CHECK(u.leaked && u.leakDamage == 1 && !u.shieldUsed && u.boss.playerHit && !u.gameOver);
	CHECK(s.GetStats().hp == hp - 1 && s.GetStats().combo == 0 && s.ImmortalActive());
	CHECK(s.Immortal().Phase() == BossPhase::PushedBack);
	// a Shield takes the next contact and is used up
	s.ApplyRune(Rune::Shield);
	u = {};
	for (int i = 0; i < 4000 && !u.leaked; ++i)
		u = s.Update(0.01f);
	CHECK(u.leaked && u.shieldUsed && u.leakDamage == 0 && u.boss.contactBlocked && s.GetStats().hp == hp - 1 && !s.HasShield());
	// then it keeps coming until the lives are gone
	for (int i = 0; i < 40000 && s.State() == GameState::Playing; ++i)
		u = s.Update(0.01f);
	CHECK(u.gameOver && s.State() == GameState::GameOver && s.GetStats().hp == 0);
	CHECK(s.DefeatedBy() == 0 && !s.ImmortalActive() && s.GetStats().immortalsBeaten == 0);
	CHECK(s.Loadout().material[0] == 0);
	s.Start(64);
	CHECK(s.DefeatedBy() == -1);
}

// O-6: the items against an immortal.
static void TestImmortalItems()
{
	// Blink: it walks back; Frost slows it
	PracticeSession s;
	PlayToImmortalClean(s, 64, LoadoutWith(ItemId::Blink, 1));
	CHECK(ImmortalWaitFight(s));
	for (int i = 0; i < 100; ++i)
		s.Update(0.01f);
	float x0 = s.Immortal().X();
	s.Update(0.1f);
	float normal = x0 - s.Immortal().X();
	CHECK(NearF(normal, GetBossDefinition(0).speed * 0.1f, 1e-3f));
	s.ApplyRune(Rune::Frost);
	x0 = s.Immortal().X();
	s.Update(0.1f);
	CHECK(NearF(x0 - s.Immortal().X(), normal * PLAY_FROST_SPEED, 1e-3f));
	x0 = s.Immortal().X();
	CHECK(s.UseItem(0).used && s.ItemCooldown(ItemId::Blink) == 40.0f);
	s.Update(0.1f);
	CHECK(s.Immortal().X() > x0 && s.Immortal().X() <= BOSS_START_X);

	// Wind Waker: still, and pushed back at once
	PracticeSession w;
	PlayToImmortalClean(w, 64, LoadoutWith(ItemId::Euls, 2));
	CHECK(ImmortalWaitFight(w));
	for (int i = 0; i < 80; ++i)
		w.Update(0.1f);
	x0 = w.Immortal().X();
	CHECK(x0 < BOSS_START_X - PLAY_EULS_PUSHBACK);
	CHECK(w.UseItem(0).used && NearF(w.Immortal().X(), x0 + PLAY_EULS_PUSHBACK, 1e-3f));
	x0 = w.Immortal().X();
	w.Update(0.1f); w.Update(0.1f);
	CHECK(w.Immortal().X() == x0);

	// Black King Bar: the contact costs nothing, and a Shield is not used up while it runs
	PracticeSession b;
	PlayToImmortalClean(b, 64, LoadoutWith(ItemId::Bkb, 1));
	CHECK(ImmortalWaitFight(b));
	b.ApplyRune(Rune::Shield);
	int hp = b.GetStats().hp;
	UpdateResult u = {};
	for (int i = 0; i < 4000 && (b.Immortal().X() - HIT_LINE_X) / b.Immortal().Speed() > 2.0f; ++i)
		u = b.Update(0.01f);
	CHECK(b.UseItem(0).used);
	for (int i = 0; i < 500 && !u.leaked; ++i)
		u = b.Update(0.01f);
	CHECK(u.leaked && u.shieldUsed && u.leakDamage == 0 && b.GetStats().hp == hp && b.HasShield());

	// Healing Salve heals inside the fight too
	PracticeSession h;
	PlayToImmortalClean(h, 64, LoadoutWith(ItemId::Salve, 1, ItemId::Salve, 1));
	CHECK(ImmortalWaitFight(h));
	hp = h.GetStats().hp;
	if (hp < PLAY_MAX_HP)
	{
		CHECK(h.UseItem(0).used && h.GetStats().hp == hp + 1);
		h.Update(0.01f);
		CHECK(h.Immortal().PlayerHp() == hp + 1);
	}

	// Refresher Orb: armed until the next combo that deals damage, which deals x2
	PracticeSession r;
	PlayToImmortalClean(r, 64, LoadoutWith(ItemId::Refresher, 1));
	CHECK(!r.UseItem(0).used && !r.RefresherArmed());  // nothing to use it on during the warning
	CHECK(ImmortalWaitFight(r));
	CHECK(r.UseItem(0).used && r.RefresherArmed() && r.ItemCooldown(ItemId::Refresher) == 90.0f);
	CHECK(!r.UseItem(0).used);
	for (int i = 0; i < 100; ++i)
		r.Update(0.01f);
	CHECK(r.RefresherArmed());
	UpdateResult c = ImmortalCombo(r);
	CHECK(c.boss.comboComplete && c.boss.damage == 200 && c.kill.killed && !r.RefresherArmed());

	// Aghanim's Scepter: the rune of an immortal is a choice too; Midas adds to its gold
	Inventory inv = LoadoutWith(ItemId::Aghanim, 1);
	int gold = 100000;
	Buy(inv, ItemId::Midas, gold);
	PracticeSession a;
	PlayToImmortalClean(a, 64, inv);
	CHECK(ImmortalWaitFight(a));
	KillReport kill = ImmortalWin(a);
	CHECK(kill.killed && kill.runeChoice && kill.rune == Rune::None && a.RuneChoiceCount() == 2);
	CHECK(kill.gold == PLAY_IMMORTAL_GOLD + PLAY_IMMORTAL_GOLD / 2);
	CHECK(a.ChooseRune(0) != Rune::None && a.RuneChoiceCount() == 0);
}

// §32 M-3: the chance. Nothing for 14 enemies; an elite beaten adds 10 %, an overlord 20 %; one that reaches the
// player adds nothing; the roll is made at once; by the 45th enemy the Immortal has always come.
static void TestImmortalChance()
{
	PracticeSession s;
	s.SetMode(SessionMode::Play);
	s.Start(71);
	PlayWaitSpawn(s);
	for (int n = 1; n < PLAY_ELITE_FIRST; ++n)
	{
		CHECK(s.ImmortalChance() == 0 && !s.ImmortalDue());
		KillReport kill = PlayKill(s);
		CHECK(kill.killed && kill.immortalChance == 0 && !kill.immortalComing);
		PlayWaitSpawn(s);
	}
	CHECK(s.SpawnCount() == PLAY_ELITE_FIRST && s.Enemy().kind == EnemyKind::Elite);
	UpdateResult leak = RunUntilLeak(s);  // the elite reaches the player: 2 lives, and no chance gained
	CHECK(leak.leaked && leak.leakDamage == PLAY_LEAK_ELITE && s.ImmortalChance() == 0 && !s.ImmortalDue());
	PlayWaitSpawn(s);
	while (s.Enemy().kind == EnemyKind::Normal && s.State() == GameState::Playing)
	{
		PlayKill(s);
		PlayWaitSpawn(s);
	}
	CHECK(s.SpawnCount() == PLAY_OVERLORD_FIRST && s.Enemy().kind == EnemyKind::Overlord && s.ImmortalChance() == 0);
	KillReport kill = PlayKill(s);
	CHECK(kill.killed && kill.immortalChance == PLAY_IMMORTAL_CHANCE_OVERLORD);
	CHECK(s.ImmortalChance() == PLAY_IMMORTAL_CHANCE_OVERLORD && s.ImmortalDue() == kill.immortalComing);

	// the first roll (10 % after the 15th enemy) hits about one run in ten
	int hits = 0;
	const int runs = 400;
	for (int seed = 1; seed <= runs; ++seed)
	{
		PracticeSession r;
		r.SetMode(SessionMode::Play);
		r.Start(static_cast<unsigned>(seed) * 7919u);
		PlayWaitSpawn(r);
		while (r.Enemy().kind == EnemyKind::Normal)
		{
			PlayKill(r);
			PlayWaitSpawn(r);
		}
		KillReport k = PlayKill(r);
		CHECK(k.killed && k.kind == EnemyKind::Elite && k.immortalChance == PLAY_IMMORTAL_CHANCE_ELITE);
		CHECK(r.ImmortalDue() == k.immortalComing);
		if (k.immortalComing)
		{
			++hits;
			UpdateResult u = {};
			for (int i = 0; i < 200 && !u.immortalWarning; ++i)
				u = r.Update(0.05f);
			CHECK(u.immortalWarning && u.immortalBoss == 0 && r.SpawnCount() == PLAY_ELITE_FIRST && !r.Enemy().active);
		}
	}
	CHECK(hits >= runs / 20 && hits <= runs / 5);

	// with every elite and overlord beaten the chance reaches 100 % at the 45th enemy at the latest; then it builds
	// again from 0 and the second Immortal is the next of the list
	int latest = 0;
	for (unsigned seed = 1; seed <= 40; ++seed)
	{
		PracticeSession r;
		UpdateResult warn = PlayToImmortal(r, seed * 104729u);
		CHECK(warn.immortalWarning && warn.immortalBoss == 0 && r.SpawnCount() <= 45);
		if (r.SpawnCount() > latest)
			latest = r.SpawnCount();
		if (seed > 5)
			continue;
		int first = r.SpawnCount();
		CHECK(ImmortalWaitFight(r));
		CHECK(ImmortalWin(r).killed && r.ImmortalChance() == 0);
		warn = PlayOnToImmortal(r);
		CHECK(warn.immortalWarning && warn.immortalBoss == 1 && warn.immortalTier == 2);
		CHECK(r.SpawnCount() >= first + 5 && r.SpawnCount() <= first + 35);
	}
	CHECK(latest >= 25);  // and it is not always early
}

// §32 M-4, M-7: the first five of a run come in order (combos of 4 to 8) with their drops; from the sixth on a random
// one, faster and with a shorter window.
static void TestImmortalMilestones()
{
	PracticeSession s;
	UpdateResult warn = PlayToImmortal(s, 65);
	int materials = 0;
	for (int i = 0; i < BOSS_COUNT; ++i)
	{
		CHECK(warn.immortalWarning && warn.immortalBoss == i && warn.immortalTier == ImmortalTier(i));
		CHECK(ImmortalWaitFight(s));
		CHECK(s.Immortal().Speed() == GetBossDefinition(i).speed && s.Immortal().ComboLength() == 4 + i);
		KillReport kill = ImmortalWin(s);
		CHECK(kill.killed && kill.immortal && kill.materialCount == ImmortalDropCount(i));
		CHECK(i > 2 || kill.materials[0] == MaterialOfBoss(i));
		materials += kill.materialCount;
		warn = PlayOnToImmortal(s);
	}
	CHECK(materials == 7 && s.Loadout().material[0] + s.Loadout().material[1] + s.Loadout().material[2] == 7);
	CHECK(s.Loadout().material[0] >= 1 && s.Loadout().material[1] >= 1 && s.Loadout().material[2] >= 1);
	CHECK(s.GetStats().immortalsBeaten == BOSS_COUNT);
	// the sixth and the seventh
	int last = BOSS_COUNT - 1;
	for (int n = 1; n <= 2; ++n)
	{
		CHECK(warn.immortalWarning && warn.immortalBoss != last);
		CHECK(warn.immortalBoss >= 0 && warn.immortalBoss < BOSS_COUNT && warn.immortalTier == ImmortalTier(warn.immortalBoss));
		last = warn.immortalBoss;
		CHECK(ImmortalWaitFight(s));
		const BossDefinition& def = GetBossDefinition(last);
		float window = def.window * (1.0f - PLAY_IMMORTAL_WINDOW_STEP * n);
		CHECK(NearF(s.Immortal().Speed(), def.speed * (1.0f + PLAY_IMMORTAL_SPEED_STEP * n), 1e-3f));
		CHECK(NearF(s.Immortal().Window(), window > BOSS_MIN_WINDOW ? window : BOSS_MIN_WINDOW, 1e-4f));
		KillReport kill = ImmortalWin(s);
		CHECK(kill.killed && kill.materialCount == ImmortalDropCount(last));
		warn = PlayOnToImmortal(s);
	}
	CHECK(s.GetStats().immortalsBeaten == BOSS_COUNT + 2 && s.State() == GameState::Playing);
}


// ---------------------------------------------------------------- touch layout (spec §30; plain geometry, no SDL)

static bool BoxesApart(const touchlayout::Box& a, const touchlayout::Box& b)
{
	return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
}

static void TestTouchLayout()
{
	using namespace touchlayout;
	// the default is exactly where the buttons were before the layout could be changed
	Layout d = Default(false, DEFAULT_SIZE);
	const int old[BUTTON_COUNT][2] = { { 16, 184 }, { 112, 184 }, { 208, 184 }, { 304, 184 }, { 256, 280 }, { 352, 280 } };
	for (int i = 0; i < BUTTON_COUNT; ++i)
	{
		Box b = ButtonBox(d, i);
		CHECK(b.x == old[i][0] && b.y == old[i][1] && b.w == 88 && b.h == 88);
	}
	CHECK(d.x[BLOCK_ITEMS] == 744 && d.y[BLOCK_ITEMS] == 292 && BlockBox(d, BLOCK_CAST).w == 0);
	CHECK(Valid(d) && HudShift(d) == HUD_SHIFT);  // the orb row moves right of the cluster, as it always did

	// every default is valid, its buttons never overlap, and the split groups keep their shape
	for (int split = 0; split < 2; ++split)
	{
		for (int size = 0; size < SIZE_COUNT; ++size)
		{
			Layout l = Default(split != 0, size);
			CHECK(Valid(l) && l.size == size && l.split == (split != 0));
			bool apart = true, inside = true;
			for (int a = 0; a < BUTTON_COUNT; ++a)
			{
				Box box = ButtonBox(l, a);
				Box block = BlockBox(l, l.split && a >= 3 ? BLOCK_CAST : BLOCK_MAIN);
				inside = inside && box.x >= block.x && box.y >= block.y && box.x + box.w <= block.x + block.w
					&& box.y + box.h <= block.y + block.h && box.w == SIZES[size];
				for (int b = a + 1; b < BUTTON_COUNT; ++b)
					apart = apart && BoxesApart(box, ButtonBox(l, b));
			}
			CHECK(apart && inside);
		}
	}
	Layout s = Default(true, DEFAULT_SIZE);
	int step = SIZES[DEFAULT_SIZE] + GAP;
	CHECK(ButtonBox(s, 1).x == ButtonBox(s, 0).x + step && ButtonBox(s, 2).x == ButtonBox(s, 0).x + 2 * step);
	CHECK(ButtonBox(s, 0).y == ButtonBox(s, 2).y && BlockBox(s, BLOCK_MAIN).h == SIZES[DEFAULT_SIZE]);
	// R above D and F, half a step in: the same places they have in the whole cluster
	CHECK(ButtonBox(s, 3).x - ButtonBox(s, 4).x == ButtonBox(d, 3).x - ButtonBox(d, 4).x);
	CHECK(ButtonBox(s, 5).x - ButtonBox(s, 4).x == step && ButtonBox(s, 4).y - ButtonBox(s, 3).y == step);
	CHECK(BlockBox(s, BLOCK_MAIN).x < 200 && BlockBox(s, BLOCK_CAST).x > 600);  // one group per thumb
	CHECK(HudShift(s) == 0);                                                     // nothing in the middle: the orbs stay centred

	// a block cannot leave the area, and two blocks cannot overlap
	Layout m = d;
	m.x[BLOCK_MAIN] = -500; m.y[BLOCK_MAIN] = 9999;
	CHECK(!Valid(m));
	Clamp(m, BLOCK_MAIN);
	CHECK(m.x[BLOCK_MAIN] == AREA_LEFT && m.y[BLOCK_MAIN] + BlockBox(m, BLOCK_MAIN).h == AREA_BOTTOM && Valid(m));
	m.x[BLOCK_MAIN] = 9999; m.y[BLOCK_MAIN] = -9999;
	Clamp(m, BLOCK_MAIN);
	CHECK(m.x[BLOCK_MAIN] + BlockBox(m, BLOCK_MAIN).w == AREA_RIGHT && m.y[BLOCK_MAIN] == AREA_TOP);
	CHECK(!Valid(m));                       // it now lies on the item bar
	Sanitize(m);
	CHECK(Valid(m) && m.x[BLOCK_MAIN] == 16 && m.y[BLOCK_MAIN] == 184);  // anything invalid falls back to the default
	Layout bad = d;
	bad.size = 7;
	CHECK(!Valid(bad));
	Sanitize(bad);
	CHECK(Valid(bad) && bad.size == DEFAULT_SIZE);
	// the unused CAST block of a one-cluster layout never blocks anything
	Layout u = d;
	u.x[BLOCK_CAST] = u.x[BLOCK_ITEMS]; u.y[BLOCK_CAST] = u.y[BLOCK_ITEMS];
	CHECK(Valid(u));
	// the orbs go left when both the middle and the right are taken
	Layout r = Default(true, DEFAULT_SIZE);
	r.x[BLOCK_MAIN] = 400; r.y[BLOCK_MAIN] = 150;
	CHECK(Valid(r) && HudShift(r) == -HUD_SHIFT);
	r.y[BLOCK_MAIN] = 200;                  // below the orb row: nothing to avoid
	CHECK(Valid(r) && HudShift(r) == 0);
}

int main()
{
	RunTest("enemy definitions", TestEnemyDefinitions);
	RunTest("initial state", TestInitialState);
	RunTest("enemy spawn flow", TestSpawnFlow);
	RunTest("correct cast", TestCorrectCast);
	RunTest("incorrect cast", TestIncorrectCast);
	RunTest("score + combo + best combo", TestScoreAndCombo);
	RunTest("enemy reaches the player", TestLeak);
	RunTest("game over", TestGameOver);
	RunTest("casts that are not judged", TestUnjudgedCasts);
	RunTest("accuracy", TestAccuracy);
	RunTest("new session resets everything", TestNewSession);
	RunTest("best combo across restarts", TestBestComboAcrossRestarts);
	RunTest("state transitions (Enter/Esc)", TestStateTransitions);
	RunTest("tornado: pure functions", TestTornadoFunctions);
	RunTest("tornado: launch (D / F / others)", TestTornadoLaunch);
	RunTest("tornado: flight (direction, dt)", TestTornadoFlight);
	RunTest("tornado: hit", TestTornadoHit);
	RunTest("tornado: miss", TestTornadoMiss);
	RunTest("tornado: bound to its enemy", TestTornadoBoundToEnemy);
	RunTest("tornado: wrong target", TestTornadoWrongTarget);
	RunTest("tornado: no enemy", TestTornadoNoEnemy);
	RunTest("tornado: many projectiles", TestTornadoManyProjectiles);
	RunTest("tornado: animation loop", TestTornadoAnimationLoop);
	RunTest("tornado: lifetime", TestTornadoLifetime);
	RunTest("difficulty bounds", TestDifficulty);
	RunTest("time step (dt)", TestTimeStep);
	RunTest("input outside Playing", TestInputOutsidePlaying);
	RunTest("invoker through the session", TestInvokerThroughSession);
	RunTest("persistent bests", TestPersistentBests);
	RunTest("recipe hint: assisted runs", TestAssistedRuns);
	RunTest("leaderboard order", TestTopRuns);
	RunTest("tutorial: script", TestTutorialScript);
	RunTest("tutorial: guided keys", TestTutorialGuidedKeys);
	RunTest("tutorial: Sun Strike lesson", TestTutorialSunStrike);
	RunTest("tutorial: run", TestTutorialRun);
	RunTest("tutorial: run leaks", TestTutorialRunLeaks);
	RunTest("boss: definitions and timings", TestBossDefinitions);
	RunTest("boss: long combos (1.9)", TestBossLongCombo);
	RunTest("boss: timing grades", TestBossGrades);
	RunTest("boss: start", TestBossStart);
	RunTest("boss: perfect combo (boss 1)", TestBossPerfectCombo);
	RunTest("boss: GREAT / GOOD damage", TestBossPartialGrades);
	RunTest("boss: too early / too late", TestBossTiming);
	RunTest("boss: wrong spell, double tap", TestBossWrongSpell);
	RunTest("boss: four-spell combo (boss 3)", TestBossFourSpellCombo);
	RunTest("boss: win", TestBossWin);
	RunTest("boss: lose", TestBossLose);
	RunTest("boss 1.5: all ten skills, kinds", TestBossAllSkills);
	RunTest("boss 1.5: quick combo (Frost Troll)", TestBossQuickCombo);
	RunTest("boss 1.5: quick grades, slow, miss", TestBossQuickGrades);
	RunTest("boss 1.5: five-spell combo", TestBossFiveSpellCombo);
	RunTest("boss 1.5: two phases (Archon)", TestBossPhases);
	RunTest("play: tables", TestPlayTables);
	RunTest("play: survival unchanged", TestPlaySurvivalUnchanged);
	RunTest("play: elite 15, overlord 20, sizes", TestPlayBossCadence);
	RunTest("play: chains", TestPlayChain);
	RunTest("play: leak damage, shield", TestPlayLeakDamage);
	RunTest("play: runes", TestPlayRunes);
	RunTest("items: shop, upgrades, slots", TestItemShop);
	RunTest("items: use and cooldowns", TestItemUse);
	RunTest("items: refresher, bkb, midas, aghanim", TestItemBossItems);
	RunTest("immortal: tables, scaling, x2", TestImmortalTables);
	RunTest("immortal: materials in the shop", TestMaterialShop);
	RunTest("immortal: warning and hand-off", TestImmortalWarning);
	RunTest("immortal: win, reward, clock", TestImmortalWin);
	RunTest("immortal: contact, game over", TestImmortalContact);
	RunTest("immortal: items", TestImmortalItems);
	RunTest("immortal: the chance", TestImmortalChance);
	RunTest("immortal: 4 to 8, then harder", TestImmortalMilestones);
	RunTest("touch layout: blocks, split, sizes", TestTouchLayout);

	printf("\n%d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
