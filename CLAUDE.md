# Three Elements — tài liệu bàn giao cho agent

> Ngôn ngữ làm việc với chủ project: tiếng Việt (các tài liệu kỹ thuật bên dưới viết bằng tiếng Anh). Tài liệu này là bản tóm tắt để agent mới hiểu project và làm tiếp. Trạng thái được ghi nhận ngày 2026-09-21 (nhánh `main`, commit `1c81f62`). Phần "chạy được" được suy ra từ đọc code, chưa build hay chạy game.

**Đọc theo thứ tự này trước khi sửa code:**

1. [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md) — luật chơi MVP (Practice Mode), **nguồn sự thật duy nhất**, mỗi luật gắn nhãn CONFIRMED / RECOMMENDED / FUTURE. Hiện **không có mục nào đang chặn**. Quy tắc phá thế bí: đây là game luyện Invoker, **Dota 2 Invoker là mô hình tham chiếu**; nếu spec không nói về một cơ chế Invoker thì làm theo Invoker, không tự phát minh luật mới.
2. [PLAN.md](PLAN.md) — lộ trình theo giai đoạn (Phase 0–8), phạm vi, điều không được làm, tiêu chí hoàn thành.
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
- Mỗi lượt chỉ có **một quái mục tiêu** mang `TargetSkillId` (không hard-code "loại quái = skill"). Quái **không hiện recipe** và **không có gợi ý** trong bản thường: người chơi phải nhớ quái nào cần skill nào.
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
| Loại trừ khỏi MVP | Cốt truyện, chiến đấu, AI quái phức tạp, cooldown, mở khóa skill, **gợi ý (hints)**, nhiều mục tiêu, boss, kiếm tiền, Steam, UX mobile nâng cao, thành tựu, công thức điểm phức tạp |
| Tùy chọn debug | Chỉ cho phát triển: hiển thị `TargetSkillId` của quái đang hoạt động; mặc định tắt, không có trong bản web phát hành |

Chi tiết từng luật, bảng phân loại input, ví dụ và các trường hợp biên nằm trong [GAMEPLAY_SPEC.md](GAMEPLAY_SPEC.md). Mục "Open decisions" của spec hiện **không có mục chặn**.

## 2b. Ý tưởng thiết kế Figma (tham khảo cho giai đoạn sau, KHÔNG thuộc MVP)

Figma mô tả một hướng khác (thẻ bài, cốt truyện). Phần này được **hoãn** tới Phase 7 trong PLAN.md; không đầu tư vào nó trong MVP. So sánh với code hiện tại:

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
- **Cảnh báo về khả năng build:** `Three Elements.vcxproj` chứa đường dẫn tuyệt đối cứng. Include trỏ tới `D:\Dev\Code\3Elements\ThreeElements\Dependencies\include`, còn thư mục lib trỏ tới `C:\Users\hawon\source\repos\ThreeElements\Dependencies\lib\x64`. Đường dẫn lib có vẻ là của một máy/vị trí cũ. Nếu link lỗi, đổi sang đường dẫn tương đối (`$(SolutionDir)Dependencies\...`). Chưa kiểm chứng.
- Đường dẫn asset trong code là tương đối (`assets/...`), nên **thư mục làm việc khi chạy phải chứa thư mục `assets`** (thư mục gốc project là `Three Elements/`). `.vcxproj.user` chưa đặt working directory.
- Cửa sổ 928×544, 25 FPS.

## 4. Cấu trúc code (`Three Elements/`, khoảng 1.500 dòng)

| File | Vai trò |
|---|---|
| `main.cpp` | Tạo singleton `GameManager`, gọi `InitSDL()` rồi `LoopGame()`. Có khối `#if 0` là code cũ |
| `GameManager.h/.cpp` | Singleton: khởi tạo SDL, vòng lặp game, vẽ/cuộn background 12 lớp, spawn quái, hiển thị phím và skill |
| `BaseObject.h/.cpp` | Lớp gốc: texture, rect, clip animation, `LoadImg`, `Render` |
| `MainPlayer.h/.cpp` | Xử lý phím Q/W/E/R, giữ danh sách nguyên tố, ô skill D và F |
| `Skill.h/.cpp` | Enum `Spell` và `Element`, bảng tra tổ hợp → tên skill (`spellMap`) |
| `Keyboard.h/.cpp` | Icon phím (2 frame, có enum `KeyType`) |
| `Enemy.h/.cpp` | `EnemyObject`: sprite animation, di chuyển sang trái, lưu đường dẫn ảnh để phân loại |
| `ImpTimer.h/.cpp` | Bộ đếm giữ FPS ổn định |
| `Define.h` | Include chung (SDL, SDL_image, stdio, string) |
| `ThreatObject.h/.cpp`, `IKeyHandler.h` | **Chưa dùng / đã comment** — có thể xóa |

Kế thừa: `MainPlayer`, `EnemyObject`, `Keyboard`, `Skill` đều kế thừa `BaseObject`.

Tài nguyên trong `assets/`:
- `background/`: 12 lớp parallax
- `enemies/`: 13 sprite (7 đang dùng: mushroom_run, goblin_run, eyes_fly, skeleton, fire_wiz, nec_walk, worm_run; 6 chưa dùng: bat_fly, dark_wiz, kitsune_run, knight_run, mush, nec_walk_bg)
- `keyboard/`: icon Q W E R D F
- `skill/`: 10 icon skill
- `main.bmp`: sprite người chơi

## 5. Đã làm được

- **Nhập nguyên tố:** Q/W/E được đẩy vào danh sách và chỉ giữ 3 phần tử mới nhất (giống 3 orb của Invoker). Icon 3 phím hiển thị ở góc trên trái theo thứ tự bấm.
- **Invoke bằng R:** tổ hợp được sắp xếp nên thứ tự bấm không ảnh hưởng kết quả. Tra ra đúng 10 skill Invoker:

  | Tổ hợp | Skill | Tổ hợp | Skill |
  |---|---|---|---|
  | QQQ | Cold Snap | EWW | Alacrity |
  | QQW | Ghost Walk | EEE | Sun Strike |
  | EQQ | Ice Wall | EEQ | Forge Spirit |
  | WWW | EMP | EEW | Chaos Meteor |
  | QWW | Tornado | EQW | Deafening Blast |

- **Ô D/F:** skill mới vào ô D, skill cũ đẩy sang F. Cả hai được vẽ trên màn hình.
- **Background:** 12 lớp parallax cuộn liên tục với tốc độ khác nhau.
- **Quái:** 7 loại có animation, spawn ngẫu nhiên mỗi 5 giây từ mép phải (x=800, y=400), chạy sang trái với tốc độ 5 px/frame. Mỗi loại chỉ có một con trên màn hình cùng lúc.

## 6. Chưa có (phần việc còn lại)

- **Chiến đấu:** skill chỉ hiện icon, không có hiệu ứng, sát thương hay đạn. Chưa có luật "skill nào diệt quái nào" (10 skill so với 7 quái, chưa ghép cặp). Va chạm quái–người chơi đang bị comment.
- **Luật chơi:** không có máu, điểm, game over, độ khó tăng dần, quái không bị xóa.
- **Người chơi:** đứng yên, không có animation hành động hay ra chiêu.
- **Phím D/F:** icon đã load nhưng chưa vẽ, chưa có cơ chế cast skill từ ô D/F.
- **UI:** không có menu, chọn/gán thẻ, option, màn hình kết thúc.
- **Khác:** âm thanh, font/text, lưu trạng thái, cốt truyện Threne.

Mức hoàn thành: README ghi "70% code đã xong", nhưng đúng chủ yếu cho phần nhập liệu và tra cứu skill. Vòng lặp gameplay cốt lõi còn thiếu gần hết; ước lượng khoảng 35–45% của một game chơi được.

## 7. Vấn đề kỹ thuật biết trước

1. **Rò rỉ bộ nhớ:** `LoopGame` tạo 11 đối tượng Skill lẻ (`sNO_SPELL`, `sCOLD_SNAP`, …) không dùng và không giải phóng. Quái không bao giờ bị xóa. Phần dọn dẹp trong `Close()` gần như bị comment hết.
2. **Quái không biến mất:** ra khỏi mép trái thì bị đặt lại về x=800 rồi chạy tiếp. Khi cả 7 loại đã ở trên màn hình thì không spawn thêm. Cơ chế "khoảng cách tối thiểu" trong `respawnEnemy` thực tế vô hiệu vì x luôn là 800 và y luôn là 400.
3. **Biến bị che khuất/không khởi tạo:** trong `LoopGame`, `lastRespawnTime` và `respawnInterval` cục bộ che biến cùng tên của class. Biến thành viên `lastRespawnTime` không được khởi tạo nhưng `respawnEnemy` lại đọc nó, nên lần spawn đầu có thể bị chặn ngẫu nhiên.
4. **Cờ R không reset:** sau lần bấm R đầu tiên, phần hiển thị skill bật vĩnh viễn. `ResetRKey()` tồn tại nhưng không ai gọi.
5. **Thiết kế lộn xộn:** `MainPlayer` giữ hai đối tượng Skill (`skill_` và `skill`). Mỗi Skill tự dựng lại toàn bộ `spellMap` trong constructor. `m_Keylist` luôn rỗng nên vòng lặp tìm phím D/F không làm gì. Có biến không dùng (`activeEnemies`, `availableEnemy`, `rect_D`, `rect_F`).
6. **Lỗi trong thay đổi chưa commit:** trong `GameManager.cpp` có dòng `printf("Enemt path: %s\n", enemy->GetPath())`. `GetPath()` trả về `std::string` còn `%s` cần chuỗi C, nên là hành vi không xác định. Cần sửa (`.c_str()`) hoặc bỏ. Cùng chỗ có một lỗi gõ trong comment ("neededGGG").
7. **File build bị theo dõi trong git:** `.gitignore` đã liệt kê `x64/` và `Debug/` nhưng các file `.exe`, `.pdb`, `.obj`, `.ilk`, `.idb` vẫn nằm trong git và luôn hiện "modified" sau mỗi lần build. Nên gỡ khỏi index (`git rm --cached`) khi chủ project đồng ý.

## 8. Hướng đi

Hướng thiết kế đã chốt (mục 2). Lộ trình chi tiết nằm trong [PLAN.md](PLAN.md); tóm tắt thứ tự:

1. **Ổn định hóa** (Phase 1): build di động, gỡ file build khỏi git, sửa các lỗi ở mục 7, giữ nguyên hành vi game.
2. **Tách Core** (bảng skill/recipe dùng chung, trạng thái orb và ô D/F, hành động input logic), rồi **Practice Mode** (Phase 2–3): quái có `TargetSkillId`, cast D/F, HP, điểm/combo/độ chính xác, độ khó theo thời gian, tách vòng lặp và dùng delta-time.
3. **Bản Web** (Phase 4), rồi UX/âm thanh (Phase 5), Android (Phase 6).
4. Chiến đấu/cốt truyện (Phase 7) và PC/Steam (Phase 8) chỉ sau khi MVP đã được chơi thử.

**Trước khi viết code luật chơi:** đọc GAMEPLAY_SPEC.md. Không còn câu hỏi nào chặn việc triển khai; các giá trị chỉnh được (tốc độ và độ trễ ban đầu, vị trí đường chạm, bảng ánh xạ sprite ↔ skill, phím restart) là hằng số/dữ liệu do người triển khai chọn, ghi ở một chỗ để chỉnh khi chơi thử.

## 9. Quy ước khi làm việc trên repo này

- Style hiện có: tab để thụt lề, tên hàm lẫn lộn giữa `PascalCase` (`LoadImg`, `SetPos`) và `camelCase` (`handleKeyPress`), biến thành viên có hậu tố `_` hoặc tiền tố `m_`. Code mới nên theo style của file đang sửa.
- Comment trong code trộn tiếng Anh và tiếng Việt; commit message bằng tiếng Anh.
- Không có test, không có CI.
- Lịch sử commit: bắt đầu từ game nền (background + nhân vật), rồi refactor OOP, sau đó thêm Keyboard/Skill, gộp combo skill, và gần đây nhất là respawn quái ngẫu nhiên và chỉnh thời gian respawn.
