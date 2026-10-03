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
- **E-8 [CONFIRMED, owner decision 2026-09-23 — supersedes the original "development-only" rule below]** The active enemy's required skill (`TargetSkillId`) is shown on screen to every player, in every build (native and web), not only with `--debug`. Label: `TARGET: <skill name>`, top-right HUD. This makes the game a "look up the recipe" trainer rather than a pure-recall one; see the reversed non-goal in §17.
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
- **X-5** Hit feedback **[implemented 2026-09-28, presentation only]**: a correct cast shows a gold ring and a rising "+1" where the enemy was (the points text is **green** since 1.8.1: gold is kept for gold, so "+1" is not read as one gold); an enemy beaten by a spell that visibly travels (the Forge Spirit's run, the Chaos Meteor's fall) stays on screen, faded and standing still, until the spell reaches it, though the kill is judged at the cast as always; a wrong cast tints the enemy red and shows "MISS"; a leak shows a red screen frame, a short light shake and a blinking lost HP square. Sound effects are **[implemented 2026-09-29]** (synthesised, with a mute toggle); music is **[FUTURE]**.
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
- **Current state (2026-09-28):** all three records are implemented. Practice compares (`MergeBests`, tested); the presentation layer saves them (`bests.txt` next to the exe on desktop, `localStorage` on the web) and seeds the in-session Best Combo from them at start-up (`RestoreBestCombo`). A session updates the records when it ends by Game Over **or** by Esc while Playing. They are shown on the Ready screen and on Game Over, where a line that set a new record is marked "NEW BEST!". The Top-10 score list is kept alongside them.
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
- **Z-3b [CONFIRMED]** Controls (owner decision, Phase 3 review): **Ready** — Enter starts a session, Esc quits the application. **Playing** — Esc **pauses** (owner decision 2026-10-01, replaces "Esc returns to Ready; there is no pause"): nothing moves, the clocks stop, the gameplay keys do nothing. From the pause, Esc / Enter resumes and Q (or the QUIT TO MENU button) returns to Ready (the session is stopped and reset, the best combo record is kept). Enter is ignored while Playing, so nothing can restart or quit by accident. The pause belongs to the presentation: `PracticeSession` is simply not updated while it lasts, and `PressEscape` still means "back to Ready". An app sent to the background (phone) pauses by itself. **Game Over** — Enter starts a new session, Esc returns to Ready.
- **Z-3c [CONFIRMED, owner 2026-10-01]** **Result screens** (Game Over of both modes, a boss fight's result): information and commands are never mixed. What happened is one panel of `LABEL  value` rows (a gold `NEW BEST!` after a value that set a record; the rank and the records under a separator line); what the player can do is always a button below the panel (`PLAY AGAIN`, `MENU`, `RECIPES`, `LEADERBOARD`, each with its key on a keyboard).
- **Z-4 [RECOMMENDED]** Focus loss/hidden tab: clamp `dt`; never spawn several enemies to "catch up".
- **Z-6 [CONFIRMED, owner decision 2026-09-30]** The Ready screen is a **main menu**, one option per line (revised 2026-10-01: the four modes PLAY, SURVIVAL, BOSS FIGHTS, TUTORIAL as big buttons, then small RECIPES, LEADERBOARD, SETTINGS — sound and recipe hint in one panel — and QUIT; no logo picture): PLAY, BOSS FIGHTS (§25, moved under PLAY by the owner 2026-09-30), TUTORIAL, RECIPES, LEADERBOARD, SOUND, QUIT (not on the web). The **leaderboard ranks runs by survival time**; the menu shows the top 3, the LEADERBOARD screen the top 10. Online boards are planned: **Google Play Games on Android** (sign-in, weekly board by default plus all-time) and **a server of our own for the web**; until then the list is this device's.
- **Z-5 [CONFIRMED, owner decision 2026-09-30]** The Ready screen offers **two modes**: **Practice** (this document's game, Enter as before) and **Tutorial** (§24, T key or its button). An "Advanced" mode may come later **[FUTURE]**.

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

**No *recipe* hints [CONFIRMED].** In normal play, never display: the Q/W/E recipe above the enemy, the required key sequence, recipe text, "press QQW"-style prompts, or spell-recipe overlays. Showing the *icons* of the currently invoked spells in D/F and the current orbs is normal UI, not a hint.

**Target-skill hint [CONFIRMED, owner decision 2026-09-23 — reverses the previous "no hints" stance on `TargetSkillId`].** The enemy's required *skill name* (not its recipe) is now shown to every player at all times, per E-8. The player still has to know or work out the Q/W/E recipe for that skill themselves — only "which skill" is given, not "which keys".

**Recipe reference [CONFIRMED, owner decision 2026-09-30].** A reference list of all 10 skills (icon, name, the three orbs) can be opened with H or a "RECIPES" button **on the Ready and Game Over screens only**; it can never be opened while Playing, and any key or tap closes it. Knowing the recipes is still the player's job during play.

**Target-skill icon [CONFIRMED, owner decision 2026-09-30].** The target hint shows the skill's icon, large, below its name (still no recipe).

**Tutorial exception [CONFIRMED, owner decision 2026-09-30].** The Tutorial (§24) is a separate mode whose whole purpose is to show the keys: it displays key sequences such as `E E E R D`. None of that appears in Practice.

**Recipe hint option [CONFIRMED, owner decision 2026-09-30 — an intentional, off-by-default exception to "no recipe hints"].** A setting "RECIPE HINT" (**default OFF**; menu line, an in-play `HINT ON/OFF` button under `SOUND`, key G; saved with the other settings) shows the target skill's recipe as three coloured orbs between the `TARGET` name and its icon. Rules:
- **H-1** A run in which the hint was on **at any moment** (on at the start, or switched on while Playing) is **assisted**. Switching it off again does not undo this.
- **H-2 [CHANGED, owner 2026-10-01]** An assisted run is **ranked like any other**: it enters the leaderboards, sets records and best boss times, and earns gold. (Until 1.6.0 it was not ranked and set no records.) The `assisted` mark is kept as information only.
- **H-3** Score, combo, accuracy, HP and difficulty work exactly as in a normal run; the hint changes nothing but what is drawn.
- **H-4** The Tutorial never shows the hint (it has its own guidance); its last card suggests turning it on for players who need help.

*Observation (not blocking):* a full "beginner mode" beyond this option (slower enemies, etc.) is still FUTURE.

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
R with <3 orbs (OD-3) → no-op, §4 · slot kept after cast (OD-6) → kept, S-5 · reset scope (OD-7) → Z-1 · how to know the target (OD-1) → by enemy appearance, plus an on-screen skill-name hint (not a recipe), §7 E-8/§17 · one enemy at a time (OD-2) → yes, C-1 · enemy roster (OD-8) → E-3/E-7 · HP (OD-13) → 3, 1 per leak · accuracy (OD-14) → §13 · combo break (OD-12) → leak only · score (OD-11) → +1 · empty-slot/no-enemy casts (OD-4/5) → not counted, §9 · spawn timing (OD-9) → delay between challenges, §14 · selection (OD-10) → C-3 · difficulty (OD-15) → D-2..D-4 · start/restart (OD-16) → Z-3 · stats display (OD-17) → MVP displays the five stats; text-rendering method is an implementation choice · best combo persistence (OD-18) → persistent, §13 · key layouts (OD-19) → EC-18 · pause/focus (OD-20) → Z-4.

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

## 24. Tutorial mode

**Status: implemented 2026-09-30** (`Practice/Tutorial.*`, tested in `Tests/PracticeTests`; drawn by `GameManager::RenderTutorial`).

A short guided introduction for players who have never played Invoker. Owner decisions 2026-09-30: step pacing with NEXT, **one** key-by-key example (Sun Strike, `E E E`), everything else explained on cards and then looked up by the player, strong highlighting of what to look at, a choice between Practice and Tutorial on the Ready screen, English only, no other modes for now.

### Rules

- **T-1 [CONFIRMED]** Entered from the Ready screen (T key or a TUTORIAL button); Practice stays the default (Enter). Esc leaves the tutorial at any time and returns to Ready.
- **T-2 [CONFIRMED]** English text only (the built-in pixel font: capitals, digits, basic punctuation). Every text line fits the 928 px screen at scale 2 (about 45 characters).
- **T-3 [CONFIRMED]** Two kinds of steps:
  - **Card**: a short explanation. The player continues with **NEXT**: Enter or Space, or tapping the NEXT button.
  - **Guided input**: the exact sequence to press is shown (e.g. `E  E  E  R  D`), the key to press **now** is highlighted (in the sequence, and on the touch button on phones), and the step waits for it.
- **T-4 [CONFIRMED]** **Only one guided spell: Sun Strike (`E E E`, `R`, `D`).** The other rules (any order, the D/F rotation, wrong spells) are explained on cards; the other nine recipes are looked up by the player (Recipes list, §17).
- **T-5 [CONFIRMED]** **Highlight what matters.** Whenever a step is about something on screen it is framed by a pulsing gold highlight: the enemy when it appears, the `TARGET` hint (name and icon) that says which spell it needs, the orb row, slot D when the spell lands in it, and the key to press (on screen and on the touch button).
- **T-6 [RECOMMENDED]** In a guided step a wrong key does nothing to the game state: the expected key flashes and a line says `PRESS E`. No penalty, no retry limit. Keys reach the real Invoker state only when they are the expected key, so orbs and slots always match what the tutorial says.
- **T-7 [RECOMMENDED]** Tutorial enemies are **training dummies**: standing still in lesson 2, walking slowly in lesson 4. No score, combo, accuracy, records or top-10 entries are ever changed by the tutorial. HP is shown only in lesson 4 and cannot reach Game Over.
- **T-8 [CONFIRMED]** In lesson 4 only the skill **name and icon** are shown (as in Practice), never the keys: the player looks the recipe up. In the tutorial the Recipes list can be opened with H even while enemies walk; they stop while it is open (tutorial only; in Practice it stays Ready / Game Over only).
- **T-9 [RECOMMENDED]** The rules taught are exactly the Practice rules (Core `InvokerState` for orbs / invoke / D-F slots; right / wrong decided by `skill == target`), so nothing learnt in the tutorial behaves differently in Practice. One simplification: in the tutorial every spell, Tornado included, is judged when it is cast (no projectile flight).
- **T-10 [RECOMMENDED]** On completion: a `TUTORIAL COMPLETE` card with `ENTER  PLAY PRACTICE` and `ESC  MENU`. Completion is remembered locally (like the records), e.g. for a checkmark on the button; it never blocks Practice.
- **T-11 [RECOMMENDED]** Architecture: a `TutorialSession` in the Practice layer (no SDL): an ordered script of steps, the current step, the expected key, the dummy enemies, its own `InvokerState`, the same right / wrong rule. Pure and unit-tested like `PracticeSession`. The presentation layer draws cards, key sequences and highlights.

### Script (4 lessons; about 2 minutes)

| # | Lesson | Steps (C = card + NEXT, G = guided keys; *highlighted* = gold frame) | Teaches |
|---|---|---|---|
| 1 | Elements | C: `QUAS  WEX  EXORT - YOUR THREE ELEMENTS` · G: `Q` `W` `E`, each new orb *highlighted* · C: `YOU HOLD 3 ORBS. A 4TH PUSHES OUT THE OLDEST` · G: `Q` | Q/W/E only load orbs; 3 at most, rolling |
| 2 | Sun Strike | A dummy walks in and stops, *highlighted*; C: `AN ENEMY! LOOK AT ITS TARGET` with the `TARGET: SUN STRIKE` hint (name and icon) *highlighted* · C: `SUN STRIKE  =  E E E` · G: `E E E` · C: `PRESS R TO INVOKE` · G: `R`, slot D *highlighted* as Sun Strike lands · C: `IT IS IN SLOT D. PRESS D TO CAST` · G: `D` → gold ring, `+1` · C: `RIGHT SPELL = ENEMY GONE` | The whole loop once, key by key |
| 3 | Rules | C: `ORDER DOES NOT MATTER: QQW = QWQ = WQQ` · C: `A NEW SPELL GOES TO D, THE OLD ONE TO F. BOTH CAST` (slots *highlighted*) · C: `WRONG SPELL = MISS. NO DAMAGE. TRY AGAIN` · C: `FORGOT A RECIPE? PRESS H FOR THE LIST` | The remaining rules, as cards |
| 4 | Your turn | C: `NOW YOU. READ THE TARGET, FIND THE KEYS` · 3 slow dummies, random targets; each new one and its `TARGET` hint *highlighted* for a moment; H opens the Recipes list (enemies stop); HP shown, a leak takes a heart but never ends the run | The full loop on your own |
| — | End | C: `TUTORIAL COMPLETE` · `ENTER  PLAY PRACTICE` / `ESC  MENU` | — |

## 25. Boss mode (update 1.2)

> **Since 1.9 (§32):** the tiers are renamed and rescheduled. What this section calls a *boss* of PLAY (a chain of 3) is now the **OVERLORD**; what it calls an *OVERLORD* (a combo fight inside PLAY) and the bosses of *Boss Fights* are now the **IMMORTALS**. Where §32 says otherwise, §32 wins.

**Status: CONFIRMED by the owner 2026-09-30 and implemented the same day** (`Practice/Boss.*`, tested in `Tests/PracticeTests`; drawn by `GameManager::RenderBoss*`; "triển khai 2 3 4": the proposals of the Combo/Boss discussion recorded in PLAN.md, taken as they were proposed). A separate mode next to Practice and the Tutorial. It trains the Invoker skill Practice cannot: **timing** delayed spells so they land together, and invoking 3–4 spells with only two slots. Practice rules (§§3–19) are unchanged; everything below applies to Boss mode only.

### Entering and leaving

- **B-1 [CONFIRMED]** Menu line **BOSS** (key B) opens the boss list; all three bosses can be chosen from the start (no unlocks). A fight starts at once. Esc / Back pauses the fight (Z-3b); the pause offers the boss list (nothing is saved for an abandoned fight).
- **B-2 [CONFIRMED]** Q/W/E/R/D/F work exactly as in Practice (Core `InvokerState`, no cooldowns). The player has **3 HP**. The Recipes list is not available during a fight (as in Practice). RECIPE HINT works as in §17 (see B-12).

### The boss

- **B-3 [CONFIRMED]** One boss per fight. It walks toward the player at its speed. When it reaches the player (the Practice hit line): **HP −1**, a running combo fails, and the boss is knocked back to `BOSS_RESET_X`. HP 0 = **defeat**.
- **B-4 [CONFIRMED, revised 2026-09-30]** The boss has **100% HP**. Only a combo attempt damages it, by how well its spells were timed (B-7, B-14); a single spell outside an attempt never does. A perfectly timed combo takes all 100% at once; otherwise it takes 2-3 combos. HP 0% = **victory**.

### Spell timelines (Boss mode only)

- **B-5 [CONFIRMED]** Initial tuning values, Dota 2 Invoker as the reference, all in one table in `Practice/Boss.h`:

| Spell | Boss mode behaviour |
|---|---|
| Tornado | Projectile from the player (700 px/s, aimed at the boss at cast time). On hit it **lifts the boss for 2.5 s**; the boss is **invulnerable in the air** and comes down on the same spot. A Tornado passes through a boss that is already in the air |
| Sun Strike | Lands **1.7 s** after the cast, where the boss was at cast time (radius 70 px) |
| Chaos Meteor | Lands **1.3 s** after the cast, where the boss was at cast time (radius 80 px) |
| EMP | Detonates **2.9 s** after the cast, where the boss was at cast time (radius 120 px) |
| Deafening Blast | Projectile from the player (700 px/s); hits the boss when it reaches it |
| the other five | No effect on the boss (drawn as usual) |

A spell "lands" on the boss when its impact happens within its radius of the boss's body (projectiles: when they reach it).

### Combos

- **B-6 [CONFIRMED]** Each boss has one combo: an ordered list of spells that always starts with Tornado. It is shown at the top right as spell names (and small icons) in order; the next spell is highlighted, done ones are marked.
- **B-7 [CONFIRMED]** Judging (fixed combos, option (a) of the discussion):
  1. An **attempt** starts when Tornado is cast while the boss is on the ground and no attempt is running. Before that, other spells do nothing (not a failure).
  2. During an attempt the player must **cast** the remaining spells **in the shown order**. Casting a spell out of order **fails** the attempt (`WRONG SPELL`). Casting the spell just cast again, or anything after the whole list has been cast, is ignored (a double tap never fails a combo).
  3. The boss lands `2.5 s` after the attempt's Tornado hit; the landing opens the **window** (1.2 s for boss 1, 1.0 s for bosses 2 and 3). **Each** follow-up spell is **graded** by when it lands (B-14): landing while the boss is still in the air (or before it was lifted) is a miss (`TOO EARLY`), outside its radius a miss (`MISSED`), not landed when the window closes a miss (`TOO LATE`). **A missed spell only loses its own share; it does not end the attempt** (owner 2026-09-30). A Tornado that never hits fails the attempt (`MISSED`).
  4. The attempt ends when every follow-up has been graded (or the window closes). **Damage = the average score of its follow-ups, in % of the boss's HP.** Damage above 0: **combo**, the boss is pushed back 200 px (at most to `BOSS_RESET_X`). Damage 0: the attempt failed (the reason of its first miss is shown).
  5. After an attempt ends, spells of that attempt still on their way do nothing. A new attempt starts with the next Tornado on a grounded boss.
- **B-8 [CONFIRMED]** Targeting v1: no free aiming; delayed spells land where the boss was when they were cast. The difficulty is the timing (free aiming is FUTURE).

### Bosses (v1)

- **B-9 [CONFIRMED]** Art v1: existing enemy sprites drawn larger and tinted, with an HP bar (owner art can replace them later).

| # | Boss | Combo | HP | Speed | Window |
|---|---|---|---|---|---|
| 1 | Stone Knight | Tornado → Sun Strike | 100% | 40 px/s | 1.2 s |
| 2 | Dark Wizard | Tornado → Chaos Meteor → Deafening Blast | 100% | 45 px/s | 1.0 s |
| 3 | Kitsune Queen | Tornado → EMP → Chaos Meteor → Deafening Blast | 100% | 50 px/s | 1.0 s |

- **B-10 [CONFIRMED]** **Boss 1 is the lesson** (point 8 of the discussion, built into the first boss instead of a separate tutorial lesson): a line explains `TORNADO LIFTS IT. LAND THE NEXT SPELL AS IT COMES DOWN`, and while the boss is in the air a **`CAST NOW`** cue appears exactly when casting the next spell would make it land at least `GREAT` (B-14). Bosses 2 and 3 show no cue.
- **B-14 [CONFIRMED, owner 2026-09-30]** **Timing grades.** A follow-up spell that lands on the grounded boss inside the window is graded by how long after the landing it hit: **PERFECT** ≤ 0.15 s → score 100 · **GREAT** ≤ 0.4 s → 60 · **GOOD** later in the window → 35 · a miss → 0. The grade rises above the boss as it lands (`PERFECT!` / `GREAT` / `GOOD`, or the miss reason), and the damage (`-60%`) shows next to the boss's HP bar. The ideal moment is 0.1 s after the landing (inside PERFECT, with a little room for being early, since early is a miss).
- **B-15 [CONFIRMED, owner 2026-09-30]** **Timing bars (RECIPE HINT on only).** With the hint on, each follow-up spell of the combo strip has a bar under its icon. From the attempt's Tornado cast (using the predicted landing while the Tornado flies, the real one once it hits) the bar **shrinks to empty at the ideal moment to cast that spell** (landing + 0.1 s − the spell's delay or flight time); around that moment the bar turns green and a line says `CAST <SPELL> NOW!`. Bars disappear once their spell is cast. As always with the hint, the fight gets no best time (B-12).
- **B-11 [CONFIRMED]** Timing is visible for every boss: a ring on the ground shows where each delayed spell will land and closes as its impact nears; the boss's shadow shows it is in the air.

### Results

- **B-12 [CONFIRMED]** Victory shows the fight time; the **best time per boss** is saved locally (like the records) and shown in the boss list. The recipe hint does not matter for it (§17 H-2, changed 2026-10-01). No leaderboard for Boss mode yet (FUTURE: its own board).
- **B-13 [CONFIRMED]** Architecture: a `BossSession` in the Practice layer (`Practice/Boss.*`, no SDL, time passed in as `dt`, unit-tested like `TutorialSession`); the Core and `PracticeSession` are untouched. The presentation draws the boss (lift, shadow, HP bar), impact rings, the combo panel, the grades, the timing bars (hint), the cue and the result screens.

## 26. PLAY mode — the main game (update 1.3)

> **Since 1.9 (§32):** the tiers are renamed and rescheduled. What this section calls a *boss* of PLAY (a chain of 3) is now the **OVERLORD**; what it calls an *OVERLORD* (a combo fight inside PLAY) and the bosses of *Boss Fights* are now the **IMMORTALS**. Where §32 says otherwise, §32 wins.

**Status: CONFIRMED by the owner 2026-09-30 / 2026-10-01** (discussion recorded in PLAN.md). PLAY is the main mode. It plays by the **Practice rules, renamed SURVIVAL** in the menu (§§3–19: instant judging, the Tornado projectile, wrong cast = MISS), with longer targets, gold and rewards on top. None of the Boss-mode delays (§25) apply here. SURVIVAL itself is unchanged.

- **P3-1 [CONFIRMED]** Menu: `PLAY` (Enter) · `SURVIVAL` (the former Practice, key S) · `BOSS FIGHTS` · `TUTORIAL` · … The Tutorial's end card starts PLAY.
- **P3-2 [CONFIRMED]** **Three kinds of enemy**, still one at a time (C-1):

| Kind | Target | Appears | Speed | Reaching the player | Points | Gold |
|---|---|---|---|---|---|---|
| Normal | 1 skill | otherwise | difficulty speed | −1 life | 1 | — |
| Elite | a **chain of 2** skills, in order | at random: 0 % at the start, rising to 30 % after 3 min | ×0.8 | −2 lives | 3 | 5 |
| Boss | a **chain of 3** skills, in order | every **10th** enemy | ×0.6 | −3 lives | 10 | 20 |

  Chains never repeat a skill; the first skill of a chain follows the enemy table (E-3) and is never the previous enemy's first skill. Elites and bosses are drawn larger (×1.4, ×2) and tinted.
- **P3-3 [CONFIRMED]** **Chains.** The HUD's TARGET hint shows the skill needed **now** (name, icon) and, for a chain, the whole chain as small icons (done ones marked, the current one highlighted). A correct cast of the current skill breaks it and moves to the next (counted as a correct cast, combo +1); the enemy dies when the last one is cast. A wrong cast is a MISS as in Survival: **the progress made on the chain is kept**. Tornado is judged when its projectile hits, as in Survival.
- **P3-4 [CONFIRMED]** **Rune after every boss** (random): **Regeneration** +1 life (max 5; not offered at 5) · **Frost** enemies move at 60 % for 15 s · **Double Damage** points ×2 for 20 s · **Bounty** +25 gold · **Shield** the next enemy that reaches the player does no damage. The rune's name is announced on screen; running ones show in the HUD.
- **P3-5 [CONFIRMED]** **Score and gold.** Points per kill (table) rank PLAY runs (leaderboard by **score**, this device for now; ties: more bosses, then longer time). **Gold** comes only from elites, bosses and Bounty; it is **banked across runs** on the device (saved as it is earned, never lost at Game Over) and is the currency of the future shop (§ to come). A run with the recipe hint on is ranked and earns gold like any other (§17 H-2).
- **P3-6 [CONFIRMED]** **Stages.** Every boss defeated starts a new stage: the background changes (tinted placeholder until the owner's background art arrives). Difficulty (speed, delay) follows the survival clock as in §14; tuning together with the new backgrounds is still open (PLAN.md).
- **P3-7 [CONFIRMED]** Lives start at 3 (as Survival), max 5. Game Over at 0 shows score, gold earned (and the bank), stage reached and time, and the rank on the PLAY board.
- **P3-8 [RECOMMENDED]** Architecture: the same `PracticeSession` with a mode (`Survival` / `Play`); Survival behaviour is exactly the previous Practice (its tests are unchanged). Chains, kinds, runes and gold live in the Practice layer (tested); drawing, saving the gold bank and the PLAY leaderboard in the presentation.

## 27. Shop and items (update 1.4)

**Status: CONFIRMED by the owner 2026-10-01** ("Ok làm đi" on the plan in PLAN.md "Next steps — planned 2026-10-01"). Items are bought with the PLAY gold (§26 P3-5) and **only work in PLAY**; Survival, Boss Fights and the Tutorial stay pure training. The effects are this game's, inspired by Dota 2 items, not copies of them; the icons are drawn from scratch.

- **I-1 [CONFIRMED]** **Shop** (Home, small button SHOP): every item with its icon, level, effect, cooldown and price; BUY / UPGRADE; EQUIP / UNEQUIP. Gold is spent from the bank at once and saved with it. No real money, ever.
- **I-2 [CONFIRMED]** **Permanent items** have two levels: buying gives level 1, an UPGRADE (a higher price) gives level 2 with its own name. **Consumables** are bought one unit at a time (at most 9 of each) and used up; the stronger consumables are their own items.

| Item | Level 1: effect · cooldown · price | Level 2: name · effect · cooldown · price |
|---|---|---|
| Blink Dagger (active) | the enemy walks back for 3 s · 40 s · 1500 | Swift Blink · 4 s · 30 s · 4000 |
| Refresher Orb (active) | the current chain loses 2 skills, at least 1 is left · 90 s · 3000 | Refresher Orb II · same · 60 s · 7000 |
| Eul's Scepter (active) | the enemy stands still for 2 s · 25 s · 1200 | Wind Waker · 3 s and pushed back 150 px · 20 s · 3500 |
| Black King Bar (active) | no life lost for 5 s · 120 s · 2500 | BKB II · 7 s · 90 s · 6000 |
| Hand of Midas (passive) | +50 % gold · — · 2000 | Midas II · +100 % gold · — · 6000 |
| Octarine Core (passive) | item cooldowns −25 % · — · 3500 | Octarine II · −40 % · — · 8000 |
| Aghanim's Scepter (passive) | after a boss, **choose** 1 of 2 runes · — · 4000 | Aghanim's Blessing · 1 of 3 · — · 9000 |
| Healing Salve (consumable) | +1 life (max 5) · — · 60 | — |
| Cheese (consumable) | +2 lives (max 5) · — · 300 | — |
| Smoke of Deceit (consumable) | enemies at 50 % speed for 8 s · — · 80 | — |
| Greater Smoke (consumable) | enemies at 50 % speed for 12 s · — · 200 | — |

- **I-3 [CONFIRMED]** **Six item slots in 2 rows of 3**, like the Dota 2 inventory, for the **right hand** while the left hand types Q/W/E/R/D/F: keys **U I O / J K L** (also numpad 7 8 9 / 4 5 6); on a touch screen the 2 × 3 slots on the right edge are buttons. The loadout is chosen in the shop and kept for every run; passive items work only while equipped.
- **I-4 [CONFIRMED]** **Using an item** (PLAY, Playing): an active item or consumable in a slot is used when its key / button is pressed and it is ready: off cooldown, and it has something to do (Blink / Eul's / Refresher need an enemy, Refresher a chain with more than one skill left, Salve / Cheese a missing life). Otherwise nothing happens and nothing is spent. Cooldowns run on the PLAY clock and restart with every run; Octarine shortens them when the item is used. A used consumable is gone from the inventory for good.
- **I-5 [CONFIRMED]** Effects in detail: Blink — the current enemy walks backwards at its own speed (never past its spawn point). Eul's — the current enemy does not move (Wind Waker also pushes it back 150 px at once). Refresher — counts as breaking skills of the chain (no points, no combo). BKB — while it lasts a leak costs no life (the enemy is still removed). Smoke — stacks with Frost. Midas — multiplies every gold gain (rounded). Aghanim — after a boss the game pauses on the rune choice; the player picks with 1 / 2 / 3 or a tap.
- **I-6 [CONFIRMED]** Items change what a run can reach; the PLAY leaderboard accepts it (owner 2026-09-30: everyone can buy them by playing).
- **I-7 [RECOMMENDED]** Architecture: `Practice/Items.*` (item table, inventory, buying and equipping — pure, tested) and the item effects inside `PracticeSession` in PLAY mode (tested); the shop screen, the in-game item bar, the icons and saving the inventory are presentation.

### Boss mode, update 1.5: all ten skills, longer combos  *(§25 continued)*

**Status: CONFIRMED by the owner 2026-10-01** ("thực hiện bản 1.5", the plan in PLAN.md). B-1 … B-15 stay as they are; this adds what the five other skills do in Boss mode and five new bosses. The boss art is still a placeholder (enemy sprites drawn larger and tinted) until the owner's boss sheets arrive (format in PLAN.md).

- **B-16 [CONFIRMED]** **Any spell can open a combo** (the first spell of the boss's list, cast while the boss is on the ground). Each later spell is one of two kinds:
  - **Landing step** (as before, B-7 / B-14): Sun Strike, Chaos Meteor, EMP or Deafening Blast **after a Tornado** in the same combo. It is graded by how soon after the boss comes down it lands.
  - **Quick step**: every other spell of a combo. It is graded **when it is cast**, by how long after the previous spell of the combo it came: **PERFECT** ≤ 1.2 s · **GREAT** ≤ 2.0 s · **GOOD** ≤ 3.2 s. After 3.2 s the step is missed (`TOO LATE`, only its share is lost) and the combo goes on to the next spell; a Tornado that is missed this way ends the attempt. A bar under the spell's icon shows the time left for the next quick step (always, not only with the hint).
- **B-17 [CONFIRMED]** **Holds.** When they are cast as the right spell of the combo: **Cold Snap** freezes the boss for 3.2 s (it does not move) · **Ice Wall** slows it to 30 % for 4 s · **Ghost Walk** makes it lose the player and stand still for 3.2 s. **Forge Spirit** and **Alacrity** have no effect on the boss's movement (the spirit and the aura are drawn). Outside a combo these spells do nothing to the boss.
- **B-18 [CONFIRMED]** **Bosses 4-8** (HP 100 %, as B-4; damage = the average grade of the follow-up spells):

| # | Boss | Combo | Speed | Window |
|---|---|---|---|---|
| 4 | Frost Troll | Cold Snap → Sun Strike | 45 px/s | — |
| 5 | Glacier Golem | Ice Wall → Chaos Meteor → Deafening Blast | 50 px/s | — |
| 6 | Fire Imp | Cold Snap → Alacrity → Forge Spirit | 55 px/s | — |
| 7 | Shadow Assassin | Ghost Walk → Tornado → Sun Strike → Chaos Meteor → Deafening Blast | 50 px/s | 1.2 s |
| 8 | Archon (final) | phase 1: Tornado → EMP → Sun Strike → Chaos Meteor → Deafening Blast · phase 2: Ice Wall → Cold Snap → Forge Spirit → Alacrity | 50 px/s | 1.4 s |

  In combos with a Tornado the landing spells are listed in the order they have to be cast (the longest delay first).
- **B-19 [CONFIRMED]** **Two phases** (boss 8): once its HP is at 50 % or less, its combo changes to the second list (announced on screen); it never changes back.
- **B-20 [CONFIRMED]** The boss list shows all eight bosses in compact rows (name, the combo as small icons, best time); keys 1-8, arrows + Enter, or a tap.

## 28. OVERLORD — Boss-Fights bosses inside PLAY, and material drops (update 1.6)

> **Since 1.9 (§32):** the tiers are renamed and rescheduled. What this section calls a *boss* of PLAY (a chain of 3) is now the **OVERLORD**; what it calls an *OVERLORD* (a combo fight inside PLAY) and the bosses of *Boss Fights* are now the **IMMORTALS**. Where §32 says otherwise, §32 wins.

**Status: CONFIRMED by the owner 2026-10-01 ("Đồng ý, làm đi") and implemented in 1.6.0.** The numbers (+50 points, +100 gold, +15 % speed / −10 % window per later overlord, the material table) are initial tuning values like the rest of PLAY. PLAY (§26) stays as it is: normal enemies, elites, chain bosses. An **OVERLORD** is a Boss Fights boss (§25) that appears inside a PLAY run and is fought by the Boss-mode rules; when it dies the stage is cleared and PLAY goes on.

### When

- **O-1** The **20th, 30th and 40th enemy** of a run are overlords instead of chain bosses: 20 → Dark Wizard (boss 2), 30 → Shadow Assassin (boss 7), 40 → Archon (boss 8). The 10th enemy stays a chain boss. (Owner: fixed milestones first; "change later if it does not fit".)
- **O-2** From the 50th enemy on, **every 10th enemy is an overlord chosen at random** among the eight bosses (never the same twice in a row), and each one is harder than the last: with n = 1 for the 50th enemy, 2 for the 60th and so on (overlords beaten so far minus two), its walking speed is ×(1 + 0.15 n) and its landing window ×(1 − 0.10 n), never below 0.6 s.
- **O-3** **Warning:** 2.5 s before the fight the screen announces `OVERLORD INCOMING` with the boss's name and a warning sound of its tier (tier 1 at the 20th enemy, tier 2 at the 30th, tier 3 from the 40th on: one, two, three horn calls, synthesised like every other sound). No other enemy is on the field during the warning or the fight; the combo strip of Boss Fights is shown, and the overlord already stands at the right edge without moving. The keys work during the warning (orbs and invoke, to prepare the first spells); casts and items have nothing to act on yet.

### The fight

- **O-4** The rules are §25 (lift, delays, landing and quick steps, grades, holds, phases, the boss's HP in %). The run's **lives, orbs and D/F slots carry over** into the fight and back out. The overlord reaching the player costs **1 life** and it is knocked back (not the 3 lives of a chain boss); the fight goes on until one side is dead. For the run's numbers a combo that deals damage counts as one correct cast (combo +1), a combo that fails or is broken as one incorrect cast, and a contact resets the combo to 0. The overlord's HP bar is at the bottom centre of the screen.
- **O-5** The **difficulty clock stops** during the warning and the fight (the run's time shown on the HUD keeps running), so the enemies after the fight are as fast as they were before it.
- **O-6** **Runes and items:** Frost and Smoke slow the overlord, a Shield or a Black King Bar blocks its contact, Double Damage doubles the points. Blink Dagger makes it walk back, Eul's Scepter makes it stand still (Wind Waker also pushes it back), Healing Salve / Cheese heal. **Refresher Orb is redefined for overlords: the next combo that deals damage deals ×2** (it stays armed until then; its cooldown starts when it is used). Hand of Midas, Octarine Core and Aghanim's Scepter keep their effects (gold, cooldowns, rune choice).
- **O-7** The RECIPE HINT setting works as in Boss Fights (recipe orbs under the combo, timing bars); a hinted run is ranked like any other (§17 H-2). There is no separate help for a first encounter (owner: the player decides).

### Victory, defeat

- **O-8** An overlord beaten: **+50 points**, **+100 gold**, a **rune** (as after a chain boss; a choice with Aghanim), **one material** (O-10), the next stage; PLAY goes on with its normal loop. It counts as a boss defeated.
- **O-9** Lives at 0: Game Over as usual; the screen adds `DEFEATED BY <NAME> - PRACTISE IT IN BOSS FIGHTS`.

### Materials and the shop

- **O-10** Every overlord beaten drops **exactly one material** (no chance involved; reaching it is the hard part). Three materials, by boss: bosses 1-3 → **Point Booster** · bosses 4-7 → **Mystic Staff** · boss 8 → **Sacred Relic** (so the 20th / 30th / 40th enemy give one of each). Materials are kept across runs and saved with the inventory (99 at most of each); the drop is shown when it falls and on the Game Over screen. **Shown as a picture (owner 2026-10-01):** each material has its own icon, and so has each rune; when a boss or an overlord falls, the icons of what it gave hop out of it and rest on the ground for a few seconds with their names. The same icons are used in the shop, the rune choice and the Game Over panel.
- **O-11** **Shop requirements** (on top of the gold; one material is used up by the purchase): **Aghanim's Scepter** needs a Point Booster. **Every level-2 upgrade needs a material:** Swift Blink and Wind Waker a Point Booster · BKB II, Midas II and Octarine II a Mystic Staff · Refresher Orb II and Aghanim's Blessing a Sacred Relic. Level 1 of the other items and all consumables need gold only. Items already owned stay owned. The shop shows the materials the player has and what an item needs (`NEEDS POINT BOOSTER`).
- **O-12** The PLAY leaderboard still ranks by score (§26 P3-5).

### Architecture

- **O-13** `PracticeSession` (PLAY) owns a `BossSession` for the overlord and hands the run to it for the fight (shared invoker state, lives, item effects); both stay in the Practice layer and are tested there. Materials live in `Practice/Items.*` (inventory, requirements, buying). The warning sounds, the banner, the drop and the shop lines are presentation. What both sessions share (play field, enemy table, the Tornado projectile) is in `Practice/Field.h`, so that `Practice.h` can include `Boss.h`.

## 29. Online leaderboards — Google Play Games, Android only (update 1.7)

**Status: CONFIRMED by the owner 2026-10-01 (discussion: Google's own leaderboard screen, two boards, hinted runs included, guests keep the device's board, built during the closed test). Switched on in 1.7.0 with the ids from the owner's Play Console (`android/play-games.properties`).**

- **OB-1** Android only, with Google Play Games Services. PC and the web keep the leaderboards of the device (§13, §26 P3-5); nothing changes there.
- **OB-2** **Signing in is never required.** Play Games signs a player with a profile in by itself when the app starts. A player who is not signed in (a guest, or offline) plays exactly as before and has the device's leaderboards only; there is no typed-in guest name and no guest entry on the global boards (owner: direction A).
- **OB-3** **Two global boards:** PLAY by **score** (larger is better) and SURVIVAL by **survival time** (sent in milliseconds, larger is better). No boss boards for now. Each board has Google's three time spans: today, **this week**, **all time**; the weekly reset is Google's.
- **OB-4** A result is sent at **Game Over**, like the device's leaderboard. A run with the recipe hint on is sent like any other (§17 H-2). The best PLAY score and the best survival time of the device are sent again whenever the player opens the global boards, so a player who signs in later still gets them; Play Games keeps a player's best result, so a lower one never replaces a higher one.
- **OB-5** **Screen:** the LEADERBOARD screen (this device) gets one button, `GLOBAL RANKING` (`SIGN IN FOR GLOBAL RANKING` when not signed in), only in a build that has Play Games. It opens **Google's own leaderboard screen**; when the player is not signed in it starts Google's sign-in first. The game draws no names or avatars itself.
- **OB-6** **Name and avatar** are the player's Play Games profile, shown by Google's screen. The game never reads, stores or sends them.
- **OB-7** **Privacy:** the app still requests no permission (the library talks through the phone's Google Play services). The privacy policy, the Data safety form and the store text must say that a signed-in player's results go to Google Play Games; the policy and the store text were updated with 1.7.0; the Data safety form is the owner's step in the Play Console.
- **OB-8** **Architecture:** presentation only. `Online.h/.cpp` (C++) offers `Available / SignedIn / Submit / ShowBoards`, which do nothing outside Android; the Practice layer does not know about it. On Android they call `ThreeElementsActivity`, which hands over to `OnlineBoards`: the Play Games one (`android/app/src/online`) when `android/play-games.properties` exists, an empty stand-in (`src/offline`) otherwise, in which case the library is not even part of the app.

## 30. Touch button layout (update 1.7.1)

**Status: CONFIRMED by the owner 2026-10-01 (move the cluster, split it into two groups, choose the size; the item bar moves too because the split needs its place).** Presentation only; nothing here changes a rule of play.

- **L-1** On a touch device, SETTINGS has a third row, **BUTTON LAYOUT**, which opens the layout editor. The default layout is exactly the one the game always had, so a player who never opens the editor sees no change.
- **L-2** **Groups.** `GROUPS: 1` — one cluster: Q W E R in a row, D F below it, half a key to the right. `GROUPS: 2` — two groups: **Q W E** (the elements, one thumb) and **R above D F** (invoke and cast, the other thumb). Inside a group the buttons keep the places they have in the whole cluster; only groups move. Switching between the two starts from that mode's default places (Q W E low on the left, R / D F low on the right, the items above Q W E).
- **L-3** **Moving.** A group is dragged with a finger (or the mouse). The item bar of PLAY (2 x 3 slots) is a third group and moves the same way. A group cannot leave the area between the HUD rows and the lane where the enemies walk, and cannot lie on another group: while it does, its frame is red, and dropped there it goes back to where it came from.
- **L-4** **Size:** small (72 px), medium (88 px, the default) or large (104 px), for all six buttons together; the item bar keeps its size. If the larger buttons no longer fit where the groups are, the groups go back to the default places of their mode.
- **L-5** `RESET` restores one cluster, medium, at the default place. `DONE` (or Back / Esc / Enter) closes the editor. The layout is saved with the settings on the device, at once.
- **L-6** The orb row in the middle of the screen moves sideways to stay clear of the groups (centre first, then right, then left); the editor shows it where it will be.
- **L-8 [owner 2026-10-01, update 1.7.2]** **On a touch screen the D and F buttons are the skill slots.** Each shows the icon of the skill it holds and is tapped to cast it; the key's letter stays small in a corner. An empty slot is an empty tile with the letter. The two slots that used to sit under the orb row are not drawn on a touch screen (they said the same thing twice); the three orbs stay. A button glows for a moment when a new skill enters it. With a keyboard nothing changes: there are no touch buttons, so the slots stay under the orbs. In the Tutorial, what points at a slot points at its button.
- **L-7** **Architecture:** the geometry is `TouchLayout.h/.cpp` (no SDL, tested with the Practice tests); `GameManager` draws, drags and saves. `--touch` (development only) shows the touch buttons on a PC.

- **L-9** **The buttons answer the finger (1.9.11, owner).** A pressed button lights up in its colour (Q / W / E their element's, R gold, D / F white) for 0.16 s, its key shows the pressed frame of its icon and sinks 2 px, and a ring in the same colour spreads 12 px around it and fades in 0.3 s. A hardware key lights its on-screen button the same way when the buttons are shown. Presentation only.

## 31. Guide and first-time tips (update 1.8)

**Status: CONFIRMED by the owner 2026-10-01 (both layers; GUIDE as its own button; the list of tips as proposed).** The Tutorial (§24) teaches orbs, invoke and cast; everything PLAY adds is taught here. Presentation only.

- **G-1** **First-time tips.** The first time something new happens in a PLAY or SURVIVAL run, a small card explains it in at most four short lines, with its icon when it has one. **The run waits** while the card is on screen. Only its `GOT IT` button (or Enter / Space / Esc / Back) closes it: the gameplay keys do not, so a player hammering Q / W / E cannot skip it unread. Each card is shown once per device; several can queue up and are shown one after another.
- **G-2** The cards and when they appear: **ELITE** (the first elite) · **BOSS** (the first chain boss) · **RUNE** (the first rune, naming the one received) · **GOLD** (the first gold) · **ITEMS** (the start of the first PLAY run with an item in a slot) · **OVERLORD** (the first `OVERLORD INCOMING`) · **MATERIAL** (the first material, naming it) · **NEED HELP?** (three wrong casts in a row while the recipe hint is off: it points at the HINT button). No cards in the Tutorial or in Boss Fights.
- **G-3** SETTINGS has a row **TIPS**: it makes every card show once more.
- **G-4** **GUIDE**: its own button in the row of small buttons of the menu. A reference in five tabs, each line short, the numbers taken from the rules: **BASICS** (orbs, invoke, slots, the two modes) · **ENEMIES** (normal / elite / boss / overlord: spells needed, lives lost, points, gold) · **RUNES** (the five, with icons) · **ITEMS** (the eleven, with icon, effect, cooldown and upgrade: the shop's own texts) · **BOSSES** (the combo, the Tornado lift and the spell delays, the grades, quick steps, holds, the timing bars). Arrows / Tab change the tab, Esc or CLOSE leaves. RECIPES stays a separate button.
- **G-6** **The buttons can be moved (1.9.12, owner).** On a touch screen, the first Game Over of PLAY or SURVIVAL shows a card YOUR BUTTONS ("buttons in the way of your thumbs? Move them, split them in two or make them bigger") with two buttons: GOT IT and CHANGE NOW, which opens the button layout editor at once. Shown once, like every card; TIPS shows it again. In SETTINGS the row is called **MOVE BUTTONS** (it was BUTTON LAYOUT), with "drag them - split in two - 3 sizes" under its name, and the line under the rows explains the row that is selected. The Tutorial is unchanged: it teaches the spells, and the buttons only bother a player after some real play.
- **G-5** All of it is English like the rest of the game (the pixel font has no Vietnamese letters).

## 32. Tiers: Normal, Elite, Overlord, Immortal (update 1.9)

**Status: CONFIRMED by the owner 2026-10-02** (the names and "only Immortals drop materials" 2026-10-01; the five combos, the schedule, the chance, "replace the Boss Fights roster", the drops, the asset clean-up and the enemy sizes 2026-10-02). The numbers are initial tuning values like the rest of PLAY. Where this section and §25 / §26 / §28 disagree, this section wins; everything it does not mention stays as written there.

- **M-1** **Names.** "Boss" was too general a word (owner). The four tiers of PLAY are **NORMAL** (1 spell), **ELITE** (a chain of 2), **OVERLORD** (a chain of 3: the *boss* of §26) and **IMMORTAL** (a combo fight with a life bar: the *OVERLORD* of §28). The menu entry *BOSS FIGHTS* is **IMMORTALS**. In the code the fight engine keeps its name (`BossSession`); `EnemyKind::Overlord` is the chain of 3, and everything named `Immortal` is the combo fight.
- **M-2** **A fixed schedule** (it replaces the random elites of P3-2 and the boss on every 10th enemy). The first **9** enemies of a run are normal. From the 10th on, every 5th enemy is special: the **20th, 30th, 40th ...** is an **overlord**, the others of them (the **10th, 15th, 25th, 35th ...**) an **elite**. Every other enemy is normal. (Until 1.9.3 the first elite was the 15th; the owner left the choice between 10 and 15 open and 10 was taken: nine identical enemies are enough of a warm-up, the first chain comes at a round number, and the first Immortal can come sooner.) Leak damage, points, gold, speed and size of each tier are unchanged (§26); an overlord still leaves a rune and starts the next stage.
- **M-3** **The Immortal comes by chance.** A run has an *Immortal chance*, 0 % at the start and shown on the HUD (bottom centre: `IMMORTAL`, a bar, the percentage). Beating an elite adds **10 %**, beating an overlord **20 %** (at most 100 %); one that reaches the player adds nothing. Right after each addition the chance is rolled. On a hit, the Immortal is the next thing to appear, before the next enemy, and the chance goes back to 0 %. It is an **extra**: it takes no enemy number, so the schedule of M-2 does not move. Beating every elite and overlord, the chance is 10, 20, 40, 50, 70, 80, 100 % after the 10th, 15th, 20th ... 40th enemy: the first Immortal usually comes around the 15th to 25th enemy and never later than the 40th. Survival has no elites, overlords or Immortals.
- **M-4** **Which one.** The first five Immortals of a run come in the order of M-6 (combos of 4, 5, 6, 7, 8). From the sixth on it is a random one of the five, never the same twice in a row, each one 15 % faster with a 10 % shorter landing window than the one before (O-2's scaling, never below 0.6 s).
- **M-5** **The warning** lasts **4 s** (2.5 s before): `IMMORTAL INCOMING`, its name, the horn (one call for a combo of 4, two for 5 or 6, three for 7 or 8), and its combo strip at the top right, so the first spells can be prepared. The fight itself is §28's (lives and slots of the run, contact costs 1 life, the difficulty clock stops, the items act as in O-6). Reward: +50 points, +100 gold, a rune, the next stage, and materials (M-7). Beating an Immortal does not add to the chance.
- **M-6** **The five Immortals** replace the eight bosses of §25 in the menu and in PLAY. Their combos are the ones Invoker players use most, in rising length:

  | # | Immortal | Combo | Speed, window |
  |---|---|---|---|
  | 1 | **Rimefang** | Tornado → EMP → Chaos Meteor → Deafening Blast | 50 px/s, 1.0 s |
  | 2 | **Cindermaw** | Tornado → EMP → Sun Strike → Chaos Meteor → Deafening Blast | 50 px/s, 1.4 s |
  | 3 | **Gravehorn** | Tornado → Sun Strike → Chaos Meteor → Deafening Blast → Cold Snap → Forge Spirit | 45 px/s, 1.2 s |
  | 4 | **Voltara** | Tornado → EMP → Chaos Meteor → Deafening Blast → Chaos Meteor → Deafening Blast → Ice Wall | 45 px/s, 1.2 s |
  | 5 | **The Hollow King** | Forge Spirit → Alacrity → Ice Wall → Cold Snap → Tornado → Sun Strike → Chaos Meteor → Deafening Blast | 40 px/s, 1.2 s |

  A combo may have up to **eight** spells. The combo strip uses smaller tiles from six spells on. Damage is still the average of the grades of all the follow-ups, so one perfect combo still wins. None of the five is guided or has two phases (the rules for both stay in the engine and in the tests). Best times are kept per Immortal in a new file (`immortals.txt` / `threeElements_immortalTimes`); the times of the eight old bosses are not shown any more.
- **B-21** *(§25 continued)* **Long combos.** (a) A spell catches the landing once: Sun Strike, Chaos Meteor, EMP or Deafening Blast is a *landing step* only the first time it appears after the Tornado; the same spell again after that Tornado (the "Refresher" part of Voltara's combo) is a *quick step*, graded when cast like any other. (b) When the window after the landing closes, the landing steps not yet scored are missed (TOO LATE) as before, and the ones never cast are **passed over**: the combo goes on with the quick steps after them, whose time starts at that moment.
- **M-7** **Materials.** Only Immortals drop them. Rimefang drops a **Point Booster**, Cindermaw a **Mystic Staff**, Gravehorn a **Sacred Relic**; Voltara and The Hollow King drop **two random materials**. The shop's needs are unchanged (O-11).
- **M-8** **Enemy sizes.** The enemy sheets come from different packs: next to a player of about 106 px some bodies were only 30 to 40 px tall. Each enemy has a whole-number size (so the pixels stay square): knight ×3; goblin, skeleton, dark wizard, eye and mushroom ×2; the others ×1. Every enemy is 66 to 102 px tall, **smaller than the player on purpose**; elites are ×1.4 and overlords ×2 of that. (1.9.2 tried 99 to 126 px with elites ×1.25 and overlords ×1.6; the owner took it back in 1.9.3: "if every one is that large, there is no difference left when a boss appears". The tiers must differ at a glance. Do not enlarge the normal enemies again.) The hit box grows with it; the contact line is measured from the body's front as before, so the time an enemy takes to arrive does not change. **Presentation, 1.9.2:** the player and every enemy have a small shadow on the ground (drawn in code; it stays on the ground under a flying enemy; made smaller in 1.9.3, about a third of the body's width), and an enemy beaten in a run fades out drifting up for 0.3 s instead of vanishing in one frame (after the wait of X-5 when the spell is still on its way).
- **M-9** **Art and assets.** The Immortals have the owner's art (the first three since 1.9.0): a moving sheet and a "got hit" sheet (4 × 4 frames). It runs while it walks, stands while it is lifted, frozen or pushed back, and flinches when a spell of the combo scores. Voltara (a flying character: its moving sheet hovers above the ground) and The Hollow King got theirs in 1.9.1, so all five have their own art. `art/make_bosses.py` brings the two sheets of a character to one scale and to the size it is drawn at, mirrors them (the originals face right) and stores them with a palette. Ice Wall has the owner's sprite (one pillar in front of the player). **Originals stay in `art/` at full quality; `assets/` only holds files at the size the game draws them**: the assets folder went from 21 MB to under 4 MB.
- **M-11** **Partial credit for a chain (1.9.5, owner).** An elite or an overlord that reaches the player costs **one life per skill still on its chain**: an untouched overlord 3, with one skill broken 2, with two broken 1; an untouched elite 2, with its first skill broken 1. (Before, the full 3 or 2 whatever had been done; the owner found 3 lives for an overlord nearly beaten unfair.) A wrong cast changes nothing; a Refresher Orb that removes skills of the chain lowers the cost too. Shield and Black King Bar still take the whole hit. Normal enemies and Survival: 1 life, as always.
- **M-10** **Guide and tips** (§31) use the new names: the cards ELITE and OVERLORD say that beating one raises the Immortal chance; the GUIDE's ENEMIES tab lists the schedule and the chance, and its last tab is IMMORTALS.

## 33. World ranking of the web version (update 1.9.14)

**Status: CONFIRMED by the owner 2026-10-03** (a name typed by the player; separate from the Android boards; basic protection; this week and all time; the privacy policy updated).

- **W-1** The Android version keeps Google Play Games (§29). The web version has its own boards on a small server of ours: a Cloudflare Worker with a D1 database (`web/boards`). They are not shared.
- **W-2** Two boards: **PLAY** by points and **SURVIVAL** by time (sent in milliseconds), each shown for **this week** (from Monday 00:00 UTC) and **all time**: the ten best names, each with its best result.
- **W-3** **The name.** The first time the player opens WORLD RANKING (a button of the LEADERBOARD screen, Enter on a keyboard) the browser's own text box asks for a name: 2 to 12 of A-Z 0-9 space - . in capitals, a few offensive words refused. It is kept in the browser (`threeElements_webName`) and can be changed (CHANGE NAME / key N). Without a name nothing is sent. When a name is set, the device's best PLAY and SURVIVAL results are sent once.
- **W-4** Every finished PLAY or SURVIVAL run is sent at Game Over (with a name). Nothing comes back to the game: a refused or failed send changes nothing.
- **W-5** **Basic protection** (owner): the server refuses a PLAY score above 8 points a second of the run (plus 200), a SURVIVAL time that does not match the run's length, more than 30 runs an hour from one address (stored only as a salted hash), and sends from any page but the published site. A fake but plausible score can still be sent; replaying the run on the server from its seed and keys would stop that and can be added later.
- **W-6** The world ranking shows LOADING, NO CONNECTION or NO RUNS YET when there is nothing to list; the player's own name is gold. The button only exists in a web build given the server's address (`BOARDS_URL` in `web/deploy.conf`); PC and Android never show it.
- **W-7** Privacy: what is sent and kept is described in the policy page (section World ranking); a name is removed on request or when offensive.
