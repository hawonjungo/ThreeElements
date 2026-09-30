# Injoker (repo: Three Elements) — tài liệu bàn giao cho agent

> **Tên sản phẩm là Injoker** (quyết định chủ project 2026-09-30, tên dùng trên Google Play). Tên thư mục, repo, `.sln` và project vẫn là "Three Elements" để không phá build/lịch sử; chỉ những gì người chơi thấy mang tên Injoker (tiêu đề cửa sổ, tên app Android, trang web, màn Ready). Mã app Android: `net.relifes.injoker` (không đổi được sau khi lên Play).

> Ngôn ngữ làm việc với chủ project: tiếng Việt (các tài liệu kỹ thuật bên dưới viết bằng tiếng Anh). Tài liệu này là bản tóm tắt để agent mới hiểu project và làm tiếp. Trạng thái cập nhật ngày 2026-09-28: Phase 3 xong và đã có **bản web chạy trên điện thoại** (https://injoker.relifes.net). Mọi việc làm trên nhánh **`main`**; nhánh `gh-pages` chỉ chứa bản web đã build. Repo chỉ còn đúng hai nhánh này (các nhánh cũ đã xóa 2026-09-29). Debug và Release x64 lẫn bản web đều đã được build và chạy thử.

**Đọc theo thứ tự này trước khi sửa code:**

1. [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md) — luật chơi MVP (Practice Mode), **nguồn sự thật duy nhất**, mỗi luật gắn nhãn CONFIRMED / RECOMMENDED / FUTURE. Hiện **không có mục nào đang chặn**. Quy tắc phá thế bí: đây là game luyện Invoker, **Dota 2 Invoker là mô hình tham chiếu**; nếu spec không nói về một cơ chế Invoker thì làm theo Invoker, không tự phát minh luật mới.
2. [PLAN.md](PLAN.md) — lộ trình theo giai đoạn (Phase 0–9), phạm vi, điều không được làm, tiêu chí hoàn thành.
3. [PROJECT_AUDIT.md](PROJECT_AUDIT.md) — kiến trúc code hiện tại, lỗi đã biết (B1–B17), rủi ro Web/Android, phần không nên viết lại.

Nếu tài liệu này mâu thuẫn với GAMEPLAY_SPEC.md về luật chơi, **GAMEPLAY_SPEC.md thắng**.

## 1. Project là gì

**Three Elements** là game side-scrolling 2D pixel, viết 100% C++ với SDL2 và SDL2_image. Đây là project cá nhân năm hai đại học, mục đích chính là học OOP, tác giả sẽ quay lại làm tiếp "khi rảnh".

**Mục đích chính (đã chốt):** một game **luyện tập kiểu Invoker** (DOTA 2), mô phỏng quy trình Q/W/E/R/D/F càng sát càng tốt. Luyện: tốc độ gõ phím, đổi skill nhanh, nhớ công thức, quyết định dùng skill nào, giữ combo liên tục, phản xạ. **Practice Mode là nền móng của game về sau, không phải code thử nghiệm bỏ đi.**

Cách chơi (MVP):

- Người chơi bấm **Q / W / E** để nạp nguyên tố (Quas = băng, Wex = sét, Exort = lửa), tối đa 3 orb (orb thứ 4 đẩy orb cũ nhất ra). Thứ tự bấm **không quan trọng**, kết quả do **số lượng** Q/W/E quyết định (QQW = QWQ = WQQ = Ghost Walk).
- Bảng recipe: QQQ Cold Snap · QQW Ghost Walk · QQE Ice Wall · QWW Tornado · QWE Deafening Blast · QEE Forge Spirit · WWW EMP · WWE Alacrity · WEE Chaos Meteor · EEE Sun Strike.
- **R** = Invoke: đủ đúng 3 orb thì ra skill và đưa vào ô D/F; **chưa đủ 3 orb thì R không làm gì** (không phạt, không reset orb). Invoke không tiêu orb.
- **D / F** = hai ô skill kiểu Invoker (skill mới vào D, D cũ sang F, F cũ bị bỏ; invoke lại skill đang ở F thì hai ô đổi chỗ) và là phím **cast**. Cast xong skill **vẫn nằm trong ô** tới khi lần invoke sau đổi ô. Không cooldown.
- Mỗi lượt chỉ có **một quái mục tiêu** mang `TargetSkillId` (không hard-code "loại quái = skill"). Quái **không hiện recipe**, nhưng từ 2026-09-23 HUD **có hiện tên skill mục tiêu** ("TARGET: <tên skill>") cho mọi người chơi — người chơi vẫn phải tự nhớ/suy ra recipe Q/W/E của skill đó.
- **Cast đúng:** quái biến mất ngay, điểm +1, combo +1, tính là cast đúng. **Cast sai:** quái không bị thương và vẫn tiến tới, người chơi được thử lại, tính là cast sai, **không** ngắt combo. **Quái chạm người chơi:** HP −1, quái biến mất, **combo về 0**, lượt sau bắt đầu.
- Cast được tính (cho độ chính xác) chỉ khi bấm D/F với ô có skill **và** đang có quái. Bấm R khi chưa đủ 3 orb, bấm D/F khi ô trống, hoặc cast khi không có quái **không phải cast sai** và không ảnh hưởng độ chính xác.
- **HP ban đầu = 3.** Phiên chơi vô hạn, độ khó tăng theo thời gian (chỉ chỉnh tốc độ quái và độ trễ giữa các lượt bằng một hàm xác định đơn giản), Game Over khi HP ≤ 0.
- Thống kê: điểm, combo hiện tại, combo tốt nhất, độ chính xác (= cast đúng ÷ tổng cast), thời gian sống sót. **Best Score / Best Combo / Best Survival Time** lưu cục bộ và **không** bị Restart xóa; Restart xóa orb, ô D/F, HP, điểm, combo, độ chính xác, đồng hồ sống sót, quái đang hoạt động và đồng hồ độ khó.
- Nền tảng: **Web trước (GitHub Pages)**, sau đó Android (nút cảm ứng Q/W/E/R/D/F xếp như bàn phím), PC/Steam chỉ khi game đủ tốt.
- Bản prototype thiết kế (chỉ tham khảo): https://www.figma.com/proto/8sxWcAHPDHRtevGTqmJKJd/Three-Elements

## 2. Quyết định sản phẩm đã chốt (không còn là câu hỏi mở)

| Chủ đề | Quyết định |
|---|---|
| Cơ chế ra chiêu | **Phím Q/W/E/R/D/F** kiểu Invoker (trên mobile: nút cảm ứng cùng các hành động logic đó). **Không** chuyển sang thẻ bài. |
| Thứ tự Q/W/E | Không quan trọng; recipe phải được chuẩn hóa, không phụ thuộc thứ tự nhập |
| Cooldown / mở khóa skill | Không có; cả 10 skill dùng được ngay từ đầu |
| Quái ↔ skill | Quái có `TargetSkillId` (dữ liệu), không gắn cứng theo loại quái |
| Mục tiêu | Chỉ **một quái mục tiêu** mỗi lần; chưa làm nhiều mục tiêu |
| Cast đúng / sai | Đúng: quái biến mất, điểm và combo tăng. Sai: không sát thương, quái vẫn tiến; chạm người chơi thì trừ HP và bị xóa |
| Kết thúc | Vô hạn, độ khó tăng theo thời gian, Game Over khi HP ≤ 0 |
| Thống kê tối thiểu | Điểm, combo hiện tại, combo tốt nhất, độ chính xác, thời gian sống sót |
| Kiến trúc | Ba lớp, phụ thuộc một chiều **Presentation → Practice → Core**. **Core** = InputAction, Quas/Wex/Exort, chuẩn hóa recipe, SkillDefinition, SkillCatalog, InvokerState, Invoke, ô D/F (không biết SDL, quái, HP, điểm, vẽ, vòng lặp). **Practice** = EnemyDefinition, TargetSkillId, PracticeSession, HP, điểm, combo, độ chính xác, thời gian, độ khó, quái đang hoạt động, kết quả thử thách. **Presentation** = input SDL/cảm ứng, vẽ, sprite, âm thanh, HUD, parallax. Xem GAMEPLAY_SPEC.md §20 |
| Giữ code hiện có | Logic recipe và ô D/F hiện tại **đã đúng**: giữ nguyên hành vi, chỉ chuyển vào Core khi cần. Không viết lại chỉ vì phong cách |
| Không được thêm | Game engine, event bus, dependency injection, ECS, kế thừa/template không cần thiết. Giữ kiến trúc nhỏ, dễ hiểu |
| Loại trừ khỏi MVP | Cốt truyện, chiến đấu, AI quái phức tạp, cooldown, mở khóa skill, **gợi ý recipe** (không hiện phím cần bấm), nhiều mục tiêu, boss, kiếm tiền, Steam, UX mobile nâng cao, thành tựu, công thức điểm phức tạp. Gợi ý **tên skill mục tiêu** thì có (GAMEPLAY_SPEC.md E-8) |
| Tùy chọn debug | Chỉ cho phát triển: hiển thị `TargetSkillId` của quái đang hoạt động; mặc định tắt, không có trong bản web phát hành |

Chi tiết từng luật, bảng phân loại input, ví dụ và các trường hợp biên nằm trong [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md). Mục "Open decisions" của spec hiện **không có mục chặn**.

## 2b. Ý tưởng thiết kế Figma (tham khảo cho giai đoạn sau, KHÔNG thuộc MVP)

Figma mô tả một hướng khác (thẻ bài, cốt truyện). Phần này được **hoãn** tới Phase 8 trong PLAN.md; không đầu tư vào nó trong MVP. So sánh với code hiện tại:

| | Thiết kế (Figma) | Code thực tế |
|---|---|---|
| Nguyên tố | 3 thẻ bài trong màn hình `assign cards` | 3 phím Q/W/E |
| Cách ra chiêu | Gán thẻ vào bộ kết hợp | Bấm phím rồi bấm R |
| Cốt truyện | 3 anh hùng hợp thể thành **Threne** để đánh Evil King | Chưa có |
| Nhân vật/vai trò | Hammer (đỏ/nâu, vật lý), Knight/Mage (xanh dương/tím, phép), Healer (xanh lá, hồi phục), Dragon | Chưa có. Chỉ có 1 sprite người chơi (`main.bmp`) |
| Màn hình | Logo (`Desktop - 1`), menu Play/Option/Quit (`Desktop - 4`), `assign cards`, `Game Play 6` đến `11` | Chỉ có màn gameplay, vào game là chạy luôn |

Figma gồm các Pages: `PC`, `demo`, `gameplay`, `poster`.

Điểm chung ở cả hai bên: bối cảnh side-scrolling ngang và ý tưởng "3 nguyên tố kết hợp ra kỹ năng". Cơ chế **đã chọn** là phím (mục 2); tên/hình skill là dữ liệu tách khỏi logic (id ổn định riêng) để có thể đổi giao diện ở bản final.

## 3. Stack và cách build

- Visual Studio 2022 (toolset v143), x64. Mở `Three Elements.sln`.
- Thư viện SDL2, SDL2_image nằm sẵn trong `Dependencies/` (kèm DLL trong `Dependencies/lib/x64`). SDL2_ttf có header và lib nhưng phần dùng đang bị comment. **Không dùng SDL_mixer:** âm thanh được tạo bằng code (`Audio.*`) và phát qua audio của chính SDL2.
- **Build (đã ổn định ở Phase 1):** đường dẫn include/lib trong `Three Elements.vcxproj` đều tương đối so với file project, cả 4 cấu hình (Debug/Release × x64/Win32) build được; sau build, các DLL SDL được tự copy cạnh exe. Bản Win32 mới chỉ build, chưa chạy thử.
- Đường dẫn asset trong code là tương đối (`assets/...`), nên **thư mục làm việc khi chạy phải chứa thư mục `assets`** (thư mục `Three Elements/`). Debugger của Visual Studio đã được đặt đúng thư mục này; khi chạy exe từ dòng lệnh phải tự `cd` vào đó.
- Cửa sổ 928×544. Bản PC giới hạn 60 FPS; bản web chạy theo tần số màn hình. Mọi chuyển động/animation tính theo thời gian thật (`dt`), nên FPS không đổi tốc độ game.
- **Bản web (Emscripten):** `web/build.sh` build vào **một** thư mục duy nhất `D:\Dev\web-build` (emsdk ở `D:\Dev\tools\emsdk`; đổi được bằng biến `EMSDK_DIR`, `WEB_BUILD_DIR`). `web/build.sh --deploy` build rồi commit bản build vào nhánh `gh-pages`, push, và **chờ GitHub Pages phát hành xong rồi so file trên trang thật** (GitHub đôi khi bỏ qua một lần push; script báo lại, chạy lại là được). Chạy từ Git Bash. `web/shell.html` là trang HTML bao quanh canvas (bố cục điện thoại, nút toàn màn hình, thẻ meta iOS), `web/manifest.webmanifest` cho "Thêm vào màn hình chính". Tên miền **`injoker.relifes.net`** (từ 2026-09-30; file `CNAME` trong `gh-pages` do chủ project đặt trong Settings → Pages), **HTTPS hoạt động**. `3elements.relifes.net` không còn là tên miền của trang.
- **Không tạo thêm nhánh hay thư mục build mới** khi build/deploy (yêu cầu của chủ project).
- **Bản Android (native SDL2, Phase 7, bắt đầu 2026-09-30):** thư mục `android/` (Gradle, AGP 8.7.3, Gradle 8.9, JDK 21 của Android Studio). Mã nguồn SDL2 2.32.10 / SDL2_image 2.8.12 **không nằm trong git**: chạy `android/fetch_deps.sh` một lần. Build: `cd android && JAVA_HOME="/c/Program Files/Android/Android Studio/jbr" ./gradlew assembleDebug` (hoặc `installDebug` khi điện thoại cắm USB). NDK/CMake: tự lấy bản mới nhất đã cài. **Google Play (2026-09-30):** bản release AAB ký bằng khóa upload (`D:/Dev/Keys/`, ngoài git), targetSdk 36, căn trang 16 KB, không xin quyền nào; nội dung trang Play, khai báo và ảnh ở `art/store/`; chính sách quyền riêng tư `web/privacy.html` → https://injoker.relifes.net/privacy.html. Tài khoản developer cá nhân → cần thử nghiệm kín 12 người × 14 ngày trước khi phát hành. Chi tiết: [android/README.md](android/README.md).

## 4. Cấu trúc code (`Three Elements/`)

| File | Vai trò |
|---|---|
| `main.cpp` | Tạo singleton `GameManager`, đọc tùy chọn `--debug` (chỉ dev), gọi `InitSDL()` rồi `LoopGame()`; trả 1 nếu khởi tạo lỗi |
| `../web/` | `build.sh` (build/deploy web), `shell.html`, `manifest.webmanifest` |
| `../android/` | Project Android: `app/jni/CMakeLists.txt` (SDL2 + SDL2_image + game → `libmain.so`), `ThreeElementsActivity` (kế thừa `SDLActivity`), manifest (ngang, toàn màn hình), `fetch_deps.sh`, README |
| `../art/` | Ảnh gốc: `skill-icons/` → `make_skill_icons.py` (icon skill); `character/` (nhân vật Injoker của chủ project) → `make_injoker.py` (sprite người chơi, logo màn Ready, icon Android, icon web, icon Google Play `art/store/icon-512.png`) |
| `GameManager.h/.cpp` | **Presentation**: khởi tạo SDL, `LoadAssets()` + `RunFrame()` (một khung hình: input, `PracticeSession::Update(dt)`, vẽ). `LoopGame()` gọi `RunFrame()` trong vòng `while` (PC) hoặc qua `emscripten_set_main_loop_arg` (web). Vẽ background, quái, hiệu ứng skill, HUD (orb màu, ô D/F ở giữa), cụm phím cảm ứng, màn Ready/Game Over, Top-10 điểm, log phát triển |
| `Practice/Practice.h/.cpp` | **Practice** (không SDL, không đồng hồ, không biến toàn cục): `EnemyDefinition` (10 quái, `targetSkill`), `DifficultyAt(elapsed)`, `PracticeSession` (Ready/Playing/GameOver, HP, điểm, combo, độ chính xác, thời gian sống sót, một quái đang hoạt động, sở hữu `InvokerState`) |
| `Practice/Tutorial.h/.cpp` | **Tutorial** (spec §24, không SDL): `TutorialSession` chạy kịch bản các bước (Card / Keys / Run / Done), dùng `InvokerState` và luật đúng/sai của Practice, hình nhân tập, 3 quái đi chậm ở bài cuối; không đụng điểm/kỷ lục |
| `PixelText.h/.cpp` | Chữ pixel 5×7 vẽ bằng `SDL_RenderFillRect` cho HUD (không cần font hay SDL2_ttf) |
| `Draw.h/.cpp` | Hàm vẽ cơ bản dùng chung: hình tròn, vòng, cung, hình thoi, `Hash` (số giả ngẫu nhiên cho hạt) |
| `SkillVfx.h/.cpp` | Hiệu ứng **vẽ bằng code** cho 8 skill (trừ Tornado, Ghost Walk dùng sprite): bảng thời lượng/điểm xuất phát + một hàm vẽ mỗi skill, tính lại từ tuổi hiệu ứng mỗi khung hình |
| `Audio.h/.cpp` | Âm thanh kiểu chiptune **tạo bằng code lúc khởi động** (không file âm thanh, không SDL_mixer), trộn nhiều tiếng trong callback SDL; không mở được thiết bị thì game chạy im lặng |
| `BaseObject.h/.cpp` | Lớp gốc: texture, rect, clip animation, `LoadImg`, `Render` |
| `Core/Invoker.h/.cpp` | **Core** (không SDL): `InputAction`, `Orb`, `Recipe` (chuẩn hóa theo số lượng), `SkillDefinition`/catalog 10 skill (id, recipe, tên, đường dẫn icon), `InvokerState` (orb, ô D/F, `Apply/AddOrb/Invoke/Cast/Reset`). Đây là nơi duy nhất chứa luật Invoker |
| `MainPlayer.h/.cpp` | Sprite người chơi + `TranslateKey` (SDL key → `InputAction`, bỏ qua `key.repeat`); không giữ trạng thái game |
| `Skill.h/.cpp` | Icon của một skill: `LoadIcon` (RGBA, không color key, làm mượt) và `RenderAt(x, y, size)`; dữ liệu skill nằm trong Core |
| `../Tests/` | `InvokerCoreTests` (243 kiểm tra) và `PracticeTests` (946 kiểm tra, gồm Tutorial) + `run_tests.cmd` + README: project riêng trong `.sln`, không được link vào game |
| `Keyboard.h/.cpp` | Icon phím (2 frame); dùng cho nhãn D/F và cụm phím cảm ứng (orb được vẽ thành hình tròn màu, không dùng icon phím nữa) |
| `Enemy.h/.cpp` | `EnemyObject`: chỉ còn là sprite sheet + animation theo thời gian (`Update(dt)`, `ENEMY_ANIM_FPS`); vị trí do Practice quyết định |
| `ImpTimer.h/.cpp` | Bộ đếm giữ FPS ổn định cho bản PC (chỉ `start()` và `get_ticks()`) |
| `Define.h` | Include chung (SDL, SDL_image, stdio, string) |

Kế thừa: `MainPlayer`, `EnemyObject`, `Keyboard`, `Skill` đều kế thừa `BaseObject`.

Tài nguyên trong `assets/`:
- `background/`: 12 lớp parallax
- `enemies/`: 13 sprite, **10 đang dùng** (mushroom_run, goblin_run, eyes_fly, skeleton, fire_wiz, nec_walk, worm_run + dark_wiz, kitsune_run, knight_run thêm ở Phase 3); chưa dùng: bat_fly, mush (trùng nấm), nec_walk_bg (trùng nec_walk)
- `keyboard/`: icon Q W E R D F
- `skill/`: 10 icon skill **128×128 RGBA, nền trong suốt**, do script `art/make_skill_icons.py` tạo từ ảnh gốc 2048×2048 của chủ project trong `art/skill-icons/` (tách nền trắng/caro/tối loang từ mép, thu nhỏ). Thay ảnh gốc thì chạy lại `python art/make_skill_icons.py` (cần Pillow); `art/` không được đóng gói vào bản web. `Skills/` (cạnh `skill/`) chỉ còn 2 thư mục sprite hiệu ứng: `Tornado/` (`tornado_vfx_16f.png`, sheet 512×512, 4×4 frame 128×128, nền trong suốt; `tornado_icon.png` chưa dùng) và `Ghost/` (`Ghost Walk-spritesheet.png`; `Ghost.png` chưa dùng)
- `player/injoker.png`: nhân vật Injoker (một tư thế đứng, vẽ 120 px cao, lướt nhấp nhô; sprite sheet chạy sẽ thay sau). `title/injoker_logo.png`: logo màn Ready. Cả hai do `art/make_injoker.py` tạo; sprite chạy cũ `main.bmp` đã bỏ (2026-09-30).

## 5. Đã làm được

- **Core (Phase 2):** nhập Q/W/E (3 orb, thứ tự không quan trọng), R invoke khi đủ 3 orb, ô D/F đúng kiểu Invoker, catalog 10 skill. Có test (`Tests/InvokerCoreTests`).
- **Practice Mode (Phase 3):** `Practice/Practice.*` (không SDL) — 10 loại quái, mỗi loại mang `targetSkill` (bảng dữ liệu, tự do đổi); đúng **một quái** mỗi lượt; cast D/F đúng thì quái biến mất (+1 điểm, +1 combo), cast sai thì chỉ tính vào độ chính xác, quái chạm người chơi thì HP −1 và combo về 0, HP 0 thì Game Over; độ khó tăng theo thời gian (tốc độ quái 125→380 px/s, độ trễ giữa các lượt 1,5→0,5 s: **giá trị tuning ban đầu của MVP, chưa cân bằng**); chơi lại bằng Enter. Có test (`Tests/PracticeTests`).
- **Trình bày (SDL):** vòng lặp dùng `dt` thật cho mọi thứ (luật chơi, animation quái, cuộn nền); quái vẽ theo vị trí Practice (10 sprite nạp một lần, đặt đúng mặt đất); HUD gồm HP, điểm, combo, best combo, độ chính xác, thời gian, orb, ô D/F; màn hình Ready và Game Over; bộ chữ pixel tự vẽ (`PixelText.*`) vì SDL2_ttf chưa được tích hợp và repo chưa có file font.
- **Điều khiển:** Q/W/E/R/D/F chơi. **Ready:** Enter bắt đầu, Esc thoát app. **Playing:** Esc về Ready (phiên bị dừng và reset, kỷ lục best combo giữ nguyên), Enter bị bỏ qua. **Game Over:** Enter bắt đầu phiên mới, Esc về Ready. Không có pause. Luật Enter/Esc nằm trong `PracticeSession::PressEnter/PressEscape` (có test). Phím giữ (auto-repeat) không còn tạo nhiều lần bấm.
- **Tornado (hiệu ứng thật đầu tiên):** cast Tornado từ D hoặc F (nhận theo `SkillId` trong ô) bắn ra một quả cầu lốc từ người chơi về phía quái, hướng chốt một lần lúc bắn (không tự dẫn), bay thẳng theo `dt` (700 px/s). **Chỉ được chấm khi trúng con quái đã bị nhắm tới**, qua đúng đường chấm điểm cũ (`PracticeSession::JudgeCast`): trúng quái cần Tornado thì quái biến mất, điểm và combo +1 đúng một lần; trúng quái cần skill khác thì là **1 cast sai** (quái vẫn sống, HP không đổi, combo/điểm/độ chính xác theo luật cast sai thường); **trượt thì không đổi gì** (không combo, điểm, độ chính xác, HP). Không có quái thì không bắn và không tính cast. **Không giới hạn số projectile cùng lúc** (mỗi cast hợp lệ tạo đúng một quả). Animation lặp 0→15→0 khi projectile còn sống. Logic ở `Practice/Practice.*` (hàm thuần, có test), vẽ ở `GameManager::RenderTornadoes` (16 frame, 10 fps, nearest-neighbour, dưới HUD). Giá trị là tuning ban đầu.
- **Gợi ý tên skill (2026-09-23, quyết định chủ project):** HUD luôn hiện "TARGET: <tên skill>" ở góc trên phải cho mọi người chơi, mọi bản build (native lẫn web), không cần `--debug` (`GameManager::RenderTargetHint`). Vẫn **không** hiện recipe/phím cần bấm. Chạy `"Three Elements.exe" --debug` (chỉ để phát triển) vẫn thêm dòng log `[debug] ... target ... recipe ...` ra console, không ảnh hưởng HUD.
- **Hiệu ứng skill (2026-09-29):** Tornado và Ghost Walk dùng sprite riêng; 8 skill còn lại vẽ bằng code theo kiểu Dota (`SkillVfx.*`): Cold Snap mảnh băng, Ice Wall hàng cột băng, EMP cầu điện tím rồi nổ, Alacrity hào quang cam-tím, Sun Strike cột sáng vàng, Forge Spirit cầu lửa bay, Chaos Meteor thiên thạch rơi rồi nổ, Deafening Blast sóng vòng cung. Chỉ để trình bày; Practice chấm cast như cũ. Projectile bay 700 px/s. Các sheet hiệu ứng tạm cũ của 8 skill đã bị xóa (2026-09-29).
- **Âm thanh (2026-09-29):** mỗi orb một tiếng (Q chuông băng, W tiếng điện, E tiếng lửa), R invoke, cast đúng/sai, cast chưa chấm (whoosh), mất máu, Game Over, bắt đầu phiên. Nút "SOUND ON/OFF" dưới ACC (chạm/click) hoặc phím **M**; lựa chọn lưu ở `settings.txt` (PC) / `localStorage` (web). Trên web, `shell.html` đánh thức AudioContext ở lần chạm/phím đầu tiên (trình duyệt bắt buộc).
- **Đếm lượt truy cập (web, 2026-09-29):** `shell.html` gọi dịch vụ miễn phí Abacus (`abacus.jasoncameron.dev`, khóa `threeelements-relifes/visitors`); mỗi trình duyệt chỉ cộng 1 lần (cờ `localStorage`), chỉ trang thật (relifes.net / github.io) mới cộng, bản chạy thử chỉ đọc. Hiện dưới canvas trên PC, ở góc trên phải (dải đen) khi điện thoại nằm ngang. Dịch vụ lỗi thì bộ đếm tự ẩn. Ai cũng gọi được API này nên con số chỉ mang tính tham khảo.
- **HUD (2026-09-23, quyết định chủ project):** orb Q/W/E vẽ thành hình tròn màu (Quas xanh băng, Wex tím điện, Exort cam lửa, `kOrbColors`), ô trống là vòng mờ; orb và ô D/F nằm giữa màn hình và chỉ hiện khi đang chơi.
- **Điện thoại (web):** cụm phím cảm ứng Q/W/E/R (một hàng) + D/F (hàng dưới, lệch như bàn phím) ở **giữa cạnh trái** (y 184–368, dời lên 2026-09-30 để dễ bấm và không che nhân vật), 88 px; khi có phím cảm ứng, hàng orb và ô D/F dịch sang phải 160 px (`TOUCH_HUD_SHIFT_X`), **chỉ hiện trên thiết bị cảm ứng** (media query lúc khởi động hoặc khi có chạm), ẩn trên PC. Chạm vào chữ Enter/Esc trên màn hình để bắt đầu/về menu. Nằm ngang thì canvas giãn tối đa, giữ tỉ lệ; nút "Full" bật toàn màn hình (Android) hoặc hướng dẫn "Thêm vào MH chính" (iPhone, vì iOS không cho web toàn màn hình). Trên web, Esc ở Ready không làm gì (không có app để thoát).
- **Kỷ lục (2026-09-28):** Best Score / Best Combo / Best Survival Time lưu ở `bests.txt` (PC) hoặc `localStorage` (web), cập nhật khi phiên kết thúc (Game Over hoặc Esc khi đang chơi), hiện ở màn Ready và Game Over ("NEW BEST!"). Practice chỉ so sánh (`MergeBests`, có test); `GameManager` lưu/đọc. **Top-10 điểm** vẫn giữ, lưu ở `highscores.txt` / `localStorage`.
- **Phản hồi khi chơi (2026-09-28, mức nhẹ):** cast đúng → vòng vàng + "+1"; cast sai → quái nháy đỏ + "MISS"; quái chạm người chơi → viền đỏ, rung nhẹ (chỉ phần cảnh, HUD và phím cảm ứng đứng yên), ô HP vừa mất nhấp nháy. Hằng số `FEEDBACK_*` trong `GameManager.h`.
- **Background:** 12 lớp parallax 928×793, cắt một dải 544 px **không co giãn** (`BACKGROUND_CROP_Y`), mặt đất vẫn ở y = 500 (trước đây ảnh bị ép dẹt).
- **Hình ảnh sắc nét (2026-09-30):** `SDL_HINT_RENDER_SCALE_QUALITY "0"` (nearest) cho pixel art; chỉ icon skill (tranh vẽ) được làm mượt (`Skill::LoadIcon`).
- **Bảng tra công thức (2026-09-30, quyết định chủ project):** phím H hoặc nút "RECIPES (H)" ở màn Ready/Game Over mở bảng 10 skill (icon, tên, 3 orb màu); **không mở được khi đang chơi**; phím/chạm bất kỳ để đóng (`GameManager::RenderRecipes`).
- **Tutorial (2026-09-30, spec §24):** nút **TUTORIAL (T)** ở màn Ready (nhấp nháy tới khi hoàn thành lần đầu; trạng thái lưu cùng cài đặt âm thanh). 4 bài: nguyên tố → Sun Strike dắt tay từng phím (E E E R D, phím cần bấm sáng lên cả trên HUD lẫn nút cảm ứng) → thẻ quy tắc → tự chơi 3 quái chậm (chỉ thấy tên + icon, H mở bảng công thức và tạm dừng). Khung vàng nhấp nháy làm nổi bật quái, TARGET, orb, ô D/F. Enter/Space/NEXT sang thẻ tiếp; Esc thoát; màn cuối Enter vào Practice. Không ảnh hưởng điểm/kỷ lục.
- **Menu chính (2026-09-30, quyết định chủ project):** màn Ready là menu từng dòng PLAY / TUTORIAL / RECIPES / LEADERBOARD / SOUND / QUIT (QUIT không có trên web); phím mũi tên + Enter, phím tắt T/H/L/M/Esc, hoặc chạm. Bên phải là **Top 3 theo thời gian sống**, chạm hoặc L mở **Top 10** (màn LEADERBOARD, kèm "MY BEST"). Game Over có nút RECIPES và LEADERBOARD và dòng "RANK #n ON THIS DEVICE". Hiện bảng xếp hạng là **của máy này** (`toptimes.txt` / `localStorage` khóa `threeElements_topTimes`), thứ tự do `practice::InsertTopRun` (có test). Top-10 theo điểm cũ đã bỏ.
- **Nhân vật Injoker (2026-09-30, quyết định chủ project):** pháp sư áo tím lướt trên vòng phép xoay (nhấp nhô, nền vẫn cuộn); **orb đang nạp bay quanh nhân vật** kiểu Invoker (màu Q/W/E, quả phía sau nhỏ và mờ hơn, lóe sáng khi R invoke) — `GameManager::RenderPlayer`, hằng số `PLAYER_*` trong `GameManager.h`. Skill bay ra từ tay cầm gậy (`practice::PLAYER_CAST_X/Y` = 128, 450). Màn Ready: logo + chữ INJOKER (ẩn HUD số liệu ở Ready).
- **Gợi ý mục tiêu có icon (2026-09-30):** "TARGET: <tên>" kèm icon skill 80 px ngay bên dưới tên (`SKILL_HINT_SIZE`).

## 6. Chưa có (phần việc còn lại)

- **UI/UX:** font thật (SDL2_ttf) và HUD đẹp, animation người chơi, cài đặt (âm lượng). **Không làm nhạc nền** (chủ project, 2026-09-29).
- **Độ khó:** chủ project thấy giá trị hiện tại ổn (2026-09-29); **hoãn cân bằng** tới khi làm background thay đổi theo độ khó, rồi chỉnh cả hai cùng lúc.
- **Nền tảng:** bản web đã chạy; app Android native đang làm (Phase 7); PC/Steam.
- **Sau MVP:** chiến đấu, cốt truyện Threne, nhiều mục tiêu, chế độ người mới có gợi ý.

## 7. Nợ kỹ thuật còn lại

1. 3 sprite quái chưa dùng (`bat_fly`, `mush`, `nec_walk_bg`).
2. Màu trong suốt cố định (175,175,175) vẫn áp cho ảnh nạp qua `BaseObject::LoadImg` (icon skill thì không).
3. Đường dẫn asset là chuỗi rải rác (chưa có chỗ chung cho Web/Android).
(Đã xử lý 2026-09-30: `Close()` giải phóng background, `BaseObject` cấm copy, `EnemyObject` dùng biến của lớp cha, nền không còn bị ép, pixel art không còn mờ.)

## 8. Hướng đi

Hướng thiết kế đã chốt (mục 2). Lộ trình chi tiết nằm trong [PLAN.md](PLAN.md); tóm tắt thứ tự:

0. **Phase 0 — Audit & Gameplay Specification:** xong.
1. **Phase 1 — Stabilization:** xong (build di động, gỡ file build khỏi git, sửa UB/biến spawn, xóa code chết).
2. **Phase 2 — Invoker Core Extraction:** xong (`Core/Invoker.*`, test trong `Tests/`).
3. **Phase 3 — Practice Gameplay:** đã cài đặt (chờ chủ project duyệt).
4. **Phase 4–5 — vòng lặp theo khung hình + bản web:** phần lớn đã xong (xem PLAN.md); còn thiếu CMake và đường dẫn asset tập trung.
5. Tiếp theo: 6 UX/Audio (gồm lưu Best stats), 7 Android, 8 Chiến đấu/Cốt truyện, 9 PC/Steam (chỉ làm khi MVP đã được chơi thử).

**Trước khi viết code luật chơi:** đọc GAMEPLAY_SPEC.md. Không còn câu hỏi nào chặn việc triển khai; các giá trị chỉnh được (tốc độ và độ trễ ban đầu, vị trí đường chạm, bảng ánh xạ sprite ↔ skill, phím restart) là hằng số/dữ liệu do người triển khai chọn, ghi ở một chỗ để chỉnh khi chơi thử.

## 9. Quy ước khi làm việc trên repo này

- Style hiện có: tab để thụt lề, tên hàm lẫn lộn giữa `PascalCase` (`LoadImg`, `SetPos`) và `camelCase` (`handleKeyPress`), biến thành viên có hậu tố `_` hoặc tiền tố `m_`. Code mới nên theo style của file đang sửa.
- Comment trong code trộn tiếng Anh và tiếng Việt; commit message bằng tiếng Anh.
- **Test:** logic Core và Practice có bộ test riêng trong `Tests/` (không dùng framework, không dính SDL). Chạy `Tests\run_tests.cmd` (build + chạy cả hai chương trình, mã thoát 0 = đạt); xem `Tests/README.md`. Phải chạy trước khi sửa Core/Practice. Không có CI. Phần SDL (`GameManager`, `PixelText`, `MainPlayer::TranslateKey`) chưa có test tự động; kiểm bằng cách chạy game (`--debug` in mục tiêu ra console để lái phiên chơi).
- **Chạy game:** thư mục làm việc phải là `Three Elements/` (chứa `assets/`). Enter = bắt đầu/chơi lại, Esc = về Ready (thoát app khi đang ở Ready, chỉ bản PC), Q/W/E/R/D/F = chơi, M = bật/tắt âm thanh, H = bảng công thức, L = bảng xếp hạng (ở menu/Game Over), T = Tutorial (ở menu), mũi tên lên/xuống + Enter để chọn trong menu.
- **Kiểm tra bản web:** sau khi `web/build.sh`, có thể phục vụ `D:\Dev\web-build` bằng một HTTP server tĩnh và mở bằng Chrome (giả lập điện thoại trong DevTools) để thử.
- Lịch sử commit: bắt đầu từ game nền (background + nhân vật), rồi refactor OOP, sau đó thêm Keyboard/Skill, gộp combo skill, và gần đây nhất là respawn quái ngẫu nhiên và chỉnh thời gian respawn.
