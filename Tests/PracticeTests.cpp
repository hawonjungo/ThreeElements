// Tests for the Practice layer (Three Elements/Practice/Practice.h/.cpp) on top of the Invoker Core.
//
// No test framework, no SDL, no window: PracticeSession takes time as `dt` and the seed as a parameter,
// so whole sessions run deterministically here. Exit code 0 = every check passed.
// How to run: see Tests/README.md.

#include "Practice/Practice.h"

#include <cmath>
#include <cstdio>

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

static SkillId Different(SkillId id)  // any skill other than `id`
{
	return static_cast<SkillId>((static_cast<int>(id) + 1) % invoker::SKILL_COUNT);
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

// Waits for an enemy and kills it with the correct spell (cast from D).
static void Kill(PracticeSession& s)
{
	RunUntilEnemy(s);
	InvokeSkill(s, s.Enemy().target);
	s.Input(InputAction::D);
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
	InvokeSkill(s, previous);
	s.Input(InputAction::D);
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
		InvokeSkill(s, t);                // kill it and wait for the next one
		s.Input(InputAction::D);
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
	CHECK(r.accepted && r.cast == CastOutcome::Correct);
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
	CHECK(r.cast == CastOutcome::Correct && !s.Enemy().active);
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
	CHECK(r.cast == CastOutcome::Correct && !s.Enemy().active);
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
		InvokeSkill(s, s.Enemy().target);
		s.Input(InputAction::D);
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
	RunTest("difficulty bounds", TestDifficulty);
	RunTest("time step (dt)", TestTimeStep);
	RunTest("input outside Playing", TestInputOutsidePlaying);
	RunTest("invoker through the session", TestInvokerThroughSession);

	printf("\n%d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
