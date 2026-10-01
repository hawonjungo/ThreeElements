# Bật bảng xếp hạng Google Play Games — việc cần làm trên Play Console

**Trạng thái 2026-10-01:** chủ project đã tạo hai bảng và gửi mã; `android/play-games.properties` đã có ba mã và bản
1.7.0 đã bật tính năng. Việc còn lại trên Console: kiểm tra `appId`, khai đủ ba SHA-1 (Bước 2), thêm tester
(Bước 4), và Bước 6.

Phần code (GAMEPLAY_SPEC.md §29) chỉ bật khi có file `android/play-games.properties` chứa ba mã lấy từ Play Console.
Tài liệu này là các bước để lấy ba mã đó. Tên các mục trên Console có thể hơi khác
tùy thời điểm và ngôn ngữ giao diện.

Kết quả cuối cùng cần gửi lại cho Claude (hoặc tự điền vào `android/play-games.properties`, mẫu ở
`play-games.properties.example`):

```
appId=<mã dự án Play Games, toàn chữ số>
leaderboardPlay=<mã bảng PLAY>
leaderboardSurvival=<mã bảng SURVIVAL>
```

Ba mã này không phải bí mật (chúng nằm trong mọi APK đã phát hành).

## Bước 1 — Tạo dự án Play Games Services

1. Play Console → app **Injoker** → **Play Games Services** → **Setup and management** → **Configuration**.
2. Chọn "No, my game doesn't use Google APIs" → tạo dự án mới, đặt tên `Injoker`.
3. Console sẽ tạo kèm một dự án Google Cloud. Khi được hỏi, cấu hình **OAuth consent screen**: tên app `Injoker`,
   email hỗ trợ và email liên hệ của bạn.

## Bước 2 — Khai khóa ký (bước hay sai nhất)

Trong **Configuration** → **Add credential** → loại **Android** → tạo OAuth client với package `net.relifes.injoker`
và dấu vân tay **SHA-1**. Cần tạo **một credential cho mỗi khóa**:

| Khóa | Dùng cho | Lấy SHA-1 ở đâu |
|---|---|---|
| App signing key | Bản người chơi tải từ Play (kể cả thử kín) | Play Console → Test and release → Setup → **App signing** → "App signing key certificate" |
| Upload key | Bản release tự cài tay từ file AAB | Cùng trang đó → "Upload key certificate" |
| Debug key của máy này | Bản debug chạy trên máy ảo / cắm USB | `55:34:0B:AE:54:41:D9:09:3E:E3:28:77:09:21:B0:FA:7F:F3:4B:8B` |

Thiếu khóa nào thì bản tương ứng sẽ không đăng nhập được (game vẫn chơi bình thường, chỉ không có bảng toàn cầu).

**Đã gặp thật (2026-10-01):** mới có credential cho app signing key thì bản debug trên máy ảo báo
"Could not sign in to Google Play Games"; log (`adb logcat`, tag `PlayGamesServices[SignInAuthenticator]`) ghi
`DEVELOPER_ERROR` kèm package, SHA-1 và App ID mà app đang dùng. Thêm credential cho khóa debug là hết.
SHA-1 của khóa upload (đọc từ file AAB): `CA:93:E7:7D:C0:C8:BD:8B:B6:61:80:A0:3C:6C:47:0C:60:F7:50:6E`.

## Bước 3 — Tạo hai bảng xếp hạng

**Play Games Services** → **Setup and management** → **Leaderboards** → **Add leaderboard**:

| Tên | Score format | Ordering | Ghi chú |
|---|---|---|---|
| `PLAY - Score` | Numeric, 0 chữ số thập phân | Larger is better | Điểm của một lượt PLAY |
| `SURVIVAL - Time` | Time (game gửi **mili giây**) | Larger is better | Thời gian sống của một lượt Survival |

Mỗi bảng tự có ba khung thời gian: hôm nay, tuần này, mọi thời đại. Không cần tạo riêng bảng tuần.
Nên bật giới hạn điểm (ví dụ PLAY tối đa 100000, SURVIVAL tối đa 6 giờ) để lọc điểm giả.

## Bước 4 — Thêm tester

**Play Games Services** → **Setup and management** → **Testers**: thêm email Google của bạn và của 12 người thử kín.
Trước khi dự án Play Games được publish, chỉ các tài khoản này đăng nhập được.

## Bước 5 — Gửi ba mã

- `appId`: dãy số ngay dưới tên game ở trang **Configuration**.
- Hai mã bảng: cột ID trong danh sách **Leaderboards** (dạng `CgkI...`).

Sau khi có ba mã, Claude sẽ: điền `play-games.properties`, sửa trang privacy policy và nội dung cửa hàng, tăng phiên
bản (1.7.0), build, và thử đăng nhập trên máy ảo có tài khoản tester.

## Bước 6 — Trước khi phát hành cho mọi người

1. **Data safety** (Play Console → App content): app không còn là "không có dữ liệu nào rời khỏi máy". Khi người chơi
   đăng nhập Play Games, điểm của họ được gửi cho Google gắn với hồ sơ Play Games. Khai theo trang hướng dẫn
   "data disclosure" của Google dành cho Play Games Services SDK (kiểm tra bản mới nhất; đừng dựa vào trí nhớ).
2. **Publish dự án Play Games** (Play Games Services → Publishing): cần tên hiển thị, mô tả, icon 512 px, ảnh
   1024×500 (đã có trong `art/store/`). Chưa publish thì người ngoài danh sách tester không đăng nhập được.
3. Tải bản AAB mới lên track thử kín.

## Ghi chú kỹ thuật

- Bản có Play Games vẫn **không xin quyền nào** (kể cả quyền internet): thư viện nói chuyện qua Google Play services
  của máy. Đã kiểm tra trên APK build thử.
- Thư viện: `com.google.android.gms:play-services-games-v2:21.0.0` (bản mới nhất còn hỗ trợ Android 5; bản 22 cần
  Android 7 trở lên).
- Máy ảo để thử phải là loại có Google Play và đã đăng nhập một tài khoản tester.
- Đã thử trên máy ảo với tài khoản của chủ project (2026-10-01): tự đăng nhập lúc mở app, nút đổi thành GLOBAL
  RANKING, màn hình của Google hiện hai bảng, kết quả tốt nhất của máy đã lên bảng. Chưa thử: bản tải từ Play
  (khóa app signing) trên điện thoại thật.
