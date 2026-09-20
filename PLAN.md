# PLAN.md — Three Elements roadmap

Based on [PROJECT_AUDIT.md](PROJECT_AUDIT.md) (2026-09-21 snapshot of the code before Phase 1; read it for file/line references and bug IDs B1…B17) and [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md).

> **Precedence:** [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md) v2 is the source of truth for gameplay rules; where it conflicts with this file, the spec wins. It has **no blocking open decisions**; tunables (starting speed/delay, hit-line position, sprite ↔ skill table, restart key) are data/constants chosen at implementation time. **Hints are excluded** from the MVP. **Persistent local best stats** (Best Score / Best Combo / Best Survival Time) are part of the MVP; build them once the session logic works.

## Status

| Phase | Title | Status |
|---|---|---|
| 0 | Audit & Gameplay Specification | **Done** |
| 1 | Stabilization | **Done** (branch `phase-1-stabilization`) |
| 2 | Invoker Core Extraction | **Done** (branch `phase-2-invoker-core`) |
| 3 | Practice Gameplay | **Implemented, awaiting owner review** (branch `phase-2-invoker-core`, not committed) |
| 4 | Game Loop / Input / State readiness | placeholder |
| 5 | Web MVP (GitHub Pages) | placeholder |
| 6 | UX / Audio / Game Feel | placeholder |
| 7 | Android | placeholder |
| 8 | Future Combat / Story | placeholder, not started |
| 9 | PC / Steam | placeholder, conditional |

## Product direction (fixed for this plan)
Invoker-style **practice game**, endless session. Trains: input speed, quick skill switching, recipe memory, choosing the right skill, keeping combos, reaction time.
Core loop: **enemy appears → it corresponds to one of the 10 skills → player recalls the recipe → Q/W/E → R → D/F → correct spell removes the enemy.** Wrong spell: no damage, enemy keeps coming, and if it reaches the player, HP goes down. Game over at HP 0.
MVP facts: all 10 skills from the start; 10 enemy types ↔ 10 skills through data (`targetSkillId`); Q/W/E order irrelevant; no cooldowns; one active enemy at a time; HP 3; difficulty rises with time; score/combo/best combo/accuracy/survival time; pixel art, side view. Targets: **Web first (GitHub Pages)** → Android → maybe PC/Steam. Touch UI = Q/W/E/R/D/F buttons laid out like a keyboard.

## Ground rules for every phase
- Smallest viable change; preserve working code (see "do not rewrite" list in the audit §17). Keep SDL2 + SDL2_image. No new engine/framework, no ECS, no scene graph, no DI container, no event bus.
- Layering (spec §20): **Presentation → Practice → Core**; Core and Practice never include SDL.
- The game must build (Debug and Release x64) and start after each commit; `Tests\run_tests.cmd` must pass.
- Do not start Combat or Story work before Phase 8.
- Keep `CLAUDE.md`, this file's checkboxes and the progress log up to date when a phase completes.

---

## Phase 0 — Audit & Gameplay Specification  *(DONE)*
- **Objective:** understand the repo and fix the product rules before coding.
- **Delivered:** `PROJECT_AUDIT.md` (architecture, flows, bugs B1–B17, Web/Android risks), `GAMEPLAY_SPEC.md` v2 (authoritative Practice Mode MVP rules, Core/Practice/Presentation boundaries), this plan, `CLAUDE.md` hand-over.
- **Completion criteria:** documents reviewed and approved by the owner. ☑

---

## Phase 1 — Stabilization  *(DONE)*
- **Objective:** a clean, portable baseline with **unchanged gameplay**.
- **Delivered:**
  - [x] Build portability: relative SDL include/lib paths for all four configurations (Release x64 builds); SDL DLLs copied next to the exe after build; debugger working directory set.
  - [x] Generated build outputs and `.vcxproj.user` no longer tracked by git.
  - [x] `printf("%s", std::string)` undefined behaviour fixed; spawn-timer shadowing/uninitialised member fixed; `std::rand` seeded; missing includes added.
  - [x] Initialisation errors logged; `InitSDL` stops on failure; `main` returns 1.
  - [x] Dead code removed after checking references: `ThreatObject.*`, `IKeyHandler.h`, the `#if 0` block, 11 unused `Skill` locals, unused `GameManager` members.
- **Deferred (not done in Phase 1, tracked here):** ownership cleanup of the 6 `Keyboard` objects and of enemies, a working `Close()` (enemy lifecycle lands in Phase 3; the rest in Phase 4); enemies never being removed (Phase 3).
- **Completion criteria:** Debug/Release x64 build from a clean copy at another path; game behaves as before. ☑

---

## Phase 2 — Invoker Core Extraction  *(DONE)*
- **Objective:** put the Invoker mechanic in a platform-independent Core so Practice, Combat, Story and touch input can build on it.
- **Delivered:**
  - [x] `Three Elements/Core/Invoker.h/.cpp` (namespace `invoker`, no SDL): `InputAction`, `Orb`, `Slot`, normalised `Recipe`, `SkillId`/`SkillDefinition` catalog of the 10 skills (id, recipe, name, icon path), `InvokerState` (orbs, D/F, `Apply`/`AddOrb`/`Invoke`/`Cast`/`Reset`).
  - [x] `MainPlayer` reduced to sprite + SDL key → `InputAction` mapping; `Skill` reduced to an icon sprite; `GameManager` draws HUD from Core state. All legacy Invoker logic removed (no duplicate).
  - [x] `Cast D/F` console logs kept as a development aid (no gameplay effect yet).
  - [x] Tests: `Tests/` project `InvokerCoreTests` (243 checks incl. a 300 000-key randomised regression against the original algorithm); run with `Tests\run_tests.cmd` (see `Tests/README.md`).
- **Not done on purpose:** key auto-repeat filtering (behaviour change → Phase 3), any gameplay effect of D/F.
- **Completion criteria:** Debug/Release x64 build; tests pass; in-game behaviour identical to Phase 1. ☑

---

## Phase 3 — Practice Gameplay  *(IMPLEMENTED — awaiting owner review)*
- **Objective:** the playable practice loop of GAMEPLAY_SPEC.md: one enemy at a time, correct/wrong cast, leak, HP 3, Game Over, restart, score/combo/accuracy/survival time, time-based difficulty.
- **Delivered** (rule IDs refer to the spec):
  - [x] 3.1 **Practice layer without SDL** (`Three Elements/Practice/`): `EnemyDefinition` table (10 entries, `targetSkill`, sprite/frames, `bodyLeft`/`feetRow` for ground alignment, `speedMultiplier`), `PracticeSession` (Ready/Playing/GameOver, HP, score, combo, best combo, accuracy counters, survival time, one active enemy with a logical position, owns the `InvokerState`), `DifficultyAt(elapsed) → {enemySpeed, challengeDelay}`, own deterministic RNG seeded per session, no immediate repeat of the same target skill (C-3).
  - [x] 3.2 **Cast judging** (§9–§11): correct → enemy removed, +1 score, +1 combo; wrong → only the accuracy counters change, the enemy keeps coming; empty-slot casts and casts with no active enemy are ignored; leak → HP −1, combo 0, enemy removed; HP ≤ 0 → Game Over; `Start()` resets Core and Practice and keeps assets loaded.
  - [x] 3.3 **Enemy data and lifecycle:** 10 sprites (7 existing + `dark_wiz`, `kitsune_run`, `knight_run`; frame counts checked visually), each sheet loaded once, aligned to a common ground line, single active enemy; the old free-running spawn/wrap-around code was removed (this also removes the enemy leaks).
  - [x] 3.4 **Loop changes Practice needs:** real `dt` from `SDL_GetTicks` (clamped inside Practice), time-based enemy movement, key auto-repeat ignored (spec Q-7). The 25 FPS cap, per-frame background scrolling and per-call enemy animation are unchanged.
  - [x] 3.5 **HUD:** HP, score, combo, best combo, accuracy ("--" until the first judged cast), survival time, orbs, D/F slots; Ready and Game Over screens; Enter starts/restarts, Esc quits. Text uses a small built-in 5×7 pixel font (`PixelText`) because SDL2_ttf is not integrated (no font asset, not linked). No hints: nothing shows the target or recipe; a development-only `--debug` command-line switch prints/shows the target.
  - [x] 3.6 **Tests:** `Tests/PracticeTests` (280 checks) next to the unchanged Core tests (243); `Tests\run_tests.cmd` runs both.
- **Deviations / notes for review:**
  - Best Combo is **per session and reset by Restart** and nothing is persisted, as requested for Phase 3. Spec §13 (persistent Best Score / Best Combo / Best Survival Time that Restart does not reset) is still to be implemented in Phase 6.
  - Initial challenge delay is 1.5 s, first spawn included; enemy speed starts at 125 px/s and grows 2.5 px/s per second up to 380 px/s; delay shrinks 0.01 s per second down to 0.5 s. All in `Practice.h`, tune by playtesting.
  - The sprite ↔ skill table in `Practice.cpp` is an arbitrary one-to-one assignment; change `targetSkill` there to remap.
- **Dependencies:** Phase 2.
- **Not done on purpose:** combat, story, hints, cooldowns, multi-target, HUD polish/effects/audio, persistent best stats (Phase 6), CMake, Web, touch/mobile.
- **Completion criteria:** a full session is playable on Windows as the spec describes (checked with scripted play on Debug and Release x64); Practice tests pass; Core and Practice contain no SDL includes. ☑ (owner review pending)

---

## Future phases (high-level placeholders — detail them when they become "next")

### Phase 4 — Game Loop / Input / State readiness
Finish what Web and mobile need beyond Phase 3: central asset-path helper, logical resolution/scaling decision, remaining ownership clean-up (`Keyboard`s, `Close()`), input mapping kept behind the logical `InputAction` seam so touch can plug in. *Gate:* the frame step can be driven by an external loop.

### Phase 5 — Web MVP (GitHub Pages)
Minimal `CMakeLists.txt` (desktop + Emscripten) beside the VS project, Emscripten build with the SDL2/SDL2_image ports (single-threaded, preloaded assets, `emscripten_set_main_loop`), canvas scaling with nearest filtering, publish static output to GitHub Pages with a documented build command. *Gate:* public URL plays start → game over → restart in current Chrome/Firefox.

### Phase 6 — UX / Audio / Game Feel
Text rendering and real HUD, start and Game Over screens, **persistent local best stats** (browser local storage on web, file on desktop), success/fail/leak feedback, SDL_mixer audio (web audio unlock), pixel-art polish (integer scaling, background aspect). No hints in normal play.

### Phase 7 — Android
SDL Android project, six keyboard-like touch buttons Q/W/E/R/D/F firing on touch-down through `InputAction`, landscape/safe-area layout, lifecycle and renderer-reset handling, latency check on a real device. iOS is out of scope.

### Phase 8 — Future Combat / Story  *(deliberately not started)*
Design note only, when the MVP has proven fun: Threne story, Evil King, hero roles, assign-cards screens, real skill effects/damage, bosses, multi-target hard modes, beginner-mode hints. Builds on Core; no code before this phase is explicitly opened.

### Phase 9 — PC / Steam  *(conditional)*
Only if the game proves worthwhile: IP review of names/icons/assets (currently "free to use" per the owner, not documented in-repo), Windows packaging, Steamworks, store assets, options menu.

---

## Progress log
- 2026-09-21 — **Phase 0** done (audit, gameplay spec v2, plan).
- 2026-09-21 — **Phase 1** done: build portable, UB and spawn-timer bugs fixed, dead code removed (branch `phase-1-stabilization`, commit `eb25131`).
- 2026-09-21 — **Phase 2** done: Invoker Core extracted, legacy logic removed, `Tests/InvokerCoreTests` added (243 checks) (branch `phase-2-invoker-core`). Next: Phase 3.
- 2026-09-21 — **Phase 3** implemented: Practice layer + tests (280 checks), SDL integration, HUD, Ready/Game Over screens, `--debug` switch. Awaiting owner review; not committed. Next: Phase 4 (only after approval).
