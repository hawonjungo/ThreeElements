# Tests

Two small console programs, no test framework, no SDL, no game assets. Each prints one line per test group and exits with code `0` when every check passed, `1` otherwise.

| Program | Tests | Checks |
|---|---|---|
| `InvokerCoreTests` | the **Invoker Core** (`Three Elements/Core/Invoker.h/.cpp`): the Q/W/E → R → D/F mechanic | 243 |
| `PracticeTests` | the **Practice layer** (`Three Elements/Practice/Practice.h/.cpp`) running on top of the Core: enemies, casts, HP, score, combo, accuracy, Game Over, restart, difficulty | 328 |

The SDL presentation (`GameManager`, `PixelText`, `MainPlayer::TranslateKey`) has no automated test; see "Not covered" at the end.

## Running the tests

Requirements: Visual Studio 2022 with the *Desktop development with C++* workload (MSBuild, toolset v143). Nothing else is needed: no SDL, no game assets.

**One command** (builds and runs both programs; exit code 0 = everything passed):

```
Tests\run_tests.cmd                  build + run Debug x64
Tests\run_tests.cmd Release          Release x64
Tests\run_tests.cmd Debug Win32      32-bit
Tests\run_tests.cmd Debug x64 1000000 42     custom random-key count and seed
```

**From Visual Studio:** open `Three Elements.sln`, right-click *InvokerCoreTests* → *Set as Startup Project*, then *Ctrl+F5* (or *PracticeTests*). (Switch the startup project back to *Three Elements* to run the game.)

**With MSBuild directly:**

```
msbuild Tests\InvokerCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
Tests\bin\x64\Debug\InvokerCoreTests.exe [randomKeyCount [seed]]
msbuild Tests\PracticeTests.vcxproj /p:Configuration=Debug /p:Platform=x64
Tests\bin\x64\Debug\PracticeTests.exe
```

Expected output of the Core tests (defaults):

```
[catalog                     ]  ...  checks, 0 failed
...
[random regression vs original]   2 checks, 0 failed
  random regression: 300000 keys (seed 12345), ~50000 invokes, 0 mismatches vs the original logic

243 checks, 0 failed
```

A failing check prints `FAIL line N: <expression>` and the program exits with code 1.

## What is covered — Invoker Core tests (`InvokerCoreTests.cpp`)

| Group | What it checks |
|---|---|
| catalog | 10 skills, ids match catalog order, each recipe has 3 orbs, recipes are unique, every 3-orb combination maps to exactly one skill, 2 orbs map to no skill |
| legacy catalog parity | recipes (as sorted letters), and icon paths identical to the tables the game used before the Core was extracted |
| all recipes + permutations | all 27 ordered Q/W/E triples resolve like the original table; QQQ, WWW, EEE; all permutations of Tornado (QWW), Deafening Blast (QWE), Ghost Walk (QQW), Ice Wall (QQE) |
| incomplete combinations | R with 0, 1 or 2 orbs does nothing: no spell, orbs and slots untouched |
| rolling orbs | a 4th orb drops the oldest; entry order is kept for display only |
| slot order | D = newest invoked spell, F = previous; casting keeps the slots (invoke A, invoke B, cast A, cast B, invoke C, cast B, cast C) |
| duplicate invoke | invoking the spell already in D changes nothing; invoking the one in F swaps D and F; a new spell drops the old F |
| cast behaviour | casting an empty slot reports "empty" |
| reset | clears the orbs and both slots |
| apply() mapping | Q/W/E add orbs |
| random regression | 300 000 random key presses run through both the **original** implementation (a verbatim copy of the old `MainPlayer`/`Skill` logic kept inside the test as an oracle) and the Core; orbs, D and F must be identical after every key |

## Design notes

- `InvokerCoreTests.vcxproj` compiles `Three Elements/Core/Invoker.cpp` **directly**, so the tests exercise the same source the game builds. It is a separate project: the game project does not reference it, and nothing from `Tests/` is linked into the game or copied next to it.
- The test project builds with `/W4 /WX`, which also keeps the Core free of warnings.
- Output goes to `Tests/bin` and `Tests/obj` (both git-ignored).
- To add a test: write a `static void TestXxx()` using `CHECK(...)` in `InvokerCoreTests.cpp` and register it with `RunTest(...)` in `main`.
- `run_tests.cmd` must keep CRLF line endings.

## Practice tests (`PracticeTests.cpp`)

`PracticeSession` receives time as `dt` and the random seed as a parameter, so whole play sessions run deterministically without a window.

| Group | What it checks |
|---|---|
| enemy definitions | 10 enemies, each with a valid sprite/frames/target; every one of the 10 skills is required by exactly one enemy |
| initial state | Ready, HP 3/3, everything else 0, accuracy 0.0; nothing happens before `Start()` |
| enemy spawn flow | first enemy after the initial delay, one enemy at a time, spawns at the right edge and moves at `speed * dt`, next enemy after a kill, never the same target twice in a row (400 spawns), same seed → same order |
| correct cast | clears the enemy, score +1, combo +1, correct count, HP unchanged, spell stays in its slot, works from D and from F |
| incorrect cast | enemy stays, no HP loss, no score/combo change, counted as incorrect, unlimited retries, enemy keeps moving, a later correct cast works |
| score + combo + best combo | streaks, wrong cast does not reset the combo, a leak does, best combo only ever rises |
| enemy reaches the player | HP −1, combo reset, score kept, enemy removed, not counted as a cast, takes about `distance / speed`, huge `dt` cannot skip the hit line |
| game over | HP 0 → Game Over exactly once; no spawns, no clock, no input afterwards |
| casts that are not judged | empty D/F, casts with no active enemy (before the first spawn and between two enemies), R with fewer than 3 orbs: no penalty and not counted |
| accuracy | 10 correct + 2 wrong = 83.3 % (no integer division), zero casts → 0.0, leaks do not change it |
| new session resets everything | HP, score, current combo, counters, accuracy, clock, enemy, orbs, D/F, spawn timer and difficulty clock; restart mid-session and after Game Over. The best combo record is the one thing that is kept |
| best combo across restarts | new session / restart / Game Over / return to Ready reset the current combo but keep the record; the record only rises on a new record (lower or equal streaks and wrong casts leave it alone); a fresh session object starts at 0 |
| state transitions (Enter/Esc) | Ready: Esc requests quit, Enter starts; Playing: Enter ignored, Esc returns to Ready (session stopped and reset, no quit); Ready is inert; Game Over: Enter starts a new session, Esc returns to Ready; Esc twice from Playing = Ready then quit request |
| difficulty bounds | speed and delay stay inside their limits, never get easier, no jumps, playable at both ends |
| time step (dt) | negative and huge `dt` are clamped; movement does not depend on how the time is sliced |
| input outside Playing | gameplay keys are ignored in Ready and Game Over |
| invoker through the session | the Core rules still hold when driven through `PracticeSession::Input` |

These tests were also checked with 12 deliberate bugs (e.g. wrong casts costing HP, combo not reset by a leak, best combo never updated, integer-division accuracy, missing `dt` clamp, target repeated) plus 6 for the best combo and Enter/Esc rules (restart resetting the record, Esc quitting from Playing, Enter restarting a running session, ...); every one made the suite fail.

## Not covered by automated tests

- The SDL layer: window, drawing, HUD, the mapping of the SDL Enter/Esc keys to `PressEnter` / `PressEscape` (the rules behind them are tested above) and `MainPlayer::TranslateKey` (which ignores `key.repeat`).
- To check it manually, run the game from `Three Elements/` with `--debug` (development only). It prints each enemy's target skill and recipe to the console and shows a `DEBUG TARGET` line, so a session can be driven by hand or by a script. In normal play nothing reveals the target.
