# PLAN.md — Three Elements roadmap

Based on [PROJECT_AUDIT.md](PROJECT_AUDIT.md) (2026-09-21 snapshot of the code before Phase 1; read it for file/line references and bug IDs B1…B17) and [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md).

> **Precedence:** [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md) v2 is the source of truth for gameplay rules; where it conflicts with this file, the spec wins. It has **no blocking open decisions**; tunables (starting speed/delay, hit-line position, sprite ↔ skill table, restart key) are data/constants chosen at implementation time. **Recipe hints are excluded** from normal play; the only exception is the off-by-default RECIPE HINT option (spec §17 H-1..H-4), whose runs are not ranked. **Persistent local best stats** (Best Score / Best Combo / Best Survival Time) are part of the MVP; build them once the session logic works.

## Status

| Phase | Title | Status |
|---|---|---|
| 0 | Audit & Gameplay Specification | **Done** |
| 1 | Stabilization | **Done** (branch `phase-1-stabilization`) |
| 2 | Invoker Core Extraction | **Done** (branch `phase-2-invoker-core`) |
| 3 | Practice Gameplay | **Committed (`a76802e`) + review round (`fix: refine practice session lifecycle`); awaiting owner review** |
| 4 | Game Loop / Input / State readiness | **Mostly done** (frame step, dt-based animation; asset-path helper and ownership clean-up left) |
| 5 | Web MVP (GitHub Pages) | **Mostly done** (live at http://3elements.relifes.net, `web/build.sh`; no CMake yet, HTTPS cert pending) |
| 6 | UX / Audio / Game Feel | **Mostly done** (records, feedback, sound effects, code-drawn skill effects; tuning deferred to the difficulty backgrounds, HUD polish left) |
| 7 | Android | **In progress** (native SDL2 project in `android/`; owner goal: debug APK on own phone, not Google Play yet) |
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
  - [x] 3.5 **HUD:** HP, score, combo, best combo, accuracy ("--" until the first judged cast), survival time, orbs, D/F slots; Ready and Game Over screens; Enter starts/restarts, Esc steps back (Playing → Ready, Game Over → Ready) and quits only from Ready. Text uses a small built-in 5×7 pixel font (`PixelText`) because SDL2_ttf is not integrated (no font asset, not linked). No hints: nothing shows the target or recipe; a development-only `--debug` command-line switch prints/shows the target.
  - [x] 3.6 **Tests:** `Tests/PracticeTests` (328 checks after the review round) next to the unchanged Core tests (243); `Tests\run_tests.cmd` runs both.
- **Deviations / notes for review:**
  - **Best Combo (review decision):** the current combo resets with every new session, the Best Combo record is kept across restarts while the application runs and only rises on a new record. Nothing is saved to disk yet; Best Score / Best Survival Time and saving to disk (local storage / file) remain for Phase 6 (spec §13).
  - **Controls (review decision):** Ready: Enter starts, Esc quits. Playing: Esc returns to Ready (session stopped and reset, record kept), Enter ignored. Game Over: Enter starts, Esc returns to Ready. No pause. The Enter/Esc rules live in `PracticeSession::PressEnter/PressEscape` (tested); `GameManager` only maps the SDL keys.
  - **Difficulty values are initial MVP tuning values, not final or balanced:** challenge delay 1.5 s → 0.5 s (first spawn included), enemy speed 125 → 380 px/s (+2.5 px/s per second survived), delay −0.01 s per second. Bounded above/below and `dt` is clamped to 0.1 s; all in `Practice.h`.
  - The sprite ↔ skill table in `Practice.cpp` is an arbitrary one-to-one assignment; change `targetSkill` there to remap.
- **Dependencies:** Phase 2.
- **Not done on purpose:** combat, story, hints, cooldowns, multi-target, HUD polish/effects/audio, persistent best stats (Phase 6), CMake, Web, touch/mobile.
- **Completion criteria:** a full session is playable on Windows as the spec describes (checked with scripted play on Debug and Release x64); Practice tests pass; Core and Practice contain no SDL includes. ☑ (owner review pending)

---

## Future phases (high-level placeholders — detail them when they become "next")

### Phase 4 — Game Loop / Input / State readiness  *(mostly done)*
- [x] Frame step: `GameManager::RunFrame()` (input, rules, drawing for one frame); `LoopGame()` drives it with a capped `while` loop on desktop (60 FPS) and `emscripten_set_main_loop_arg` on the web. *Gate met.*
- [x] Everything that moves or animates runs on real `dt` (enemy walk cycle `ENEMY_ANIM_FPS`, parallax `BACKGROUND_LAYER_SPEED`, same speeds as the old 25 FPS per-frame code); `dt` capped at 0.1 s.
- [x] Touch input through the same `InputAction` path (`HandlePointerDown`).
- [x] Logical resolution: fixed 928×544 canvas, letterboxed by CSS on the web.
- [ ] Central asset-path helper; remaining ownership clean-up (`Keyboard`s, `Close()`).

### Phase 5 — Web MVP (GitHub Pages)  *(mostly done)*
- [x] Emscripten build with the SDL2/SDL2_image ports, preloaded assets, browser-driven main loop (no ASYNCIFY): `web/build.sh`.
- [x] `web/shell.html`: canvas scaled with `image-rendering: pixelated`, phone landscape layout, fullscreen button (Android) / Add-to-Home-Screen help (iPhone), `manifest.webmanifest`.
- [x] Published to GitHub Pages from the `gh-pages` branch (`web/build.sh --deploy`, which also verifies the live files). *Gate met:* start → game over → restart plays in current Chrome, desktop and phone.
- [x] Mobile touch controls (six keyboard-style buttons, touch devices only) — pulled forward from Phase 7 for the web build.
- [ ] HTTPS for the custom domain (GitHub has not issued the certificate yet; owner action in Settings → Pages).
- [ ] Optional: `CMakeLists.txt` beside the VS project (the build script is enough for now).

### Phase 6 — UX / Audio / Game Feel  *(in progress)*
- [x] **Persistent local best stats** (Best Score / Best Combo / Best Survival Time): `practice::MergeBests` + `RestoreBestCombo` (tested), saved to `bests.txt` / `localStorage`, shown on Ready and Game Over with "NEW BEST!".
- [x] **Success / fail / leak feedback** (light): gold ring + "+1", red tint + "MISS", red frame + short shake + blinking HP square.
- [x] Sound effects (`Audio.*`): synthesised in code at start-up, mixed in an SDL audio callback, no SDL_mixer and no sound files; web audio unlocked on the first gesture; SOUND ON/OFF button + M key, remembered.
- [x] Code-drawn effects for the 8 skills that used TEST placeholder sheets (`SkillVfx.*`, `Draw.*`).
- [x] Web visitor counter (`web/shell.html`, Abacus API, one count per browser).
- [-] Background music: **not wanted** (owner, 2026-09-29).
- [ ] Difficulty tuning: **deferred** (owner, 2026-09-29: current values feel fine). Do it together with the planned difficulty-dependent backgrounds.
- [x] Pixel-art polish: nearest-neighbour scaling, background no longer squashed (unscaled band, ground unchanged).
- [x] New skill icons (owner art, processed by `art/make_skill_icons.py`), icon next to the target name.
- [x] Recipe reference on Ready / Game Over (H or button), never during play (spec §17).
- [x] Tutorial mode (spec §24): `TutorialSession` (tested) + guided overlay with highlights, reachable from Ready (T).
- [ ] Text rendering / HUD polish. No recipe hints in normal play.

### Phase 7 — Android  *(in progress)*
Owner decisions (2026-09-30): native SDL2 (not a web wrapper), debug APK installed on the owner's own phone first, tested on a real device.
- [x] Gradle project `android/` from SDL 2.32's template (AGP 8.7.3 / Gradle 8.9 / JDK 21), SDL sources fetched by `android/fetch_deps.sh` (not in git), `app/jni/CMakeLists.txt` building SDL2, SDL2_image (PNG via stb, BMP) and the game as `libmain.so`.
- [x] Code: logical 928×544 rendering with letterbox (`SDL_RenderSetLogicalSize`, all platforms), `TouchToGame`, landscape hint, touch buttons on from start, Back = Esc, save files in `SDL_GetPrefPath`, records saved when the app goes to the background.
- [x] Launcher icon (now made by `art/make_injoker.py` from the Injoker art).
- [x] First APK builds; layout / touch / Back checked on the emulator (20:9, 16:9, 4:3).
- [x] Google Play release build: signed AAB (upload key outside git), targetSdk 36, 16 KB pages, adaptive icon, `appCategory=game`, predictive-back opt-out; store listing, declarations, feature graphic and screenshots in `art/store/`; privacy policy, terms and support pages (`/policy`, `/terms`, `/support`); version 1.1.0 (recipe hint) ready for the closed test.
- [ ] Owner: create the app in Play Console, fill the declarations, upload the AAB to a closed test, 12 testers × 14 days, then apply for production.
- [ ] Later (only if wanted): release signing, Google Play listing. iOS is out of scope.

### Online leaderboard — owner decisions 2026-09-30  *(UI done with this device's data; online part not started)*
- Rank by **survival time**; top 3 on the menu, top 10 on the LEADERBOARD screen (done, `practice::InsertTopRun`).
- **Android: Google Play Games Services** sign-in and leaderboard (weekly by default + all-time). Needs the app created in Play Console (the owner has a developer account since 2026-09-30).
- **Web: a server of our own** (to choose: hosting, how web players are identified, anti-cheat basics). Note: two separate boards unless the server also serves Android.

### Combo / Boss mode — owner idea 2026-09-30  *(implemented 2026-09-30 as version **1.2.0**; spec GAMEPLAY_SPEC.md §25)*
A separate mode on top of Practice: a boss appears and only takes damage from a real Invoker **combo executed with Dota-like timing**, e.g. Tornado lifts it → Chaos Meteor / Sun Strike timed to land as it comes down → Deafening Blast pushes it back.

**Discussion 2026-09-30** (the owner then said "triển khai 2 3 4": all proposals accepted as written; they became spec §25, point 8 was folded into boss 1 as its CAST NOW cue):
- **Timing is the core.** Every spell gets a cast→impact timeline in Boss mode only; **Practice stays instant** (no rule change). Reference values from Dota (max level, to re-check when writing the spec; all in one tunable table): Tornado lift ~2.5–2.9 s, boss **invulnerable while airborne**, falls where it was lifted · Sun Strike lands after **1.7 s** at the boss's position at cast time · Chaos Meteor lands after **1.3 s**, then rolls forward and burns · EMP detonates after **2.9 s** · Deafening Blast travels and knocks back · Cold Snap short stun per hit · Ice Wall slow · Forge Spirit / Alacrity / Ghost Walk: little role in v1 combos.
- **The skill trained:** cast the delayed spells early so they land the moment the boss touches down (landing while airborne = miss), and invoke 3–4 spells with only two D/F slots (pre-invoke two, invoke the rest during the lift). No cooldowns needed.
- **Proposed decisions (owner to answer point by point):**
  1. Show the required combo as spell names in order (e.g. `TORNADO → EMP → METEOR → BLAST`), like the TARGET hint; keys only with RECIPE HINT (same not-ranked rule).
  2. Judging: **(a) fixed combos** — the boss loses HP only when the whole combo is done in order and on time — for 1.2; (b) free simulation where any hit deals damage, later.
  3. Targeting: spells land at the boss's position at cast time (no free aiming in v1; timing is the skill).
  4. Failure: wrong order / bad timing resets the combo, the boss keeps walking; boss reaching the player = HP −1 and the boss is knocked back (not removed); HP 0 = defeat.
  5. Three bosses, one combo each, rising difficulty: Tornado → Sun Strike; Tornado → Chaos Meteor → Deafening Blast; Tornado → EMP → Chaos Meteor → Deafening Blast. Each needs the combo ~3 times.
  6. Boss art: owner-made sprites, or scaled/tinted existing enemy sprites (knight, dark_wiz) + HP bar as a first version.
  7. Menu line **BOSS**, all three selectable (no unlocks), best kill time per boss saved locally; a separate leaderboard later.
  8. A short guided lesson "Tornado → Sun Strike", like the Sun Strike tutorial lesson.
- **Implementation order:** spec section in GAMEPLAY_SPEC → `ComboSession` in the Practice layer (pure timeline simulation, unit-tested, like `TutorialSession`; Practice and Core untouched) → presentation (lifted/falling boss, delayed VFX reusing `SkillVfx`, boss HP bar, impact countdown markers) → PC + web + Android + docs in one round. Larger than the tutorial.
- Later: **items** (Refresher, Eul's, Blink...). Refresher only makes sense with cooldowns, which the game does not have: a design decision for then.
- Pulls part of Phase 8 (combat) forward, by owner decision.

### Next steps — planned 2026-10-01  *(owner: "1 done. 2 đồng ý … 3 … lên kế hoạch")*

Status: 1.3.7 is uploaded to the Google Play closed test by the owner (14-day clock running). Every later version is uploaded to the same test track (versionCode +1). Other open items (stage backgrounds, tutorial card for PLAY, online boards, enemy art) wait until these two are done.

#### 1.4.0 — Shop and items (PLAY only)  *(done 2026-10-01, spec §27)*

Owner decisions: items only work in PLAY; mix of permanent and consumable items; **6 slots in 2 rows of 3, like the Dota 2 inventory**, used with the **right hand** while the left hand types Q/W/E/R/D/F; every item has a **cooldown suited to the game** and its own **upgraded version at a higher price**; prices high (grinding); gold only from elites, bosses and Bounty; simple pixel icons in the spirit of the Dota items (drawn from scratch, not Valve art); no real-money purchases.

| Step | Work | Output |
|---|---|---|
| A1 | Spec §27 Shop: item table (base + upgrade, effect, cooldown, price), slots, keys, buying / upgrading / equipping, what happens on Game Over, save format | GAMEPLAY_SPEC.md §27, owner confirms |
| A2 | Practice layer: `ItemDefinition` table, `Inventory` (owned level per item, consumable counts, 6 equipped slots, gold spending), item use + cooldowns + effects inside the PLAY session (pure, `dt`-driven) | `Practice/Items.*`, tests |
| A3 | Shop screen: menu line SHOP (Home), item grid with icon, level, price, BUY / UPGRADE, and the 2 x 3 loadout to equip | GameManager |
| A4 | In-game item bar: 2 x 3 on the right (desktop keys, phone buttons for the right thumb), cooldown sweep, consumable counts, a flash when used | GameManager |
| A5 | Pixel icons for every item and its upgrade (`art/make_item_icons.py`) | `assets/items/` |
| A6 | Saving: inventory + loadout next to the gold (`items.txt` / localStorage), privacy page updated | — |
| A7 | Release on PC / web / Android, docs, store text | 1.4.0 |

Proposed item table (to confirm in A1; effects are this game's, not Dota's):

| Item | Base: effect · cooldown · price | Upgrade: name · effect · cooldown · price |
|---|---|---|
| Blink Dagger | enemies walk back 3 s · 40 s · 1500 | Swift Blink · 4 s · 30 s · 4000 |
| Refresher Orb | the current chain loses 2 skills (at least 1 is left) · 90 s · 3000 | Refresher Orb II · same · 60 s · 7000 |
| Eul's Scepter | the enemy stands still 2 s · 25 s · 1200 | Wind Waker · 3 s and pushed back · 20 s · 3500 |
| Black King Bar | no life lost for 5 s · 120 s · 2500 | BKB II · 7 s · 90 s · 6000 |
| Hand of Midas | passive: +50 % gold · — · 2000 | Midas II · +100 % gold · — · 6000 |
| Octarine Core | passive: item cooldowns −25 % · — · 3500 | Octarine II · −40 % · — · 8000 |
| Aghanim's Scepter | passive: choose 1 of 2 runes after a boss · — · 4000 | Aghanim's Blessing · choose 1 of 3 · — · 9000 |
| Healing Salve (consumable) | +1 life · — · 60 each | Cheese · +2 lives · — · 300 each |
| Smoke of Deceit (consumable) | enemies 50 % speed 8 s · — · 80 each | Greater Smoke · 12 s · — · 200 each |

Keys proposed: **U I O / J K L** (the 2 x 3 grid under the right hand), numpad 7 8 9 / 4 5 6 as well; on phones a 2 x 3 block of buttons on the right edge (the Q/W/E/R/D/F cluster is on the left).

#### 1.5.0 — Boss Fights: long combos with all 10 skills  *(rules and placeholder art done 2026-10-01, spec §25 B-16..B-20; waiting for the owner's boss sheets)*

Owner: Boss Fights gets longer combo chains for new bosses; **the owner makes the boss art**.

| Step | Work | Output |
|---|---|---|
| B1 | Spec §25 update: Boss-mode effects for the five skills not used yet — Cold Snap (freezes the boss in short pulses: a second way to hold it), Ice Wall (slow zone), Ghost Walk (the boss loses the player and stops ~2 s), Forge Spirit (spirits hit over time), Alacrity (faster spirits) — and new bosses with 5-6 spell combos and two phases (the combo changes at 50 % HP) | spec, owner confirms |
| B2 | `BossSession`: the new effects, phases, the new boss table | `Practice/Boss.*`, tests |
| B3 | Art hand-off: boss sheet format for the owner (below); the new sheets drawn per boss (walk, hit, lifted/frozen, death) | GameManager |
| B4 | Presentation: new effects' visuals (Ice Wall zone, frozen tint, spirits), phase change, boss list with the new bosses | GameManager |
| B5 | Release on all platforms, docs | 1.5.0 |

Proposed new bosses (to confirm in B1): Frost Troll — Cold Snap → Sun Strike · Glacier Golem — Ice Wall → Chaos Meteor → Deafening Blast · Fire Imp — Cold Snap → Alacrity → Forge Spirit (graded on speed, not landing) · Shadow Assassin — Ghost Walk → Tornado → Chaos Meteor → Sun Strike → Deafening Blast · Final boss — Tornado → EMP → Chaos Meteor → Sun Strike → Ice Wall → Deafening Blast, second phase Cold Snap → Forge Spirit → Alacrity.

Boss art format for the owner: PNG with transparency, square frames of 256 px in a grid (like the Injoker sheets), boss **facing left** (it walks toward the player), feet on the same row in every frame; sheets: walk (8-16 frames), hit (4-8), death (8-16); optional idle. One sheet per animation, file names `assets/bosses/<name>-walk.png` etc.

#### 1.6.0 — OVERLORD in PLAY and material drops  *(done 2026-10-01, spec §28 confirmed by the owner)*

Owner idea 2026-10-01: Boss Fights bosses appear inside PLAY as **OVERLORDS** (20th / 30th / 40th enemy: Dark Wizard, Shadow Assassin, Archon; random and harder from the 50th on), fought by the Boss-mode rules with the run's lives and slots; a warning with a sound per tier; contact costs 1 life; the difficulty clock stops during the fight; Refresher Orb = the next combo deals x2. Each overlord drops a **material** (Point Booster, Mystic Staff, Sacred Relic) that the shop requires for Aghanim's Scepter and for every level-2 upgrade.

| Step | Work |
|---|---|
| C1 | Owner approves spec §28 (numbers: +50 points, +100 gold, scaling, material table) |
| C2 | Practice layer: overlord hand-off in `PracticeSession` (shared invoker, lives, items, clock), materials and requirements in `Practice/Items.*`, tests |
| C3 | Presentation: warning banner and three synthesised warning sounds, the Boss-mode HUD inside PLAY, drop and Game Over lines, shop requirement lines and material counts |
| C4 | Saving materials, privacy page, store text; release on PC / web / Android |

All four steps done in 1.6.0. Left for playtesting: the numbers (reward, scaling), and whether overlords should also appear by chance (the owner was undecided).

### Phase 8 — Future Combat / Story  *(deliberately not started)*
Design note only, when the MVP has proven fun: Threne story, Evil King, hero roles, assign-cards screens, real skill effects/damage, bosses, multi-target hard modes, beginner-mode hints. Builds on Core; no code before this phase is explicitly opened.

### Phase 9 — PC / Steam  *(conditional)*
Only if the game proves worthwhile: IP review of names/icons/assets (currently "free to use" per the owner, not documented in-repo), Windows packaging, Steamworks, store assets, options menu.

---

## Progress log
- 2026-09-21 — **Phase 0** done (audit, gameplay spec v2, plan).
- 2026-09-21 — **Phase 1** done: build portable, UB and spawn-timer bugs fixed, dead code removed (branch `phase-1-stabilization`, commit `eb25131`).
- 2026-09-21 — **Phase 2** done: Invoker Core extracted, legacy logic removed, `Tests/InvokerCoreTests` added (243 checks) (branch `phase-2-invoker-core`). Next: Phase 3.
- 2026-09-21 — **Phase 3** implemented (branch `phase-2-invoker-core`, commit `a76802e`): Practice layer + tests, SDL integration, HUD, Ready/Game Over screens, `--debug` switch.
- 2026-09-21 — **Phase 3 review round** (commit `fix: refine practice session lifecycle`): Best Combo kept across restarts, Esc behaviour by state, Enter/Esc rules moved into `PracticeSession`, tests 328 checks, duplicate HUD formatting removed. Awaiting owner review. Next: Phase 4 (only after approval).
- 2026-09-22 — **Tornado spell effect** (commit `430b5ba`, requested after the Phase 3 review): first projectile spell. Practice: `Tornado` projectile (fixed-direction straight flight by `dt`, circle-vs-box hit, judged through the existing `JudgeCast` when it hits, misses change nothing); SDL: 16-frame 4×4 sheet `assets/Skills/Tornado/tornado_vfx_16f.png` drawn with nearest-neighbour above the enemy and below the HUD, looping animation. Owner-confirmed rules: a wrong-target hit is one wrong cast (enemy stays), no enemy = no projectile and no counted cast, no limit on projectiles in flight. Practice tests now 864 checks. Not Phase 4.
- 2026-09-23 — **Web build and mobile** (branch `phase-2-invoker-core`, now merged into `main`): Emscripten web build on GitHub Pages, touch controls, Top-10 scores, placeholder VFX for 8 skills, always-on target-skill hint (owner decision), phone landscape/fullscreen layout, centred colour-coded orb HUD.
- 2026-09-28 — **Phase 4/5 clean-up:** frame step `RunFrame()` + browser-driven main loop (ASYNCIFY dropped), dt-based enemy animation and parallax, desktop cap 25 → 60 FPS, `web/build.sh` (build + verified deploy), docs updated, work consolidated on `main`.
- 2026-09-28 — **Phase 6 (part 1):** persistent records (Best Score / Combo / Survival Time, `MergeBests`, 12 new checks → Practice tests 876) and light hit/miss/leak feedback; pixel font gains `!` and `+`.
- 2026-09-29 — **Phase 6 (part 2):** synthesised sound effects with mute, code-drawn effects for 8 skills (placeholder sheets no longer used), web visitor counter.
- 2026-09-29 — Clean-up: the 8 unused placeholder VFX sheets deleted; branches `phase-2-invoker-core`, `backU`, `Jun2024` deleted (all work is on `main`). Owner decisions: no background music; difficulty tuning waits for difficulty-dependent backgrounds.
- 2026-09-30 — Pixel-art polish (nearest scaling, unsquashed background), tech-debt fixes (Close frees backgrounds, BaseObject non-copyable, EnemyObject uses base members), new skill icons with `art/make_skill_icons.py`, target hint icon, recipe reference (H) outside play; RECIPE HINT option (G, off by default, assisted runs not ranked).
- 2026-09-30 — **Phase 7 started:** native Android project (`android/`), Android-specific code paths, launcher icon; `CMakeLists.txt` for the NDK build.
- 2026-09-30 — **Tutorial mode** (spec §24): 4 lessons, one key-by-key spell (Sun Strike), highlights, final unguided run; `Practice/Tutorial.*` + 70 new test checks (Practice tests 946); pixel font gains `,` `=` `>`.
- 2026-09-30 — **Renamed to Injoker** (product name for Google Play; repo/folders unchanged): new player character gliding over a magic ring with the loaded orbs circling it, Ready-screen logo, Android app id `net.relifes.injoker`, new launcher / web / store icons (`art/make_injoker.py`). Web domain change to injoker.relifes.net pending the owner's DNS record.
- 2026-09-30 — **Main menu + leaderboard UI**: one option per line (arrows/Enter/hotkeys/tap), top 3 by survival time beside it, top 10 screen with MY BEST, Game Over rank line; this device's list for now. Pixel font gains `#`.
- 2026-09-30 — **Google Play release prep:** signed AAB, targetSdk 36 (+ Back fix for Android 16), 16 KB alignment verified, adaptive icon, listing/declarations/feature graphic/screenshots, privacy policy on the web.
- 2026-09-30 — **1.1.0:** RECIPE HINT option (spec §17 H-1..H-4), policy / terms / support pages linked from the game page.
- 2026-09-30 — **1.2.0 Boss mode** (spec §25): `Practice/Boss.*` (`BossSession`, 93 new test checks → Practice tests 1062), menu line BOSS FIGHTS, three bosses with Dota-timed combos, impact rings, CAST NOW cue on boss 1, best time per boss; versionCode 2.
- 2026-09-30 — **1.2.1** (owner feedback: bosses too hard): timing grades PERFECT / GREAT / GOOD with damage in % (a perfect combo kills at once, otherwise 2-3 combos), a missed spell only loses its share, timing bars under the combo icons with RECIPE HINT on (spec §25 B-4, B-7, B-14, B-15); Practice tests 1090; versionCode 3.
- 2026-09-30 — **1.2.2:** BOSS FIGHTS moved right under PLAY in the menu; build files named after the version (`injoker-<version>-vc<code>-release.aab`); versionCode 4.
- 2026-10-01 — **1.3.0 PLAY mode** (spec §26): Practice renamed SURVIVAL in the UI; PLAY with elites (2-skill chains), bosses (3-skill chains every 10 enemies), leak damage 1/2/3, runes after bosses, stage tints, gold bank, PLAY leaderboard by score; 58 new test checks → Practice tests 1148; versionCode 5.
- 2026-10-01 — **1.3.1:** the owner's running Injoker sheet replaces the gliding picture; the owner's Chaos Meteor and Forge Spirit sprite sheets replace their code-drawn effects (Boss mode shows the meteor falling before its impact); versionCode 6.
- 2026-10-01 — **1.3.2:** the run animation slowed from 14 to 9 frames per second (owner: too fast, not smooth); versionCode 7.
- 2026-10-01 — **1.3.3:** run animation 7 fps and the Injoker drawn at 90 % (owner: still hurried); versionCode 8.
- 2026-10-01 — **1.3.4:** run animation 5.5 fps (owner: still hurried); versionCode 9.
- 2026-10-01 — **1.3.5:** magic ring under the Injoker's feet removed; run animation 6 fps (owner); versionCode 10.
- 2026-10-01 — **1.3.6:** new Home: no logo picture, the four modes as big buttons, RECIPES / LEADERBOARD / SETTINGS (/ QUIT) as small ones, SETTINGS panel for sound and the recipe hint; versionCode 11.
- 2026-10-01 — **1.3.7:** the owner's second Injoker art: new run sheet and a cast sheet played on every D / F cast (drawn at 50 %); versionCode 12.
- 2026-10-01 — **1.4.0 Shop and items** (spec §27): `Practice/Items.*` + item effects in PLAY, SHOP screen, 2 x 3 item bar (U I O / J K L), Aghanim rune choice, pixel icons (`art/make_item_icons.py`), inventory saved; 83 new test checks → Practice tests 1231; versionCode 13.
- 2026-10-01 — **1.4.1:** shop: the price is written on the BUY / UPGRADE button (the label under it was crossed by the button's pulsing frame), and a line says how much gold is missing; versionCode 14.
- 2026-10-01 — **1.5.0 Boss Fights with all ten skills** (spec §25 B-16..B-20): quick steps graded on cast speed, holds (Cold Snap freeze, Ice Wall slow, Ghost Walk confusion), five new bosses with combos of up to five spells, the two-phase Archon, compact boss list for eight bosses; 88 new test checks → Practice tests 1319; versionCode 15. Boss art still placeholder (owner draws it).
- 2026-10-01 — **1.6.0 OVERLORD in PLAY and materials** (spec §28): the 20th / 30th / 40th enemy of a PLAY run is a Boss Fights boss (Dark Wizard, Shadow Assassin, Archon), random and harder from the 50th on; `PracticeSession` owns a `BossSession` and hands the run to it (lives, orbs, slots, item effects; shared declarations moved to `Practice/Field.h`); warning banner with one to three horn calls; contact costs 1 life; the difficulty clock stops; Refresher Orb arms x2 damage; +50 points, +100 gold, a rune and one material per overlord; the shop needs materials for Aghanim's Scepter and every level-2 upgrade; 139 new test checks → Practice tests 1458; versionCode 16.
- 2026-10-01 — **1.6.1** (owner feedback after playing 1.6.0): Esc / Back while playing **pauses** (RESUME / QUIT TO MENU; also in boss fights and when the app goes to the background); **runs with the recipe hint are ranked** and set records like any other; **icons** for the three materials and the five runes, which hop out of a fallen boss; the **Game Over and boss result screens** rebuilt as one panel of label / value rows with the commands as buttons; versionCode 17.
- 2026-10-01 — **Online leaderboards prepared** (spec §29, owner decisions of the same day): Google Play Games on Android only, Google's leaderboard screen, boards PLAY (score) and SURVIVAL (time), sign-in optional. Code in place and switched off (`Online.*`, `OnlineBoards` twice, `android/play-games.properties.example`); both Android variants build, the Play Games one opens Google's sign-in on the emulator. **Waiting for the owner's Play Console steps** (`android/PLAY_GAMES_SETUP.md`): the three ids. Then: fill them in, privacy policy / Data safety / store text, version 1.7.0, test with a tester account. No version bump yet (nothing changes for players).
- 2026-10-01 — **1.7.0 Global leaderboards switched on** (spec §29): the owner created the two boards; ids in `android/play-games.properties` (the project id 437985672962 is decoded from the board ids, to be checked on the Configuration page); privacy policy (new section "Global leaderboards"), support page and store text updated; versionCode 18. **Not verified yet with a real account:** sign-in, a submitted score, Google's leaderboard screen (needs a tester account signed in on a device or a Google Play emulator). Owner's remaining Console steps: credentials for the three SHA-1, testers, Data safety form, publishing the Play Games project.
- 2026-10-01 — **1.7.0 fix before any upload:** a Google sign-in left unfinished (Home, then back to the game) made the GLOBAL RANKING button dead, because the game is a singleInstance activity and the sign-in waits in a task of its own; tapping the button again now brings that task back (`TaskResumeActivity`). Tried and rejected: `singleTask` (the add-account flow destroys the game activity) and closing the waiting screens (Play Games stays stuck). First real sign-in attempt on the emulator (owner's account): Play Games answers **DEVELOPER_ERROR** for package `net.relifes.injoker`, debug SHA-1 `55:34:0B:AE:...:4B:8B`, app id 437985672962 — a Console matter (credential not linked in Play Games Services, tester list, or OAuth consent test users), not code.
- Next (owner): **shop** for the gold (Invoker-only items with Dota-like pixel icons, 6 activatable item slots, mix of permanent and consumable, high prices; Refresher Orb removes 2 skills of a chain, Blink Dagger makes enemies walk back for 3 s) — to be confirmed again with the owner before any code; then Boss Fights with all 10 skills; then stage backgrounds (owner looking for art) with difficulty tuning.
