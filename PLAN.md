# PLAN.md — Three Elements roadmap

Based on the state recorded in [PROJECT_AUDIT.md](PROJECT_AUDIT.md) (2026-09-21, HEAD `1c81f62`). Read that file for file/line references and bug IDs (B1…B17).

> **Precedence note (updated with GAMEPLAY_SPEC.md v2):** [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md) is the source of truth for gameplay rules; where it conflicts with this file, the spec wins. Gameplay decisions D1–D5 in the "Open decisions" table below are **all resolved by the spec** (one active enemy at a time; enemies carry a data-driven `targetSkillId`; **no hints** in the MVP, so ignore the recipe/name-hint and per-enemy-hint items in Phase 5.1; HP = 3; text rendering is an implementation choice). **Persistent local best stats** (Best Score / Best Combo / Best Survival Time) are part of the MVP spec; build them after the session logic works (Phase 5.2 timing is fine). Split the code into **Core / Practice / Presentation** (spec §20) and read Phase 2 through that split. This file's phase structure and scope limits still apply.

## Product direction (fixed for this plan)
Invoker-style **practice game**, endless session. Trains: input speed, quick skill switching, recipe memory, choosing the right skill, keeping combos, reaction time.
Core loop: **enemy appears → it corresponds to one of the 10 skills → player recalls the recipe → Q/W/E → R → D/F → correct spell removes the enemy.** Wrong spell: no damage, enemy keeps coming, and if it reaches the player, HP goes down. Game over at HP 0.
MVP facts: all 10 skills from the start; 10 enemy types ↔ 10 skills; Q/W/E order irrelevant (already true); no cooldowns; difficulty rises with time; score/combo/best combo/basic stats; pixel art, side view. Targets: **Web first (GitHub Pages)** → Android → maybe PC/Steam. Touch UI = Q/W/E/R/D/F buttons laid out like a keyboard.

## Ground rules for every phase
- Smallest viable change; preserve working code (see "do not rewrite" list in the audit §17). Keep SDL2 + SDL2_image. No new engine/framework, no ECS, no scene graph, no DI container.
- One phase = one or more small commits; the game must build and start after each commit.
- Do not start Combat or Story work before Phase 7.
- Keep `CLAUDE.md`, `PROJECT_AUDIT.md` and this file's checkboxes up to date when a phase completes.

## Sequencing note
Phases are listed in the requested order. Two dependencies are worth knowing:
- Phase 2 code lands in "update" and "render" code paths; the mechanical loop split in Phase 3 (item 3.1) is cheap and low-risk and **can be done right after Phase 1** if you prefer to write Phase 2 code directly into the new structure. Phase 3 does not depend on Phase 2.
- Phase 4 needs 3.1 (loop split) and 3.3 (delta-time) — do not start Phase 4 before they are done.

## Open decisions (answer before the phase that needs them)
| # | Decision | Needed by | Default if unanswered |
|---|---|---|---|
| D1 | With several enemies on screen, which one does a cast target? | Phase 2 | Nearest enemy to the player only; if its skill matches → removed, otherwise nothing happens |
| D2 | How does the player know which skill an enemy needs? (memorise by enemy look / show skill icon or name above the enemy / hint toggle) | Phase 2 (data) / Phase 5 (UI) | Memorise by appearance; optional hint toggle in Phase 5 |
| D3 | Player HP and damage per leaked enemy | Phase 2 | 5 HP, 1 per leaked enemy |
| D4 | Which 10 of the 13 enemy sheets, and which enemy ↔ which skill | Phase 2 | Use the 7 current + 3 unused (dark_wiz, mush, kitsune_run) after visual check |
| D5 | Text rendering: bitmap font vs SDL_ttf | Phase 2 (stats) / 5 | Window-title/console debug output in Phase 2; decide font in Phase 5 |
| D6 | Logical resolution for web/mobile (keep 928×544?) | Phase 4 | Keep 928×544 with letterboxing |
| D7 | Keycodes vs scancodes (non-QWERTY layouts) | Phase 3 | Keycodes (letters), revisit if reported |

---

## Phase 0 — Audit  *(DONE in this session)*
- **Objective:** understand the repo and constraints.
- **Scope:** read all source/build/assets; write `PROJECT_AUDIT.md` and `PLAN.md`.
- **Dependencies:** none.
- **Not to do:** any code change, any build that rewrites tracked binaries.
- **Completion criteria:** both documents exist and are reviewed by the owner. ☑

---

## Phase 1 — Stabilization
- **Objective:** a clean, portable, leak-free baseline with **unchanged gameplay**.
- **Exact scope:**
  - [ ] 1.1 Build portability: replace absolute paths in the vcxproj with `$(SolutionDir)`-relative ones; configure `Release|x64` (include/lib/libs); remove the stale `C:\Users\…` and `E:\…` entries; set the debugger working directory so `assets/` resolves.
  - [ ] 1.2 `git rm --cached` all tracked build outputs (`x64/`, `Three Elements/x64`, `Three Elements/Debug`, `*.user`); keep DLLs needed to run documented in README/CLAUDE.md (copy step or post-build event).
  - [ ] 1.3 Fix B1 (`printf` with `std::string` → `.c_str()` or remove the debug print) and the "neededGGG" comment typo.
  - [ ] 1.4 Fix B5: initialise `lastRespawnTime`, remove shadowing locals, drop the redundant timing check; fix B6 (`srand`, includes `<cmath>/<cstdlib>`).
  - [ ] 1.5 Ownership (B7): remove the 11 unused `Skill` locals and the unused `Keyboard` list; store the 6 keyboards/10 skills so they are freed; implement a working `Close()`; delete enemies on exit. Keep public behaviour identical.
  - [ ] 1.6 B8–B10, B11: reset/scope `m_KeyRActive`, don't draw the texture-less `m_player.skill`, fix `activeSpell`, remove members that shadow `BaseObject` fields in `EnemyObject`/`Skill` (behaviour must stay the same).
  - [ ] 1.7 Delete dead code: `ThreatObject.*`, `IKeyHandler.h`, the `#if 0` block, unused members listed in audit §13; drop them from the vcxproj.
  - [ ] 1.8 Log SDL/asset load failures (`SDL_GetError`) instead of failing silently; return a non-zero exit code on init failure.
- **Dependencies:** none.
- **Do NOT do yet:** enemy removal/kill logic, D/F casting, key-repeat filtering (behaviour changes → Phase 2/3), CMake, loop restructuring, renaming public APIs, texture cache.
- **Completion criteria:** `Debug|x64` and `Release|x64` both build from a fresh clone at any path; game starts and behaves as before (Q/W/E icons, R fills D/F, parallax, enemies spawning); no `SDL_RenderCopy` errors logged per frame; running ~5 min shows no growing memory *(manual check)*; `git status` is clean after a build.

---

## Phase 2 — Practice Gameplay Core
- **Objective:** the playable practice loop: enemy ↔ spell, cast, remove or leak, HP, score.
- **Exact scope:**
  - [ ] 2.1 Data table of 10 enemy types `{spritePath, frameCount, spellKey}` (decisions D2/D4); confirm frame counts of the added sheets visually; anchor sprites to a common ground line using their frame height.
  - [ ] 2.2 Enemy stores its `spellKey`; spawn picks any of the 10 (duplicates allowed); load each texture once and share it (small cache), not per spawn.
  - [ ] 2.3 Handle **D and F**: cast the spell in that slot (recipe already resolved by R). Apply D1: match → remove the targeted enemy; mismatch → nothing.
  - [ ] 2.4 Enemy lifecycle: remove on kill and when it reaches the player (hook at `Enemy.cpp:19-23`); no more wrap-around; proper deletion.
  - [ ] 2.5 Player HP (D3); leaked enemy → HP−1; HP 0 → simple `GameOver` flag and restart key.
  - [ ] 2.6 Difficulty ramp: spawn interval and enemy speed as functions of elapsed time (clamped).
  - [ ] 2.7 Stats data only: score, current combo, best combo, kills, wrong casts, leaks, per-kill reaction time (spawn→kill). Output via window title/console (D5) — no HUD yet.
- **Dependencies:** Phase 1 complete. D1–D5 answered (defaults available).
- **Do NOT do yet:** HUD/menus/fonts, audio, animations/effects, cooldowns, combat mode, story/cards, touch UI, web build, refactoring beyond what the items need.
- **Completion criteria:** a full session can be played on Windows: enemies of all 10 types appear, the correct recipe (any Q/W/E order) + R + D/F removes it, a wrong spell does nothing, leaks reduce HP, HP 0 ends the game, score/combo/stats are visible via title/console, difficulty visibly increases, no crashes or leaks during a 10-minute session.

---

## Phase 3 — Game Loop / Input / State
- **Objective:** make the code web/mobile-ready without changing gameplay: frame-step structure, delta-time, one logical input path, tiny state model.
- **Exact scope:**
  - [ ] 3.1 Split `LoopGame` into init / per-frame step (events → update(dt) → render) / shutdown by moving existing code (no redesign). *(may be done right after Phase 1)*
  - [ ] 3.2 Logical-key seam: a single entry "player receives key Q/W/E/R/D/F" fed from one SDL→logical mapping; **ignore `key.repeat`** (B2); record a timestamp per press; `MainPlayer` no longer depends on `SDL_Event`. (D7)
  - [ ] 3.3 Delta-time: enemy speed in px/s, spawn/ramp timers in ms, enemy animation by time; keep the player's existing time-based animation; decouple from the 25 FPS cap (keep the cap only for desktop if desired).
  - [ ] 3.4 Minimal state enum (`Playing`, `GameOver`, optionally `Paused`) inside `GameManager`; restart resets stats/enemies without reloading assets.
  - [ ] 3.5 Central asset-path helper (single prefix) and single recipe table (remove per-`Skill` copies).
- **Dependencies:** Phase 1 (3.1/3.3 could precede Phase 2 by choice; 3.4 needs Phase 2's game-over).
- **Do NOT do yet:** touch/mouse UI, gamepad, menus, scene manager or ECS, threading, changing rendering resolution/filtering, Emscripten specifics.
- **Completion criteria:** game plays identically to end of Phase 2 but: holding a key does not repeat input; movement speed is the same at any frame rate (test by changing the cap); restart works repeatedly; `MainPlayer` has no SDL event dependency; the frame step can be called from an external loop.

---

## Phase 4 — Web MVP (GitHub Pages)
- **Objective:** the game runs in a browser from a GitHub Pages URL.
- **Exact scope:**
  - [ ] 4.1 Add a minimal `CMakeLists.txt` (desktop + Emscripten) alongside the VS project (VS project stays supported).
  - [ ] 4.2 Emscripten build with the SDL2 and SDL2_image (PNG) ports; single-threaded (no SharedArrayBuffer); `--preload-file assets`; loop driven by `emscripten_set_main_loop`.
  - [ ] 4.3 Compile-portability fixes found by clang (includes, warnings).
  - [ ] 4.4 Canvas scaling: logical size (D6), nearest-neighbour filter, letterboxing; keyboard focus on canvas; verify Q/W/E/R/D/F work, no browser shortcuts triggered.
  - [ ] 4.5 Publish: static output (`index.html`, `.js`, `.wasm`, `.data`) served from GitHub Pages sub-path; a documented build/deploy command (a GitHub Actions workflow is optional).
- **Dependencies:** 3.1, 3.2, 3.3, 3.5; Phase 2 playable.
- **Do NOT do yet:** audio, mobile/touch, PWA/offline, WebGL customisation, threads, a JS wrapper UI, performance tuning beyond obvious problems.
- **Completion criteria:** public URL loads in current Chrome and Firefox desktop, game plays end to end (start → game over → restart), stable ≥ 30 FPS, no console errors, assets load from the sub-path; build instructions reproducible by a fresh clone.

---

## Phase 5 — UX / Audio / Game Feel
- **Objective:** make the practice loop readable and satisfying.
- **Exact scope:**
  - [ ] 5.1 Text rendering (D5) and a HUD: HP, score, combo, best combo, current Q/W/E orbs, D/F slots with recipe/name hint; optional per-enemy skill hint toggle (D2).
  - [ ] 5.2 Start screen and Game Over screen with session stats (accuracy, avg/best reaction time, best combo, kills); best score persisted (localStorage on web / file on desktop).
  - [ ] 5.3 Feedback: cast success/fail cue, enemy hit/disappear effect, leak flash, key-press highlight (uses the 2-frame key icons), simple player cast pose.
  - [ ] 5.4 Audio via SDL_mixer (web audio unlock on first input): SFX first, music optional.
  - [ ] 5.5 Pixel-art polish: integer scaling, background aspect (B14), enemy ground alignment.
- **Dependencies:** Phase 2, 3; Phase 4 for web verification.
- **Do NOT do yet:** options/settings menus beyond mute/hint, localisation, achievements, combat mechanics, story, cards, new enemy types beyond the 10.
- **Completion criteria:** a new player can understand HP, score, combo and the required recipe without external explanation; stats screen shows real numbers; sound plays on web after first key press; no regression in input latency (subjectively responsive, no dropped presses).

---

## Phase 6 — Android
- **Objective:** playable on Android phones with on-screen keyboard-like buttons.
- **Exact scope:**
  - [ ] 6.1 SDL Android project (Gradle/NDK) building the same sources; central asset prefix works from APK assets.
  - [ ] 6.2 Touch controls: six buttons Q/W/E/R/D/F arranged like a keyboard cluster, thumb-sized, multi-touch, triggering on **touch-down** through the logical-key seam (3.2).
  - [ ] 6.3 Landscape lock, letterboxed/safe-area layout, lifecycle handling (pause/resume), texture/renderer reset handling.
  - [ ] 6.4 Performance and latency check on at least one mid-range device.
- **Dependencies:** Phase 3 (input seam, delta-time), Phase 4 (CMake/portable code), Phase 5 recommended (HUD readable on small screens).
- **Do NOT do yet:** iOS, Play Store listing/monetisation, cloud saves, gamepad support, in-app settings, new gameplay.
- **Completion criteria:** installable APK plays a full session on a real device; fast repeated taps and simultaneous presses register reliably; no crash on background/foreground; readable at phone size.

---

## Phase 7 — Future Combat / Story  *(deliberately not started)*
- **Objective:** capture direction only; no implementation.
- **Exact scope:** write a separate design note if/when the MVP has proven fun. Candidates from the Figma prototype: Threne story, Evil King, hero roles (Hammer/Knight/Healer/Dragon), assign-cards screens, real skill effects/damage, bosses.
- **Dependencies:** MVP validated by real play (Phases 2–5); decision to expand.
- **Do NOT do:** any code, assets or UI for combat/story before this phase is explicitly opened.
- **Completion criteria (of the design note):** scoped, prioritised, and compatible with the practice loop (does not slow it down).

---

## Phase 8 — PC / Steam  *(conditional)*
- **Objective:** ship a polished Windows build on Steam, only if the game proves worthwhile.
- **Exact scope:**
  - [ ] 8.1 Intellectual-property review before release: asset licence table (currently "free to use" per owner, not documented in-repo), skill/element names and icons, game name availability; re-skin where needed.
  - [ ] 8.2 Windows packaging (Release x64, bundled DLLs/assets, installer or zip), Steamworks integration (achievements/leaderboards optional), store page assets.
  - [ ] 8.3 Options menu (keybinds, resolution/fullscreen), controller policy.
- **Dependencies:** Phases 2–5 complete and playtested; IP decisions.
- **Do NOT do yet:** any of it before Web MVP feedback exists.
- **Completion criteria:** release candidate passes a clean-machine install test; IP checklist signed off by the owner; store requirements satisfied.

---

## Progress log
- 2026-09-21 — Phase 0 done (audit + plan written). Next: Phase 1.
