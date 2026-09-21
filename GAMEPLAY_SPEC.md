# GAMEPLAY_SPEC.md — Three Elements: Practice Mode MVP

Status: **v2 — authoritative** (2026-09-21). Supersedes v1. Source: the owner's decisions of that date. No C++ was changed by this document.
Companion docs: [PROJECT_AUDIT.md](PROJECT_AUDIT.md) (current code), [PLAN.md](PLAN.md) (roadmap), [CLAUDE.md](CLAUDE.md) (hand-over).

## How to read this document

| Tag | Meaning |
|---|---|
| **[CONFIRMED]** | Decided by the owner (or fixed by the Invoker reference the owner told us to follow). Implement as written. |
| **[RECOMMENDED]** | Implementation choice made to fill a gap. Not a product rule; change it freely if there is a better reason. |
| **[FUTURE]** | Not in the MVP. Do not implement; do not block it. |

**Tie-breaker rule [CONFIRMED]:** *This is an Invoker practice game.* The reference model is the **current Dota 2 Invoker**; the owner also checked <https://invoker-game.com/> as a practical reference for the 10-spell recipe system and controls (not independently verified by the agent that wrote this file). Do **not** invent a new spell-combination system. Where this spec is silent about an Invoker mechanic, follow Invoker and do not ask; do not add gameplay rules that this spec does not state.

Glossary: **Orb** = one Q/W/E element entered. **Recipe** = the set of 3 orbs (order ignored). **Invoke** = R. **Slot** = D or F. **Cast** = pressing D or F. **Challenge** = one enemy that requires one skill. **Leak** = an enemy reaching the player. **Session** = one play from start to Game Over.

---

## 1. Product goal

- **G-1 [CONFIRMED]** The game is an **Invoker practice game**. The player practises the same fundamental actions as Invoker: **Q/W/E → R → D/F**. It trains: keyboard/input speed, fast switching between skills, remembering recipes, deciding which skill to use, continuous combos, reaction speed.
- **G-2 [CONFIRMED]** Practice Mode is the **foundation** of the future game, not throwaway code. The Invoker mechanic (**Core**) is kept independent of the Practice game (§20).
- **G-3 [CONFIRMED]** Targets: **Web first** (GitHub Pages) → Android → PC/Steam only if the game proves good. Visual direction: pixel art, side view; the existing parallax background is reused/fixed.
- **G-4 [CONFIRMED]** Input: PC/Web = keyboard `Q W E R D F`; mobile later = six touch buttons laid out like a keyboard. Keyboard and touch feed the **same logical input actions**.

## 2. Core gameplay loop

- **L-1 [CONFIRMED]** An endless practice challenge with **one active challenge enemy at a time**. Each enemy has a `TargetSkillId`.
- **L-2 [CONFIRMED]** Loop: `enemy appears → player identifies the required skill from the enemy → remembers its Q/W/E recipe → Q/W/E → R → D/F → correct cast removes the enemy → next challenge begins after a delay.`
- **L-3 [CONFIRMED]** The enemy **never displays its recipe** (§17). The enemy is the gameplay representation of the required spell: the player must remember "this enemy requires Tornado".
- **L-4 [CONFIRMED]** No combat, no story. Enemy contact is the only damage source.

## 3. Q/W/E recipe rules

- **Q-1 [CONFIRMED]** Q = **Quas**, W = **Wex**, E = **Exort**.
- **Q-2 [CONFIRMED]** The player has up to **3 active orb instances**. Entering a 4th orb drops the oldest.
- **Q-3 [CONFIRMED]** The **order of the three orbs does not matter**. The spell is determined by the **count** of Q/W/E. `QQW`, `QWQ`, `WQQ` all resolve to Ghost Walk. **No order-sensitive recipes.**
- **Q-4 [CONFIRMED]** Recipe table:

| Recipe (counts) | Spell | Recipe | Spell |
|---|---|---|---|
| QQQ | Cold Snap | QEE | Forge Spirit |
| QQW | Ghost Walk | WWW | EMP |
| QQE | Ice Wall | WWE | Alacrity |
| QWW | Tornado | WEE | Chaos Meteor |
| QWE | Deafening Blast | EEE | Sun Strike |

- **Q-5 [RECOMMENDED]** Store the normalised recipe as counts `(q, w, e)` with `q+w+e = 3`. The existing sorted-string key (`"EQQ"` etc.) is equivalent; either is fine. The 10 recipes are exactly all 10 multisets of size 3 over {Q,W,E}: assert that at startup.
- **Q-6 [RECOMMENDED]** The orb *list* keeps entry order **for display only** (orb icons); recipe resolution ignores it.
- **Q-7 [RECOMMENDED]** Keyboard **auto-repeat** presses are ignored (only the initial press adds an orb).
- **Q-8 [CONFIRMED]** Invoking does **not** consume or reset the orbs (Invoker behaviour; the current code already works this way).

## 4. Invoke rules

- **I-1 [CONFIRMED]** **R** is Invoke.
- **I-2 [CONFIRMED]** With **exactly 3 orbs** active, R resolves the current combination into one of the 10 spells and puts it into the invoked-spell slots (§5).
- **I-3 [CONFIRMED]** With **fewer than 3 orbs**, R **does nothing**: no penalty, orbs are **not** reset, no effect on accuracy.
- **I-4 [CONFIRMED]** **No spell cooldowns** in the MVP (including Invoke). All 10 spells are available from the start; no unlocks.
- **I-5 [CONFIRMED]** Invoke is **not judged**; only casts (D/F) are.

## 5. D/F slot rules

- **S-1 [CONFIRMED]** D and F are Invoker's **two invoked-spell slots** (current / previous invoked spell). They are **not** two permanently assigned independent spells.
- **S-2 [CONFIRMED]** Invoking a spell that is in neither slot: it goes to **D**; the spell that was in D moves to **F**; the spell that was in F is discarded. (Matches the current `saveSpellToSlot` and Dota 2.)
- **S-3 [CONFIRMED]** Invoking a spell **already in F** moves it to **D** and the D spell to **F** (swap). Invoking the spell **already in D** changes nothing. (Dota 2 behaviour; matches the current code.)
- **S-4 [CONFIRMED]** **D casts the spell in slot D; F casts the spell in slot F.**
- **S-5 [CONFIRMED]** After a cast, the spell **stays in its slot** until a later Invoke changes the slots.
- **S-6 [CONFIRMED]** Slot order/UI follows the existing D/F implementation where it already behaves correctly (D = most recent, F = previous).
- **S-7 [CONFIRMED]** Worked example — the sequence the mechanic must support:

| Step | D | F |
|---|---|---|
| invoke A | A | – |
| invoke B | B | A |
| cast A (press F) | B | A |
| cast B (press D) | B | A |
| invoke C | C | B |
| cast B (press F) | C | B |
| cast C (press D) | C | B |

## 6. Skill data model

- **K-1 [CONFIRMED]** `SkillDefinition` = skill id, recipe, name, icon, other skill metadata. Skills are held in a `SkillCatalog` of 10 entries.
- **K-2 [RECOMMENDED]** `id` is a stable internal identifier (never an array index, never derived from the display name). `recipe` is normalised (Q-5). `name` is display text. `icon` is an asset key/path string (textures belong to presentation). `metadata` is reserved and **unused in the MVP**.
- **K-3 [RECOMMENDED]** Catalog is static and immutable, with lookup by id and by recipe; it **replaces the per-`Skill` copies of `spellMap`**. Validate at startup (10 unique recipes covering all multisets, ids unique, icons exist) and fail loudly.
- **K-4 [RECOMMENDED]** Names and icons are data, so a later re-skin needs no logic change.
- Initial catalog = the 10 spells of Q-4, using the existing icons: `assets/skill/{ColdSnap, GhostWalk, IceWall, EMP, Tornado, Alacrity, SunStrike, ForgeSpirit, Meteor (Chaos Meteor), Blast (Deafening Blast)}.png`.
- **K-5 [FUTURE]** Effects, damage, cooldowns, unlocks.

## 7. Enemy data model

- **E-1 [CONFIRMED]** `EnemyDefinition` = **EnemyId, visual/sprite information, movement parameters, TargetSkillId**.
- **E-2 [CONFIRMED]** **Do not hard-code "enemy type == skill id".** The relationship is the data field `TargetSkillId`, so visuals can be reused with different target skills later.
- **E-3 [CONFIRMED]** Prepare **10 enemy definitions, one visual identity per skill**. For the MVP each definition targets a different skill (10 ↔ 10).
- **E-4 [RECOMMENDED]** Fields: sprite asset key, frame count, animation frame duration (ms), ground anchor by frame height (sheet heights vary 64–150 px); movement: `speedMultiplier` (default 1.0) applied on top of the difficulty speed.
- **E-5 [RECOMMENDED]** A spawned enemy instance carries its **own** `targetSkillId` copied from the definition; all correctness checks compare **skill ids** only.
- **E-6 [RECOMMENDED]** Enemy logical state (position, speed, alive flag) lives in **Practice**; presentation draws the sprite at that position. Movement is time-based (`dt`).
- **E-7 [RECOMMENDED]** The initial sprite↔skill assignment is a single data table chosen by the implementer from the 13 existing sheets (7 in use: mushroom_run 8f, goblin_run 8f, eyes_fly 8f, skeleton 4f, fire_wiz 8f, nec_walk 10f, worm_run 9f; unused: bat_fly, dark_wiz, kitsune_run, knight_run, mush, nec_walk_bg — verify their frame counts by looking at the sheets). Any one-to-one assignment is acceptable; the owner can change the table later without code changes.
- **E-8 [RECOMMENDED]** **Development-only debug option** that displays the active enemy's `TargetSkillId` for testing. It must be off by default, unreachable from normal play, and absent from the web release build (e.g. a compile-time flag).
- **E-9 [FUTURE]** Enemy AI, variants, bosses, per-spawn target reassignment, beginner-mode hints.

## 8. Practice challenge flow

`Waiting (delay) → Active (enemy alive) → Resolved (Killed | Leaked) → Waiting …`

1. **Waiting:** no active enemy; a delay counts down (§14).
2. **Active:** one enemy spawns off the right edge with its `targetSkillId` and moves toward the player at the current difficulty speed.
3. **Resolved:** by a correct cast (§9) or a leak (§10). The enemy disappears. If HP > 0 → Waiting, else Game Over.

- **C-1 [CONFIRMED]** One active challenge enemy at a time; no multi-target selection; no combat targeting.
- **C-2 [CONFIRMED]** "Next challenge begins" after resolution, after the delay of §14.
- **C-3 [RECOMMENDED]** The next enemy definition is chosen uniformly at random from the 10, **except that the same `TargetSkillId` is not chosen twice in a row** (a repeat would need no Invoke and would weaken the practice). Use an injectable RNG (seeded from time, seed logged) so runs can be reproduced.
- **C-4 [RECOMMENDED]** Within a frame, all input actions are applied **in arrival order, before** the enemy movement update, so a correct cast on the frame an enemy would leak wins.
- **C-5 [RECOMMENDED]** "Reaches the player" = the enemy's front edge crosses one named **hit line** constant in front of the player; test with `<=` (never `==`).

## 9. Input classification (what each input does)

| Input (while Playing) | Effect | Judged / counted in accuracy |
|---|---|---|
| Q / W / E | add an orb (drop the oldest if already 3) | no |
| R, fewer than 3 orbs | nothing; orbs kept | no |
| R, 3 orbs | invoke; slots updated (§5) | no |
| D/F, **slot empty** | nothing | **no** (not an incorrect cast) |
| D/F, slot filled, **no active enemy** | nothing; spell stays in slot | **no** (not an incorrect cast) |
| D/F, slot filled, active enemy, spell == target | **correct cast** (§10) | **yes, correct** |
| D/F, slot filled, active enemy, spell != target | **wrong cast** (§11) | **yes, incorrect** |
| Key auto-repeat | ignored | no |
| any gameplay key after Game Over | ignored (only restart works) | no |

A **cast** (for accuracy) is exactly a D/F press with a filled slot while an enemy is active.

## 10. Correct cast

- **X-1 [CONFIRMED]** The enemy **disappears immediately**. No damage system.
- **X-2 [CONFIRMED]** **Score +1** and **Combo +1**.
- **X-3 [CONFIRMED]** Accuracy counts a **correct** cast.
- **X-4 [CONFIRMED]** The next challenge begins (§8). The cast spell stays in its slot (S-5).
- **X-5 [FUTURE]** Death/hit effect, sound.
- **X-6 [CONFIRMED, owner request after Phase 3]** **Tornado is the first spell with a real effect: a projectile.** Casting Tornado from D or F (recognised by the `SkillId` in the slot, never by the slot) launches a projectile from the player toward the current enemy instead of resolving at once. The direction is fixed at launch (no homing); it flies straight, moved by `dt`. **It is judged only when it hits the enemy it was launched at**, through the same scoring path as every other cast (`JudgeCast`): a hit on an enemy that needs Tornado is a correct cast (enemy removed, score +1, combo +1, exactly once); the projectile is then removed. A **miss** (it leaves the play area or reaches its maximum range, or its enemy is already gone) changes nothing: no score, combo, accuracy, HP or enemy change. With no active enemy there is nothing to aim at: no projectile is created and the cast is not counted (no accuracy, combo, score or HP change). Other spells still resolve at once as before.
- **X-7 [CONFIRMED, owner]** A Tornado that **hits an enemy that needs another spell** is a **wrong cast**: the enemy stays alive, HP is unchanged, combo / score / accuracy follow the normal wrong-cast rules (§11: it counts as one incorrect cast and does not break the combo) and the projectile is removed. The enemy is never killed by a wrong-target Tornado. "Correct" keeps its single definition (spell = the enemy's target). Only a projectile that hits nothing is free.
- **X-8 [CONFIRMED, owner]** There is **no gameplay limit** on projectiles: every valid Tornado cast (an active enemy exists) creates exactly one projectile, however many are already in flight; each is judged on its own when it hits. Each one is removed when it hits, leaves the play area or reaches its maximum range, so the count is bounded by how fast keys can be pressed and the code has no technical cap either. The animation loops (frames 0 → 15 → 0 …) for as long as the projectile lives; a projectile that is removed before frame 15 simply never completes a cycle. Values (speed 700 px/s, hit radius 20 px, range 1200 px, animation 16 frames at 10 fps) are **initial MVP tuning values**. Non-goals unchanged: no damage numbers, mana, cooldowns, stun, knockback, penetration, multi-target, homing or upgrades.

## 11. Wrong cast

- **W-1 [CONFIRMED]** The enemy is **not damaged** and is **not removed**; it **continues moving toward the player**.
- **W-2 [CONFIRMED]** The player **may try again**; attempts are unlimited.
- **W-3 [CONFIRMED]** It counts as an **incorrect cast** in accuracy.
- **W-4 [CONFIRMED]** It does **not** reset or reduce the combo, and costs no HP or score.

## 12. HP and Game Over

- **H-1 [CONFIRMED]** Initial HP = **3**.
- **H-2 [CONFIRMED]** When an enemy reaches the player (leak): **HP −1**, the enemy disappears, **combo resets to 0**, the next challenge begins (unless Game Over).
- **H-3 [CONFIRMED]** **Game Over when HP ≤ 0.** Enemy contact is the only damage source. No enemy attack system, no spell damage.
- **H-4 [RECOMMENDED]** On Game Over: spawning, movement and the survival timer stop; stats are frozen and shown; only restart input is accepted; no new enemy is spawned.

## 13. Score, combo, accuracy

- **P-1 [CONFIRMED]** Score: **+1 per correct challenge.** No formulas, multipliers or bonuses.
- **P-2 [CONFIRMED]** Current Combo: **+1** per correct challenge; **reset to 0 only by a leak**; a wrong cast never resets it.
- **P-3 [CONFIRMED]** Best Combo = the highest Current Combo reached.
- **P-4 [CONFIRMED]** Accuracy = **correct casts ÷ total casts** (§9 definition of a cast). Example: 10 correct + 2 wrong = 83.3 %.
- **P-5 [RECOMMENDED]** With **zero casts**, show a neutral placeholder (e.g. "--"); never divide by zero.
- **P-6 [FUTURE]** Speed bonus, combo multiplier, difficulty multiplier, perfect bonus, reaction/invoke-time metrics.

**Statistics to track and display in the MVP [CONFIRMED]:** Score, Current Combo, Best Combo, Accuracy, Survival Time (HP and the D/F slots/orbs are also shown as part of the play UI). Survival Time counts only while Playing.

**Persistent local statistics [CONFIRMED]:** **Best Score, Best Combo, Best Survival Time** persist locally and are **not reset by Restart**.
- **Current state (Phase 3 review, owner decision):** only **Best Combo** exists so far. It is a record kept for as long as the application runs: a new session or a return to Ready resets the *current* combo but not the record, and it only changes when a new record is reached. Nothing is written to disk yet and every application run starts at 0. Best Score and Best Survival Time are not implemented yet.
- **P-7 [RECOMMENDED]** Practice exposes a plain result struct at Game Over; comparing/saving lives in presentation/platform (browser local storage on web, a small file on desktop). A failed load/save must never break the game (treat as "no saved data"). Implement after the session logic works; until then bests may live in memory only.

## 14. Difficulty model

- **D-1 [CONFIRMED]** The session is endless; difficulty increases with **elapsed session time**.
- **D-2 [CONFIRMED]** MVP tunes only **enemy movement speed** and the **delay between challenges**, via **one simple deterministic function of elapsed time**. No difficulty tiers.
- **D-3 [RECOMMENDED]** `difficulty(elapsedSeconds) → { enemySpeed, challengeDelay }`, a pure function; continuous and monotonic (never easier over time); **clamped** to a playable min/max; all constants in one place. The struct may gain fields later.
- **D-4 [RECOMMENDED]** Starting values = the current code's behaviour (≈125 px/s, 5 s between spawns) as placeholders, to be tuned by playtesting. Effective speed = `enemySpeed × EnemyDefinition.speedMultiplier`, sampled when the enemy spawns.
- **D-4b [CONFIRMED]** The values in Practice.h — enemy speed 125 → 380 px/s (+2.5 px/s per second survived), challenge delay 1.5 → 0.5 s (−0.01 s per second) — are **initial MVP tuning values**, not final and not balanced. The bounds (max speed, min delay, `dt` clamp) are part of the design; the numbers are for playtesting.
- **D-5 [RECOMMENDED]** Elapsed time = survival time accumulated from `dt` while Playing (not wall-clock), with `dt` clamped per frame.
- **D-6 [FUTURE]** More pressure variables; harder modes with several enemies (not MVP).

## 15. Session lifecycle

`Loading → Ready → Playing → GameOver → (Restart) → Playing`

- **Z-1 [CONFIRMED]** **New Game / Restart resets:** active orbs, D/F slots, HP, score, current combo, accuracy, survival timer, active enemy, difficulty timer. It does **not** reset Best Score / Best Combo / Best Survival Time (§13).
- **Z-2 [RECOMMENDED]** A `PracticeSession` owns all resettable Practice state; a restart (or a return to Ready) resets it in place and keeps only the best combo record; assets are **not** reloaded.
- **Z-3 [RECOMMENDED]** Restart is a UI command separate from the six gameplay actions (never Q/W/E/R/D/F): e.g. Enter/Space on keyboard, a button on mobile. Starting on a key press (rather than immediately) is preferred for web focus and future audio unlock.
- **Z-3b [CONFIRMED]** Controls (owner decision, Phase 3 review): **Ready** — Enter starts a session, Esc quits the application. **Playing** — Esc returns to Ready (the session is stopped and reset, the best combo record is kept), Enter is ignored, so nothing can restart or quit by accident. **Game Over** — Enter starts a new session, Esc returns to Ready. There is no pause.
- **Z-4 [RECOMMENDED]** Focus loss/hidden tab: clamp `dt`; never spawn several enemies to "catch up".

## 16. MVP scope (in)

1. Logical input actions Q W E R D F (keyboard mapping; touch later via the same actions).
2. Invoker Core: 3 orbs, count-based recipes, 10-spell catalog, Invoke, D/F slots with Invoker semantics.
3. `EnemyDefinition` with `TargetSkillId`; 10 definitions mapped 10 ↔ 10.
4. Single-challenge flow with correct/wrong cast rules of §9–§11.
5. HP 3, leak, Game Over; score, combo, best combo, accuracy, survival time; persistent bests.
6. Time-based difficulty (speed + delay).
7. Session lifecycle with restart.
8. Existing parallax background reused/fixed; pixel-art side view.
9. Web as the first target.

## 17. Explicit non-goals

**[CONFIRMED]** Not in the MVP: story mode, narrative, combat system, complex enemy AI, cooldowns, skill unlocks, multi-target mode, bosses, monetisation, Steam integration, advanced mobile UX, achievements, complicated score formulas, difficulty tiers.

**No gameplay hints [CONFIRMED].** In normal play, never display: the Q/W/E recipe above the enemy, the required key sequence, recipe text, "press QQW"-style prompts, or spell-recipe overlays. Showing the *icons* of the currently invoked spells in D/F and the current orbs is normal UI, not a hint. (Only the development-only debug option of E-8 may reveal `TargetSkillId`.)

**[FUTURE]** A beginner mode may add hints. *Observation (not blocking):* with no hints and no reference in the game, the enemy↔spell mapping can only be learnt by trial and error; a beginner mode or an out-of-game reference would address this later.

**Not addressed here (handled by PLAN.md):** audio, menus beyond restart, settings, tutorials, localisation.

## 18. Future extension points

| Future feature | Plugs into | Must hold today |
|---|---|---|
| Combat (effects, damage) | consumes Core cast results (`skillId`) | Core knows nothing about enemies/HP |
| Story / Threne / cards | a separate mode reusing Core | names/icons are data (K-4) |
| Multi-target / hard modes | Practice: list of enemies + targeting policy | comparisons use skill ids only (E-5) |
| Beginner hints | presentation reads `targetSkillId` | instance carries its own target id |
| More metrics (invoke time, reaction time) | inputs carry timestamps | `InputAction` is timestamped |
| Touch UI | another mapper producing `InputAction`s | Core/Practice never read SDL |
| Cooldowns / unlocks | fields on `SkillDefinition` | `metadata` reserved |
| Score/difficulty variables | extend `DifficultyParams` / result struct | single difficulty function |
| Enemy reuse with other targets | change `TargetSkillId` in data | E-2 |

## 19. Edge cases the implementation must handle

- **EC-1** Key **auto-repeat** must not add orbs or repeat R/D/F.
- **EC-2** **Several inputs in one frame** are processed in order; none dropped or merged.
- **EC-3** **R with < 3 orbs**: no-op, no state change, no crash (I-3).
- **EC-4** Invoke a spell **already in D** (no change) and **already in F** (swap) (S-3).
- **EC-5** **Empty slot** cast and **cast with no active enemy**: ignored, never counted (§9); must not dereference a missing skill.
- **EC-6** A **wrong cast never changes** enemy, HP, combo, score or slot contents; only accuracy counters (W-4).
- **EC-7** An enemy is **resolved exactly once**; a leak and a correct cast on the same frame → the cast wins (C-4).
- **EC-8** Leak that brings HP to ≤ 0 → Game Over immediately; **no new spawn**.
- **EC-9** After Game Over only restart is accepted; stats do not change.
- **EC-10** **Large `dt`**: clamp; the leak test uses `<=` (C-5).
- **EC-11** **Accuracy with zero casts** shows a placeholder (P-5).
- **EC-12** **Startup validation** fails visibly: recipes unique and complete, every `TargetSkillId` exists, every asset loads.
- **EC-13** **Restart** fully resets Z-1 state without reloading assets and leaves no enemy/input leftovers; **bests survive**.
- **EC-14** The same skill needed **on consecutive challenges** cannot occur with C-3, but if it did (future change), retained slots must make it work.
- **EC-15** **Order independence by construction:** `QQW`, `QWQ`, `WQQ` cannot diverge through any path that looks at input order.
- **EC-16** Empty orbs/slots at startup render without error (HUD tolerates 0–2 orbs and empty slots).
- **EC-17** Saved statistics unreadable/absent → start from zero, no crash.
- **EC-18** Non-QWERTY layouts: use keycodes for the letters for now; revisit only if it becomes a problem.

## 20. Architecture boundaries

Dependencies point one way: **Presentation → Practice → Core.** Keep it small: plain structs and functions returning small result values. **Do not** add an engine, an event bus, dependency injection, ECS, unnecessary inheritance or templates. Core and Practice must not include SDL or read the clock (pass `dt`/timestamps in) or use globals.

**Core** — the Invoker mechanic, independent of the game
- `InputAction` (Q, W, E, R, D, F; timestamp supplied by the caller), the three reagents Quas/Wex/Exort, recipe normalisation, `SkillDefinition`, `SkillCatalog`, `InvokerState` (≤ 3 orbs, slot D, slot F), Invoke, D/F slot logic.
- Applying an action returns a small result (orb added / invoke done or ignored / cast of slot X yielding skill Y or nothing). Core does **not** know SDL, enemies, HP, score, rendering or the game loop.

**Practice** — the practice game
- `EnemyDefinition`, `TargetSkillId`, `PracticeSession` (HP, score, combo, accuracy, elapsed time, difficulty, active enemy, challenge result), the difficulty function, challenge selection. Consumes Core results plus `Update(dt)`; returns plain outcome values (correct, wrong, leaked, game over).

**Presentation / platform**
- SDL input mapping (keyboard now, touch later), rendering, sprites, audio, HUD, parallax, main loop, asset loading, persistence of best stats.

**Existing code:** the recipe lookup and D/F logic in `MainPlayer::getCombineComb` / `saveSpellToSlot` and `Skill::spellMap` are **already correct** — preserve their behaviour; move them into Core only as far as needed (single shared table, no per-instance copies, no SDL types). Do not rewrite working logic for style. `BaseObject`/`EnemyObject` stay as drawing helpers driven by Practice state. Full audit: [PROJECT_AUDIT.md](PROJECT_AUDIT.md) §17-18.

## 21. Open decisions

**None blocking.** Anything else the implementer needs (start values for speed/delay, hit-line position, initial sprite↔skill table, restart key) is a tunable/data choice covered by the RECOMMENDED rules above.

## 22. Former open decisions (v1) — how they were resolved
R with <3 orbs (OD-3) → no-op, §4 · slot kept after cast (OD-6) → kept, S-5 · reset scope (OD-7) → Z-1 · how to know the target (OD-1) → by enemy appearance only, no hints, §2/§17 · one enemy at a time (OD-2) → yes, C-1 · enemy roster (OD-8) → E-3/E-7 · HP (OD-13) → 3, 1 per leak · accuracy (OD-14) → §13 · combo break (OD-12) → leak only · score (OD-11) → +1 · empty-slot/no-enemy casts (OD-4/5) → not counted, §9 · spawn timing (OD-9) → delay between challenges, §14 · selection (OD-10) → C-3 · difficulty (OD-15) → D-2..D-4 · start/restart (OD-16) → Z-3 · stats display (OD-17) → MVP displays the five stats; text-rendering method is an implementation choice · best combo persistence (OD-18) → persistent, §13 · key layouts (OD-19) → EC-18 · pause/focus (OD-20) → Z-4.

## 23. Traceability to the current code

| Spec item | Current code | Status |
|---|---|---|
| Orbs, count-based recipe, R, D/F swap | `MainPlayer::handleKeyPress/getCombineComb/saveSpellToSlot`, `Skill::spellMap` | Correct; keep semantics. Gaps: no auto-repeat filter; R sets `m_KeyRActive` even with < 3 orbs (bug B8) |
| Cast D/F | none | Missing |
| `SkillDefinition` with ids | string map | Missing ids/catalog |
| `EnemyDefinition` + `TargetSkillId` | `enemyPaths` (path, frames) | Missing |
| One enemy, kill, leak, HP 3, Game Over | enemies wrap forever | Missing |
| Score, combo, accuracy, survival time, persistent bests | none | Missing |
| Difficulty by time | constant 5 s / 5 px per frame | Missing; needs `dt` |
