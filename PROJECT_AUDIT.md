# PROJECT_AUDIT.md — Three Elements

Audit date: 2026-09-21. Repo state: branch `main`, HEAD `1c81f62` (2025-02-16), plus uncommitted edits to `GameManager.cpp` and to tracked build outputs.

**Method and limits.** Everything below comes from reading the source, project files, assets and git metadata. The game was **not built and not run** in this session (building would rewrite tracked `.obj/.exe/.pdb` files). Statements about runtime behaviour are inferred from code and are marked *(unverified)* where that matters. No code was changed.

Companion docs: [PLAN.md](PLAN.md) (roadmap), [CLAUDE.md](CLAUDE.md) (older hand-over, Vietnamese; its "cards vs keys" open question is now **decided: keys**).

---

## 1. One-paragraph summary

A ~1,500-line C++14 / SDL2 / SDL2_image side-scrolling prototype. It implements the *input half* of an Invoker-style game: Q/W/E fill a 3-slot rolling list, R resolves the sorted triple into one of 10 spells via a lookup table, and the result is pushed into D/F slots (correct Invoker swap semantics). Icons for the keys and slots are drawn. A 12-layer parallax background scrolls, and 7 enemy types spawn every 5 s and run left. **There is no casting, no enemy↔spell link, no enemy removal, no HP, no score, no game state.** The code is a single `GameManager` singleton with a 230-line `LoopGame()` that loads assets, runs the loop and tears down. Roughly: input/recipe logic ≈ done, gameplay loop ≈ absent.

---

## 2. Repository layout and build systems

```
/                              solution root
├─ Three Elements.sln          VS 2019/2022 solution, 1 project
├─ CLAUDE.md, README.md        docs (README = original author note + Figma link)
├─ Dependencies/               vendored SDL2 2.0.16, SDL2_image 2.0.5, SDL2_ttf (Windows x64 + x86 .lib/.dll, headers)
├─ Three Elements/             project dir (sources, assets, .vcxproj)
│  ├─ *.cpp / *.h              9 .cpp, 10 .h
│  ├─ assets/                  1.4 MB total: background, enemies, keyboard, skill, main.bmp, bg.png, bk.png
│  ├─ x64/Debug, Debug/        intermediate build output — TRACKED in git
├─ x64/Debug/                  exe + SDL2.dll, SDL2_image.dll, SDL2_ttf.dll + pdb — TRACKED in git
└─ Debug/                      empty
```

- **Only build system: Visual Studio project** (`Three Elements/Three Elements.vcxproj`, toolset v143, `/std` default = C++14, Console subsystem). **No CMake, no Makefile, no CI, no tests.** (`Dependencies/include/SDL_config.h.cmake` is just an SDL header template.)
- **Only `Debug|x64` is actually configured.** Include/lib paths appear only there (lines 82-83, 122, 127-128 of the vcxproj):
  - `IncludePath`/`LibraryPath` → `D:\Dev\Code\3Elements\ThreeElements\Dependencies\...` (absolute, this machine).
  - `AdditionalLibraryDirectories` → `C:\Users\hawon\source\repos\ThreeElements\Dependencies\lib\x64` (stale/other location).
  - `Debug|Win32` include path → `E:\Study\AIT\Game engine\...` (another old machine); no libs listed.
  - `Release|x64` and `Release|Win32` have **no** include path, **no** lib path, **no** SDL libs → they cannot build.
  - Linked libs: `SDL2.lib; SDL2main.lib; SDL2_image.lib`. SDL2_ttf is vendored but not linked (its use in code is commented out). No SDL_mixer.
- Output dir defaults to `$(SolutionDir)x64\Debug\` (root), intermediates to `Three Elements\x64\Debug\`. Only 3 DLLs sit next to the exe (`SDL2`, `SDL2_image`, `SDL2_ttf`). SDL2_image 2.0.5 loads `libpng16-16.dll`/`zlib1.dll` at runtime for PNG; those live in `Dependencies/lib/x64` but not in `x64/Debug` *(unverified: how the exe currently finds them, probably PATH or VS debugger)*.
- **Working directory matters:** every asset path is relative (`assets/...`), so the process must start in `Three Elements/`. `.vcxproj.user` is empty, so this relies on the VS default working dir.
- **git hygiene:** 237 tracked files; ~37 are build outputs (`*.obj, *.ilk, *.pdb, *.idb, *.log, *.tlog, *.exe`) that `.gitignore` already lists but that were committed earlier, so the ignore has no effect. They show as modified after every build. `Three Elements.vcxproj.user` is also tracked. Vendored `Dependencies/` (x86 + x64 libs, SDL_test headers) is tracked and is bigger than needed.

---

## 3. Architecture (current implementation)

### 3.1 Classes

| Class | File(s) | Role | Notes |
|---|---|---|---|
| `GameManager` | GameManager.h/.cpp (520 lines) | Singleton (`getInstace()` sic). Owns window/renderer, background layers, player, key icons, skill icons, enemy list, spawn logic, **and the whole main loop** | Instantiated at static-init time by a global in `main.cpp:6`. Never deleted. |
| `BaseObject` | BaseObject.h/.cpp | Texture + rect + sprite-sheet clip list + time-based frame animation. Owns `SDL_Texture*` (freed in dtor via `free()`) | Non-virtual dtor. `LoadImg` is `virtual` but derived classes *overload* rather than override it. Applies a colour key (175,175,175) to **every** image (`BaseObject.cpp:38`). |
| `MainPlayer : BaseObject` | MainPlayer.h/.cpp | Player sprite (8 frames, `main.bmp`) **and** input/recipe state: `elements`, `slotD`, `slotF`, `m_KeyRActive`, two `Skill` members | Input handling lives here, coupled to `SDL_Event`. |
| `Skill : BaseObject` | Skill.h/.cpp | Icon sprite **and** the recipe table `spellMap` (10 entries string→name). Enums `Spell`, `Element` | Every `Skill` builds its own copy of `spellMap` in its ctor. `Spell` enum is effectively unused. |
| `EnemyObject : BaseObject` | Enemy.h/.cpp | Sprite-sheet enemy, moves left, remembers texture path as identity | Re-declares 6 members of the base (see §11). |
| `Keyboard : BaseObject` | Keyboard.h/.cpp | 2-frame key icon (64×32 = two 32×32 frames). Has `KeyType` enum | `SetType` is never called, so `m_type` stays `KEY_NONE`. |
| `ImpTimer` | ImpTimer.h/.cpp | Stopwatch used to cap FPS (start/get_ticks; pause API unused) | Fine, keep. |
| `ThreatObject` | ThreatObject.h/.cpp | Empty class, never instantiated | Dead. |
| `IKeyHandler.h` | | Entire content commented out | Dead. |

### 3.2 Dependency relationships

```
Define.h (SDL, SDL_image, stdio, string)
   └─ BaseObject ─┬─ Skill ─────────────────┐
                  ├─ Keyboard ──────────────┤
                  ├─ EnemyObject            ├─ MainPlayer (has-a Skill ×2; includes Keyboard.h)
                  └─ ThreatObject (dead) ───┘
GameManager  has-a: MainPlayer, Skill, BaseObject(bg),
             vector<Keyboard*> (always empty), vector<EnemyObject*>, map<string,Skill*>, 12 bg textures
main.cpp     → GameManager only
```
Implicit includes: `GameManager.h` uses `std::map` (via `Skill.h`), `GameManager.cpp` uses `std::sqrt/pow/rand` with only `<algorithm>` included (relies on transitive includes). `using namespace std;` appears in several headers.

### 3.3 Ownership map (who frees what)

| Resource | Created | Freed | Status |
|---|---|---|---|
| `GameManager` | static global via `new` | never | leak by design (process exit). Its dtor (deletes `skillMap`) never runs. |
| Window / renderer | `InitSDL` | `Close()` | OK |
| 12 background textures | `loadBackgroundLayers` | only implicitly via renderer destruction | not explicitly destroyed |
| Player / bg `BaseObject` texture | `LoadImg` | dtor (never runs; singleton) | |
| 10 `Skill*` in `skillMap` | `LoopGame` (`new`) | `~GameManager` (never runs) | leak |
| 11 named `Skill*` locals (`sNO_SPELL … sDEAFENING_BLAST`) | `LoopGame:119-129` | never; never used | pure leak |
| 6 `Keyboard*` (Q,W,E,R,D,F) | `LoopGame:132-137` | never (`m_Keylist` is empty, so the cleanup loop deletes nothing) | leak |
| `EnemyObject*` | `respawnEnemy` (`new`) | never (`Close()` cleanup is commented out) | leak |
| Each enemy's texture | `IMG_Load` per spawn | via `BaseObject::free` in dtor (never runs) | per-spawn disk load, no cache |

`BaseObject` holds a raw `SDL_Texture*` and has a user dtor but no copy control (rule-of-three violation): copying any `BaseObject`-derived value would double-free. Currently nothing copies them.

---

## 4. Current runtime flow

**Startup**
1. `main.cpp:6` global → `GameManager` constructed (constructs `MainPlayer`, `Skill`, `BaseObject`; no SDL calls yet).
2. `main()` → `InitSDL()` (SDL_Init VIDEO; scale-quality hint `"1"` = linear; 928×544 window, `SDL_RENDERER_ACCELERATED`, no vsync flag; `IMG_Init(PNG)`; load 12 bg layers) → `LoopGame()` → `return 0`. If init fails, it silently returns 0.

**`LoopGame()` (GameManager.cpp:102-335) — one function does everything**
1. Load `assets/bg.png` (drawn nowhere) and `assets/main.bmp` (player), `set_clips`, place player at (10, 385).
2. `m_skill.initializeSpellMap()`; create 10 `Skill` in `skillMap` keyed by recipe; also create 11 unused locals; load 10 skill icons by recipe key (e.g. `skillMap["QQQ"]` ← ColdSnap.png).
3. Create 6 `Keyboard` icons; only Q/W/E go into `keyMap`. R/D/F icons are loaded but never drawn.
4. Local `lastRespawnTime = now-5000`, local `respawnInterval = 5000`.
5. **Frame loop** (`while(!bStop)`), per frame in this order:
   1. `fps_timer.start()`
   2. Poll all events; `SDL_QUIT` sets stop; **every event** goes to `MainPlayer::handleKeyPress`.
   3. Clear to white.
   4. `updateBackgroundLayers()` then `renderBackgroundLayers()` (12 layers, speed 0.1·(i+1) px/frame, two copies for seamless wrap).
   5. Player `Render` (time-based animation, 100 ms/frame, `SDL_GetTicks`).
   6. For each enemy: `Render()` then `UpdatePos()` (update and render interleaved; no separate update phase).
   7. Draw up to 3 key icons at (50/100/150, 150) for the current `elements` (render-then-`SetPos` ordering).
   8. If `m_KeyRActive` (set on first R, never cleared): draw `m_player.skill` (has no texture), then slot D icon at (150,250) and slot F icon at (250,250).
   9. If `now - localLast >= 5000` → `respawnEnemy(now)`.
   10. `SDL_RenderPresent`, then `SDL_Delay` to hold **25 FPS** (`FRAME_PER_SECOND = 25`).
6. After the loop: delete `m_Keylist` (empty), `Close()` (destroy renderer/window, `IMG_Quit`, `SDL_Quit`).

---

## 5. Current input flow

- Only `SDL_KEYDOWN` is handled, and **only in `MainPlayer::handleKeyPress(SDL_Event)`** (MainPlayer.cpp:41-63), using `keysym.sym` (layout-dependent keycode, not scancode). No mouse, touch, window or gamepad events are handled anywhere.
- `Q/W/E` → `elements.push_back(QUAS|WEX|EXORT)`; after any keydown, if `elements.size() > 3` the oldest is erased (rolling window of 3, like Invoker orbs).
- `R` → `getCombineComb()`; sets `m_KeyRActive = true` and returns (even if the combo was invalid).
- **`D` and `F` are not handled at all** (there is only a comment). Slots are only *filled* by R; nothing casts them.
- `key.repeat` is **not** checked, so holding Q pushes many Q's (keyboard auto-repeat).
- No timestamps are recorded (needed later for reaction/APM stats; `SDL_Event` does carry `timestamp`).

## 6. Current spell/recipe flow

1. `elements` (vector of enum) → `getElementComb()` → string like `"QWE"` in **press order** (used for the HUD).
2. `getCombineComb()` copies it, **sorts it** (`std::sort`), so order is irrelevant — this already satisfies the "Q/W/E order does not matter" requirement.
3. Lookup in `Skill::spellMap` (10 keys, all already in sorted form): QQQ Cold Snap, QQW Ghost Walk, EQQ Ice Wall, WWW EMP, QWW Tornado, EWW Alacrity, EEE Sun Strike, EEQ Forge Spirit, EEW Chaos Meteor, EQW Deafening Blast. (Standard Invoker table.)
4. Hit → `saveSpellToSlot(combo)`: D→F shift only when D is non-empty, D≠F and D≠new; then D=new. This reproduces Invoker semantics (re-invoking the D spell does not shift; re-invoking the F spell swaps). Miss (fewer than 3 elements) → `activeSpell = NO_SPELL`, slots unchanged.
5. Rendering: `skillMap[slotD]` / `skillMap[slotF]` icons.
6. `elements` is **not** cleared after invoke (matches Invoker: orbs persist).

Recipes are stored as strings, spells as display-name strings; the `Spell` enum is not used for identification.

## 7. Current enemy flow

- Table in `GameManager.h:59-70`: 7 `(path, frameCount)` pairs — mushroom_run(8), goblin_run(8), eyes_fly(8), skeleton(4), fire_wiz(8), nec_walk(10), worm_run(9). Frames are 150×150 px sheets.
- **There is no enemy type id and no link to any skill.** Identity = the texture path string, compared to avoid duplicates on screen.
- Spawn (`respawnEnemy`, GameManager.cpp:397-458): every 5 s, pick a random type *not currently in the list* (so max 7 enemies, one per type), `new EnemyObject`, `LoadImg` from disk, position (800, 400), `SetVal(5,0)`. The "minimum distance" do/while never changes `posX`, so it is a no-op.
- Movement (`Enemy.cpp:16-25`): `rect_.x -= 5` per frame (= 125 px/s at 25 FPS). At `x < 0` it is teleported back to x = 800 and keeps running. **Nothing ever removes an enemy**, so after 7 spawns (~35 s) spawning stops. `// TODO:` at `Enemy.cpp:21` is exactly where "reached the player / left the screen" would go.
- Enemy animation advances one frame **per rendered frame** (`Enemy.cpp:66`), i.e. frame-rate-coupled, unlike the player animation which is time-based.
- No collision, HP, or interaction with the player.

## 8. Rendering / update flow (summary)
Single-threaded immediate mode: `SDL_RenderClear` → background (12 stretched layers) → player → enemies → HUD icons → `SDL_RenderPresent`; fixed 25 FPS via `SDL_Delay`. Update and draw are intertwined (enemy `Render` then `UpdatePos`; background update precedes draw). Window is fixed 928×544, no resize/high-DPI/logical-size handling. Scale hint is *linear*, which will blur pixel art once scaling is introduced.

## 9. Asset management

Inventory (`Three Elements/assets`, 1.4 MB total — very web-friendly):

| Group | Count | Size | Notes |
|---|---|---|---|
| Background layers | 12 PNG | 928×793 each | drawn into a 928×544 rect → **vertically squashed** (`GameManager.cpp:374`). Loaded through a separate path from `BaseObject`. |
| `bg.png` (928×544), `bk.png` (1024×512) | 2 | | `bg.png` is loaded but not drawn; `bk.png` unused |
| Enemy sheets | 13 PNG, 1 row each | 600–1500 px wide, 64–150 px tall | **7 used**; unused: bat_fly (576×64), dark_wiz (1200×150), kitsune_run (1024×128), knight_run (768×64), mush (1200×150), nec_walk_bg (960×96). Frame counts for unused ones are not defined in code (widths suggest 8/8/12/8/10 frames, *unverified, confirm visually*). Frame heights differ (64…150), so a fixed y=400 will not ground them all. |
| Key icons | 6 PNG, 64×32 (2 frames of 32×32) | | Q/W/E drawn; R/D/F loaded, never drawn |
| Skill icons | 10 PNG, 64×64 | | all 10 loaded and drawable |
| Player | `main.bmp` 1792×112 = 8 frames of 224×112 | 784 KB | |

Loading model: no asset manager. Each object loads its own texture with `IMG_Load` → `SDL_CreateTextureFromSurface`; enemies reload from disk on every spawn; paths are string literals scattered across `GameManager.h/.cpp`. All paths referenced in code match the file names **including case** (checked by reading) — good for Linux/Emscripten/Android. Asset provenance/licences are not recorded in the repo (owner states they are free to use).

Load failures for keys, skills, player are not checked (only `bPlayer`/`bBkgn` booleans; `bBkgn` unused).

---

## 10. Existing TODOs / dead switches
`grep` finds one `TODO` (`Enemy.cpp:21`), one `#if 0` block (`main.cpp:19-90`, old bootstrap incl. `system("pause")`), and large commented-out blocks in `InitSDL` (SDL_mixer/SDL_ttf setup) and `Close()` (the real cleanup).

---

## 11. Known bugs (verified by reading unless noted)

| # | Sev. | Where | Problem |
|---|---|---|---|
| B1 | High | GameManager.cpp:446 (uncommitted) | `printf("%s", enemy->GetPath())`: `GetPath()` returns `std::string`, `%s` needs `const char*` → undefined behaviour. |
| B2 | High for the product | MainPlayer.cpp:42 | `SDL_KEYDOWN` auto-repeat not filtered: holding Q floods `elements`. Fatal for a typing/practice game. |
| B3 | High for the product | MainPlayer.cpp:41-58 | D/F never handled; no cast exists. |
| B4 | High | Enemy.cpp:19-23, GameManager.cpp:403-415 | Enemies wrap instead of being removed; unique-type spawn rule means spawning stops after 7. |
| B5 | Med | GameManager.h:49, .cpp:194-195, 398, 456 | Local `lastRespawnTime`/`respawnInterval` in `LoopGame` shadow the members; `respawnEnemy` reads the **member** `lastRespawnTime`, which is never initialised. In MSVC Debug the heap fill (0xCDCDCDCD) happens to let the first spawn pass; Release behaviour differs *(unverified)*. The check is also redundant with the caller's check. |
| B6 | Med | GameManager.cpp:422 | `std::rand()` never seeded → identical enemy sequence every run. |
| B7 | Med | see §3.3 | Memory leaks: 11 unused `Skill`, 6 `Keyboard`, all enemies; `~GameManager` never runs; `Close()` cleanup commented out. |
| B8 | Med | MainPlayer.cpp:55, GameManager.cpp:267 | `m_KeyRActive` is set by *any* R press (even an invalid combo) and never reset; `ResetRKey()` is never called. |
| B9 | Low | GameManager.cpp:271 | `m_player.skill.Render()` draws a `Skill` with no texture every frame → `SDL_RenderCopy` error each frame (harmless, noisy). |
| B10 | Low | MainPlayer.cpp:90 | `activeSpell = NO_SPELL;` assigns an enum (value 0) to `std::string` → compiles as `operator=(char)`, i.e. a one-character string containing `'\0'`. `activeSpell` is otherwise unused. |
| B11 | Low | Enemy.h:30-36 | `EnemyObject` re-declares `currentFrame_, frame_clip_, width_frame_, height_frame_, x_val_, y_val_` (hiding the base members). Also `Skill::currentFrame_`, and all of `ThreatObject`'s. Works today because the derived class consistently uses its own copies, but the base `BaseObject::Render/set_clips` would operate on different data. |
| B12 | Low | GameManager.cpp:259-263, 277-285 | Render-then-`set_clips`/`SetPos` order: first frame of a newly-shown skill icon draws nothing, later frames are one frame behind. Cosmetic. |
| B13 | Low | GameManager.cpp:434 | Enemies spawn at x=800 (inside the 928-px window → visible pop-in) and y=400 with 150-px sprites (bottom at 550 > 544 window height); 64-px sprites would float. |
| B14 | Low | GameManager.cpp:374 | Background 928×793 drawn into 928×544 (aspect distortion). May be intentional; confirm. |
| B15 | Low | BaseObject.cpp:38 | Global colour key (175,175,175) applied to every asset. Any pixel of that exact grey becomes transparent. |
| B16 | Low | GameManager.cpp:26-100 | Init failures are silent (no message, exit code 0); `Close()` not called on init failure. |
| B17 | Info | `keysym.sym` | Layout-dependent keycodes (AZERTY etc.); scancodes would be position-based. Decide deliberately. |

## 12. Technical debt
- God-object `GameManager` + 230-line `LoopGame` mixing load/run/teardown; no update/render split.
- Frame-rate-coupled movement and enemy animation (px per frame, 25 FPS assumed). Player animation is time-based, so the codebase mixes both.
- Input hard-wired to `SDL_Event` inside a game-object class (`MainPlayer`).
- Recipe data duplicated per `Skill` instance; `Skill` is both "icon sprite" and "recipe database".
- Enemy types are implicit (path strings). No data structure for `{sprite, frames, skill}`.
- No asset manager / texture cache; scattered string paths; per-spawn disk loads.
- Global mutable singleton created at static-init time; typo `getInstace`.
- Hard-coded magic numbers (positions, 800, 400, 5, 5000).
- `using namespace std;` in headers; reliance on transitive includes.
- Copy-pasted constructor typos (`rect_.y = 0` twice, `frame_clip_[0].w` inside a loop) in `MainPlayer`, `Skill`, `Keyboard` — harmless but noisy.
- Comments in mixed Vietnamese/English; large commented-out blocks.

## 13. Duplicated / obsolete
- Dead files: `ThreatObject.h/.cpp`, `IKeyHandler.h`; `main.cpp` `#if 0` block.
- Unused members: `GameManager::{m_Skilllist, activeEnemies, availableEnemy (shadowed by a local), isEnemyOnScreen, m_background}`, `MainPlayer::{skill_, m_QKeyNum/m_WKeyNum/m_EKeyNum (W/E uninitialised), m_KeyDown, activeSpell}`, `Skill::{m_active, Spell enum}`, `rect_D/rect_F`, `BaseObject::{m_screen, render()}`; `Keyboard::KeyType` usage (D/F lookup loop over an empty list).
- Unused assets: `bg.png` (loaded, not drawn), `bk.png`, 6 enemy sheets, R/D/F key icons (loaded, not drawn).
- Stale build artefacts committed (see §2); `Three Elements/Debug/` holds an old Win32 build's leftovers.
- Vendored but unused: SDL2_ttf, SDL_test headers, x86 libraries (the Win32 configuration cannot build anyway).

---

## 14. Gap analysis against the intended MVP (constraints, not tasks)

| Requirement | Current state | Constraint / gap |
|---|---|---|
| 10 spells available from start | Yes (`spellMap`) | none |
| Q/W/E order irrelevant | Yes (sorted key) | keep |
| D/F slots with Invoker swap | Slots yes, casting no | need cast input path (B3) |
| 10 enemy types ↔ 10 skills | 7 types, no mapping | needs a small data table `{sprite, frames, spellKey}`; 13 sheets exist (6 unused, frame counts unknown) |
| Correct spell removes enemy; wrong spell does nothing | No enemy removal at all | needs enemy lifecycle (B4) + a definition of *which* enemy a cast targets (open decision) |
| Enemy reaching player costs HP | No HP, no "reached" event | hook exists at `Enemy.cpp:19-23`; define the reach x-threshold (player sprite spans x≈10-234) |
| Difficulty rises over time | Constant 5 s spawn, constant speed | spawn interval/speed must become variables driven by elapsed time |
| Endless session, game over at HP 0 | No state machine | one loop function only |
| Score, combo, best combo, stats | Nothing | need event timestamps (reaction time, APM), counters; and **a way to draw text** (SDL_ttf not linked; bitmap font is an option) |
| Web first | Blocking loop, path-relative assets, VS-only build | see §15 |
| Touch buttons Q/W/E/R/D/F | Input tied to `SDL_KEYDOWN` inside `MainPlayer` | need a tiny logical-key seam so keyboard and touch feed the same entry point |
| Pixel art, side view | Linear filter hint, squashed bg | nearest-neighbour + integer scaling will matter for web/mobile |

---

## 15. Risks for Web / Emscripten

1. **Blocking main loop** (`while(!bStop)` + `SDL_Delay` inside `LoopGame`). Browsers require the loop to be a callback (`emscripten_set_main_loop`) or ASYNCIFY. `LoopGame` also does loading and teardown in the same function, so it must be split first.
2. **Build system:** Emscripten needs CMake or a Makefile; the current VS-only, absolute-path project cannot be reused. SDL/SDL_image come from Emscripten ports, not from the vendored 2.0.16 Windows libs.
3. **Asset access:** `IMG_Load("assets/...")` needs the assets preloaded into the virtual FS (`--preload-file`); paths are scattered as literals. 1.4 MB total is fine.
4. **Frame timing:** 25 FPS `SDL_Delay` cap and per-frame movement do not suit a browser-driven ~60 Hz loop; movement/animation need delta-time.
5. **Compile portability (clang/libc++):** missing `<cmath>`, `<cstdlib>` includes; `printf("%s", std::string)` (B1) is a warning/UB; `using namespace std` in headers; `Uint32` wrap arithmetic. Expect a few fix-ups.
6. **GitHub Pages constraints:** static hosting only, no COOP/COEP headers → avoid pthreads/SharedArrayBuffer (single-threaded build), serve under a sub-path (`/repo-name/`), keep `.wasm/.data` relative URLs.
7. **Audio (later):** SDL_mixer via Emscripten port; browsers block audio until a user gesture.
8. **Keyboard:** canvas needs focus; `keysym.sym` on non-QWERTY layouts; make sure R/D/F do not trigger browser shortcuts (they do not without modifiers, but verify).
9. **Scaling:** fixed 928×544 window; needs logical-size/canvas scaling and nearest filtering for pixel art.
10. **Memory:** per-spawn texture loads and leaks are tolerable on desktop for minutes; an endless session in a browser tab is not.

## 16. Risks for Android / mobile

1. **No touch handling.** SDL synthesizes mouse events from touch by default, but nothing reads mouse events; needs on-screen Q/W/E/R/D/F buttons, multi-touch aware, sized for thumbs, laid out like a keyboard.
2. **Input coupled to `SDL_Event` keydown** inside `MainPlayer` (see §14). Needs the logical-key seam first.
3. **Build:** requires the SDL Android project (Gradle + NDK, SDL sources), not the vendored Windows libs; SDL 2.0.16 headers vs. a newer SDL source tree may differ.
4. **Asset root:** on Android `SDL_RWFromFile` reads from the APK `assets/` directory; the `assets/` prefix in paths will need to be centralised.
5. **Screen aspect ratios / safe areas / orientation:** fixed 928×544 window (≈1.71:1); needs logical size + letterboxing or layout adaptation and a landscape lock.
6. **Lifecycle:** no handling of `SDL_APP_*` events (pause/resume) nor renderer/device reset events; textures may need recreating after context loss *(verify with SDL 2.0.16+ behaviour)*.
7. **Performance/battery:** 25 FPS busy loop with `SDL_Delay`, per-frame full-screen 12-layer background at 928×793 sources (24 draw calls/frame). Should be fine but must be measured.
8. **Latency:** touch latency and accidental multi-touch matter far more for a speed-practice game than for a normal one; input must be processed on touch-down, not on release.

---

## 17. What should NOT be rewritten unnecessarily
- **Recipe logic:** `spellMap` + sorted-triple lookup + D/F swap in `saveSpellToSlot`. It is correct and small. Move/deduplicate the table, do not redesign it.
- **Rolling 3-element list** and `getElementComb()`.
- **`BaseObject` texture/sprite wrapper** and `EnemyObject`'s sprite-sheet slicing. Fix leaks/shadowing minimally; no ECS, no scene graph, no engine.
- **Background parallax code** (`load/update/renderBackgroundLayers`) — works; at most fix the aspect question.
- **`ImpTimer`** and the overall SDL2 + SDL2_image stack. Do not migrate to SDL3 or another library.
- **Asset folder layout** and file naming (all paths already case-correct).
- The **singleton `GameManager`** can stay; the goal is to split its long function, not to replace the pattern.

## 18. Recommended architectural changes (proposals, NOT current implementation)
Ordered smallest → largest; each is intentionally minimal.
1. **Make the build portable:** relative paths in the vcxproj (`$(SolutionDir)Dependencies\…`), configure Release|x64, `git rm --cached` the build outputs and `.vcxproj.user`.
2. **Fix the trivial correctness bugs** B1, B5, B6, B7 (enemy/skill/key ownership), B8-B10, B11 (remove shadowing) and delete dead files.
3. **Split `LoopGame`** into `Init… / HandleEvents / Update(dt) / Render / Shutdown` bodies, moving the existing code as-is. Prerequisite for Emscripten.
4. **Logical-key seam:** one function such as "player receives key Q/W/E/R/D/F" fed by a single SDL→logical mapping (ignoring `repeat`) so touch can call the same entry later. Record a timestamp per key.
5. **Data-driven enemy types:** a small table `{spritePath, frames, spellKey}` (10 rows) plus a load-once texture cache; enemy stores its `spellKey`; remove enemies on kill / on reaching the player (ownership via `unique_ptr` or explicit erase+delete).
6. **Delta-time** for movement, spawn timer and enemy animation (keep the player's time-based animation).
7. **Tiny state enum** (`Playing`, `GameOver`) and a small `Stats` struct (hp, score, combo, bestCombo, counters) inside `GameManager`. No state-machine framework.
8. **Single recipe table** (static const) shared instead of one copy per `Skill`.
9. **Central asset-path helper** (one prefix) for web/Android.
10. **Pixel-art rendering settings:** nearest filtering, logical size + integer scaling — do this at Web time, not before.

## 19. Open questions / unverified
- Does the current Debug x64 build run cleanly on this machine (DLL set, working directory)? Not run.
- Correct frame counts for the 6 unused enemy sheets (needs visual check).
- Whether the squashed background is intended (928×793 → 928×544).
- Which `SDL_image` runtime DLLs are actually resolved at run time.
- Behaviour of B5 in Release builds.
