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

	printf("\n%d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
