# Three Elements — tài liệu bàn giao cho agent

> Ngôn ngữ làm việc với chủ project: tiếng Việt (các tài liệu kỹ thuật bên dưới viết bằng tiếng Anh). Tài liệu này là bản tóm tắt để agent mới hiểu project và làm tiếp. Trạng thái cập nhật ngày 2026-09-21, sau Phase 3 (nhánh `phase-2-invoker-core`; các phase 2–3 ở commit `a76802e`, vòng review Phase 3 ở commit `fix: refine practice session lifecycle`). Debug và Release x64 đều đã được build và chạy thử.

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
- Thư viện SDL2, SDL2_image nằm sẵn trong `Dependencies/` (kèm DLL trong `Dependencies/lib/x64`). SDL2_ttf có header và lib nhưng phần dùng đang bị comment. Chưa có SDL_mixer (âm thanh mới ở dạng comment chuẩn bị).
- **Build (đã ổn định ở Phase 1):** đường dẫn include/lib trong `Three Elements.vcxproj` đều tương đối so với file project, cả 4 cấu hình (Debug/Release × x64/Win32) build được; sau build, các DLL SDL được tự copy cạnh exe. Bản Win32 mới chỉ build, chưa chạy thử.
- Đường dẫn asset trong code là tương đối (`assets/...`), nên **thư mục làm việc khi chạy phải chứa thư mục `assets`** (thư mục `Three Elements/`). Debugger của Visual Studio đã được đặt đúng thư mục này; khi chạy exe từ dòng lệnh phải tự `cd` vào đó.
- Cửa sổ 928×544, 25 FPS.

## 4. Cấu trúc code (`Three Elements/`)

| File | Vai trò |
|---|---|
| `main.cpp` | Tạo singleton `GameManager`, đọc tùy chọn `--debug` (chỉ dev), gọi `InitSDL()` rồi `LoopGame()`; trả 1 nếu khởi tạo lỗi |
| `GameManager.h/.cpp` | **Presentation**: khởi tạo SDL, vòng lặp (đo `dt`, xử lý phím Enter/Esc, gọi `PracticeSession::Input/Update`), vẽ background, quái, HUD, màn Ready/Game Over, log phát triển |
| `Practice/Practice.h/.cpp` | **Practice** (không SDL, không đồng hồ, không biến toàn cục): `EnemyDefinition` (10 quái, `targetSkill`), `DifficultyAt(elapsed)`, `PracticeSession` (Ready/Playing/GameOver, HP, điểm, combo, độ chính xác, thời gian sống sót, một quái đang hoạt động, sở hữu `InvokerState`) |
| `PixelText.h/.cpp` | Chữ pixel 5×7 vẽ bằng `SDL_RenderFillRect` cho HUD (không cần font hay SDL2_ttf) |
| `BaseObject.h/.cpp` | Lớp gốc: texture, rect, clip animation, `LoadImg`, `Render` |
| `Core/Invoker.h/.cpp` | **Core** (không SDL): `InputAction`, `Orb`, `Recipe` (chuẩn hóa theo số lượng), `SkillDefinition`/catalog 10 skill (id, recipe, tên, đường dẫn icon), `InvokerState` (orb, ô D/F, `Apply/AddOrb/Invoke/Cast/Reset`). Đây là nơi duy nhất chứa luật Invoker |
| `MainPlayer.h/.cpp` | Sprite người chơi + `TranslateKey` (SDL key → `InputAction`, bỏ qua `key.repeat`); không giữ trạng thái game |
| `Skill.h/.cpp` | Chỉ là sprite icon của một skill (dữ liệu skill nằm trong Core) |
| `../Tests/` | `InvokerCoreTests` (243 kiểm tra) và `PracticeTests` (864 kiểm tra) + `run_tests.cmd` + README: project riêng trong `.sln`, không được link vào game |
| `Keyboard.h/.cpp` | Icon phím (2 frame); dùng cho orb Q/W/E và nhãn D/F |
| `Enemy.h/.cpp` | `EnemyObject`: chỉ còn là sprite sheet + animation; vị trí do Practice quyết định |
| `ImpTimer.h/.cpp` | Bộ đếm giữ FPS ổn định (chỉ `start()` và `get_ticks()`) |
| `Define.h` | Include chung (SDL, SDL_image, stdio, string) |

Kế thừa: `MainPlayer`, `EnemyObject`, `Keyboard`, `Skill` đều kế thừa `BaseObject`.

Tài nguyên trong `assets/`:
- `background/`: 12 lớp parallax
- `enemies/`: 13 sprite, **10 đang dùng** (mushroom_run, goblin_run, eyes_fly, skeleton, fire_wiz, nec_walk, worm_run + dark_wiz, kitsune_run, knight_run thêm ở Phase 3); chưa dùng: bat_fly, mush (trùng nấm), nec_walk_bg (trùng nec_walk)
- `keyboard/`: icon Q W E R D F
- `skill/`: 10 icon skill (`Tornado.png` là icon) + thư mục `Skills/Tornado/` (cạnh `skill/`, đã được code dùng) chứa `tornado_vfx_16f.png`, sheet hiệu ứng Tornado 512×512 (4×4 frame 128×128, nền trong suốt) và `tornado_icon.png` (chưa dùng)
- `main.bmp`: sprite người chơi

## 5. Đã làm được (sau Phase 3)

- **Core (Phase 2):** nhập Q/W/E (3 orb, thứ tự không quan trọng), R invoke khi đủ 3 orb, ô D/F đúng kiểu Invoker, catalog 10 skill. Có test (`Tests/InvokerCoreTests`).
- **Practice Mode (Phase 3):** `Practice/Practice.*` (không SDL) — 10 loại quái, mỗi loại mang `targetSkill` (bảng dữ liệu, tự do đổi); đúng **một quái** mỗi lượt; cast D/F đúng thì quái biến mất (+1 điểm, +1 combo), cast sai thì chỉ tính vào độ chính xác, quái chạm người chơi thì HP −1 và combo về 0, HP 0 thì Game Over; độ khó tăng theo thời gian (tốc độ quái 125→380 px/s, độ trễ giữa các lượt 1,5→0,5 s: **giá trị tuning ban đầu của MVP, chưa cân bằng**); chơi lại bằng Enter. Có test (`Tests/PracticeTests`).
- **Trình bày (SDL):** vòng lặp dùng `dt` thật (vẫn giới hạn 25 FPS); quái vẽ theo vị trí Practice (10 sprite nạp một lần, đặt đúng mặt đất); HUD gồm HP, điểm, combo, best combo, độ chính xác, thời gian, orb, ô D/F; màn hình Ready và Game Over; bộ chữ pixel tự vẽ (`PixelText.*`) vì SDL2_ttf chưa được tích hợp và repo chưa có file font.
- **Điều khiển:** Q/W/E/R/D/F chơi. **Ready:** Enter bắt đầu, Esc thoát app. **Playing:** Esc về Ready (phiên bị dừng và reset, kỷ lục best combo giữ nguyên), Enter bị bỏ qua. **Game Over:** Enter bắt đầu phiên mới, Esc về Ready. Không có pause. Luật Enter/Esc nằm trong `PracticeSession::PressEnter/PressEscape` (có test). Phím giữ (auto-repeat) không còn tạo nhiều lần bấm.
- **Tornado (hiệu ứng thật đầu tiên):** cast Tornado từ D hoặc F (nhận theo `SkillId` trong ô) bắn ra một quả cầu lốc từ người chơi về phía quái, hướng chốt một lần lúc bắn (không tự dẫn), bay thẳng theo `dt` (700 px/s). **Chỉ được chấm khi trúng con quái đã bị nhắm tới**, qua đúng đường chấm điểm cũ (`PracticeSession::JudgeCast`): trúng quái cần Tornado thì quái biến mất, điểm và combo +1 đúng một lần; trúng quái cần skill khác thì là **1 cast sai** (quái vẫn sống, HP không đổi, combo/điểm/độ chính xác theo luật cast sai thường); **trượt thì không đổi gì** (không combo, điểm, độ chính xác, HP). Không có quái thì không bắn và không tính cast. **Không giới hạn số projectile cùng lúc** (mỗi cast hợp lệ tạo đúng một quả). Animation lặp 0→15→0 khi projectile còn sống. Logic ở `Practice/Practice.*` (hàm thuần, có test), vẽ ở `GameManager::RenderTornadoes` (16 frame, 10 fps, nearest-neighbour, dưới HUD). Giá trị là tuning ban đầu.
- **Gợi ý tên skill (2026-09-23, quyết định chủ project):** HUD luôn hiện "TARGET: <tên skill>" ở góc trên phải cho mọi người chơi, mọi bản build (native lẫn web), không cần `--debug` (`GameManager::RenderTargetHint`). Vẫn **không** hiện recipe/phím cần bấm. Chạy `"Three Elements.exe" --debug` (chỉ để phát triển) vẫn thêm dòng log `[debug] ... target ... recipe ...` ra console, không ảnh hưởng HUD.
- **Background:** 12 lớp parallax như cũ.

## 6. Chưa có (phần việc còn lại)

- **Lưu trữ:** Best Combo hiện là kỷ lục **trong một lần chạy app**: phiên mới reset combo hiện tại nhưng giữ Best Combo, chỉ tăng khi lập kỷ lục mới. Chưa ghi ra đĩa (local storage/file) và chưa có Best Score / Best Survival Time (spec §13, Phase 6).
- **UI/UX:** font thật (SDL2_ttf) và HUD đẹp, hiệu ứng khi diệt quái/mất máu, animation người chơi, âm thanh, cài đặt.
- **Nền tảng:** Web (Emscripten, CMake), Android (nút cảm ứng), PC/Steam.
- **Sau MVP:** chiến đấu, cốt truyện Threne, nhiều mục tiêu, chế độ người mới có gợi ý.

## 7. Nợ kỹ thuật còn lại

1. `Close()` vẫn gần như chỉ hủy renderer/window; các texture tải trong `LoopGame` (background, icon, sprite) không được giải phóng tường minh (thoát process là hết). `BaseObject` chưa có copy-control (double free nếu bị copy).
2. Vòng lặp vẫn nằm trong một hàm `LoopGame` và giới hạn 25 FPS bằng `SDL_Delay` (chưa tách "một khung hình" như Web cần). Animation quái và cuộn background vẫn tính theo số khung hình, chỉ chuyển động/thời gian của Practice dùng `dt`.
3. `EnemyObject` vẫn có vài biến che biến của lớp cha (`currentFrame_`, `frame_clip_`, `width_frame_`, `height_frame_`) và 3 sprite quái chưa dùng (`bat_fly`, `mush`, `nec_walk_bg`).
4. Màu trong suốt cố định (175,175,175) áp cho mọi ảnh; nền bị squash (928×793 → 928×544); `SDL_HINT_RENDER_SCALE_QUALITY "1"` làm mờ pixel art khi scale.
5. `EnemyObject::Render` vẫn tăng frame theo mỗi lần gọi (gắn với FPS).
6. Đường dẫn asset là chuỗi rải rác (chưa có chỗ chung cho Web/Android).

## 8. Hướng đi

Hướng thiết kế đã chốt (mục 2). Lộ trình chi tiết nằm trong [PLAN.md](PLAN.md); tóm tắt thứ tự:

0. **Phase 0 — Audit & Gameplay Specification:** xong.
1. **Phase 1 — Stabilization:** xong (build di động, gỡ file build khỏi git, sửa UB/biến spawn, xóa code chết).
2. **Phase 2 — Invoker Core Extraction:** xong (`Core/Invoker.*`, test trong `Tests/`).
3. **Phase 3 — Practice Gameplay:** đã cài đặt (chờ chủ project duyệt).
4. Các phase sau chỉ là placeholder trong PLAN.md: 4 Game Loop/Input/State, 5 Web MVP, 6 UX/Audio (gồm lưu Best stats), 7 Android, 8 Chiến đấu/Cốt truyện, 9 PC/Steam (chỉ làm khi MVP đã được chơi thử).

**Trước khi viết code luật chơi:** đọc GAMEPLAY_SPEC.md. Không còn câu hỏi nào chặn việc triển khai; các giá trị chỉnh được (tốc độ và độ trễ ban đầu, vị trí đường chạm, bảng ánh xạ sprite ↔ skill, phím restart) là hằng số/dữ liệu do người triển khai chọn, ghi ở một chỗ để chỉnh khi chơi thử.

## 9. Quy ước khi làm việc trên repo này

- Style hiện có: tab để thụt lề, tên hàm lẫn lộn giữa `PascalCase` (`LoadImg`, `SetPos`) và `camelCase` (`handleKeyPress`), biến thành viên có hậu tố `_` hoặc tiền tố `m_`. Code mới nên theo style của file đang sửa.
- Comment trong code trộn tiếng Anh và tiếng Việt; commit message bằng tiếng Anh.
- **Test:** logic Core và Practice có bộ test riêng trong `Tests/` (không dùng framework, không dính SDL). Chạy `Tests\run_tests.cmd` (build + chạy cả hai chương trình, mã thoát 0 = đạt); xem `Tests/README.md`. Phải chạy trước khi sửa Core/Practice. Không có CI. Phần SDL (`GameManager`, `PixelText`, `MainPlayer::TranslateKey`) chưa có test tự động; kiểm bằng cách chạy game (`--debug` in mục tiêu ra console để lái phiên chơi).
- **Chạy game:** thư mục làm việc phải là `Three Elements/` (chứa `assets/`). Enter = bắt đầu/chơi lại, Esc = về Ready (thoát app khi đang ở Ready), Q/W/E/R/D/F = chơi.
- Lịch sử commit: bắt đầu từ game nền (background + nhân vật), rồi refactor OOP, sau đó thêm Keyboard/Skill, gộp combo skill, và gần đây nhất là respawn quái ngẫu nhiên và chỉnh thời gian respawn.
