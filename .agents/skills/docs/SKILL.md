---
name: docs
description: Tạo và đồng bộ file đặc tả dự án (docs/PROJECT_SPEC.md) cho repo firmware ESP32-S3 điều khiển hồng ngoại (IR), Tuya cloud... Lần chạy đầu quét toàn bộ repo; các lần sau chỉ đọc commit mới kể từ lần đồng bộ trước để cập nhật. Dùng khi người dùng gọi $docs, hoặc nói "cập nhật đặc tả", "sync spec", "update project spec", "viết tài liệu dự án".
---

# docs — Đặc tả dự án tự đồng bộ

Skill này duy trì MỘT file đặc tả duy nhất: `docs/PROJECT_SPEC.md`. Mục tiêu: trước khi làm tính năng mới, chỉ cần đọc file này là nắm được toàn bộ dự án (phần cứng, kiến trúc, module, giao tiếp Tuya, quy ước, cách thêm tính năng).

- Lần đầu (chưa có spec): **FULL SCAN** toàn bộ repo.
- Các lần sau: **INCREMENTAL** — chỉ xem các commit mới kể từ `last_synced_commit` ghi trong front matter của spec.

Đối số tuỳ chọn:
- `$docs full` — ép quét lại toàn bộ.
- `$docs check` — chỉ báo cáo sẽ làm gì (chế độ nào, file nào đổi), KHÔNG ghi file.

## Nguyên tắc bắt buộc

1. Chỉ ghi điều xác minh được từ repo (code, cấu hình, README, CI). Không suy đoán. Chưa chắc → ghi vào §10 "Cần xác nhận". Nếu README mâu thuẫn với code, tin code và ghi mâu thuẫn vào §10.
2. KHÔNG ghi secret hay giá trị nhạy cảm: Wi-Fi SSID/password, Tuya product key / device ID / device secret / access key / token, API key, chứng chỉ. Chỉ ghi TÊN biến/key và NƠI cấu hình (ví dụ: "Kconfig `CONFIG_TUYA_DEVICE_SECRET`, đặt trong `sdkconfig.local`"). Được đọc `sdkconfig`, `secrets.h`, `.env` để hiểu cấu trúc nhưng không chép giá trị.
3. Chỉ được sửa: (a) `docs/PROJECT_SPEC.md`, (b) khối đánh dấu trong `AGENTS.md`. Không sửa firmware, không build/flash, không tự commit.
4. Viết bằng tiếng Việt; giữ nguyên identifier, đường dẫn, tên lệnh, tên macro.
5. Spec mô tả hiện trạng (thì hiện tại). Lịch sử chỉ nằm ở §11. Mỗi mục kết thúc bằng dòng `Nguồn: ...` liệt kê file chính để người sau kiểm chứng.
6. Không dán code dài. Chỉ chữ ký hàm public, bảng, hoặc ví dụ ≤ 10 dòng khi thật sự cần.
7. Spec nên gọn (khoảng ≤ 800 dòng). Nếu một module quá dài, tách sang `docs/spec/<module>.md`, để link trong spec chính và ghi vào §12.

## Bước 0 — Xác định chế độ

Chạy các lệnh sau (dùng cú pháp phù hợp shell hiện tại; trên PowerShell hãy đặt đối số có `^{}` trong dấu nháy):

```
git rev-parse --show-toplevel
git rev-parse HEAD
git status --porcelain
```

Không phải git repo → báo người dùng rồi dừng.

Đọc front matter của `docs/PROJECT_SPEC.md` (nếu có) để lấy `last_synced_commit`. Sau đó chọn chế độ:

| Điều kiện | Chế độ |
|---|---|
| Spec chưa có, hoặc thiếu front matter / `last_synced_commit`, hoặc người dùng gọi `full` | FULL |
| `git rev-parse --verify --quiet "<last>^{commit}"` thất bại (commit mất do rebase/force-push) | FULL, nêu rõ lý do |
| Sau khi lọc (xem dưới) không còn file nào thay đổi | UP-TO-DATE: báo "spec đã khớp HEAD", KHÔNG ghi gì (kể cả front matter) |
| Thay đổi > 60 file, hoặc đụng > một nửa số module trong §12 | FULL |
| Còn lại | INCREMENTAL |

Lấy thay đổi kể từ lần đồng bộ trước:

```
git log --no-merges --format="%h %s" <last>..HEAD
git diff --name-status -M <last> HEAD
```

Nếu `git merge-base --is-ancestor <last> HEAD` thất bại (đã đổi nhánh/rebase) vẫn dùng `git diff <last> HEAD` nhưng ghi chú điều này trong báo cáo.

**Bộ lọc — bỏ qua các đường dẫn:** `docs/PROJECT_SPEC.md` và `docs/spec/**` (chính spec, tránh vòng lặp khi commit spec), `build/`, `.pio/`, `managed_components/`, `node_modules/`, `.vscode/`, `.idea/`, `*.lock`, `dependencies.lock`, `*.bin`, `*.elf`, `*.map`, `sdkconfig` và `sdkconfig.old` (file sinh ra; chỉ đọc `sdkconfig` khi repo không có `sdkconfig.defaults`), ảnh và file nhị phân khác.

Nếu có thay đổi chưa commit, KHÔNG đưa vào spec. Spec luôn phản ánh đúng HEAD. Nhắc người dùng commit trước rồi chạy lại `$docs` nếu muốn ghi nhận chúng.

Với `$docs check`: in chế độ, khoảng commit, danh sách file sẽ ảnh hưởng, rồi dừng.

## Chế độ FULL SCAN

1. **Kiểm kê:** `git ls-files` (chỉ file được track, tự tôn trọng `.gitignore`). Thống kê theo thư mục và đuôi file để hình dung quy mô. Không mở `build/`, `managed_components/`, file sinh tự động, file nhị phân.
2. **Đọc theo thứ tự**, ghi note vào mục tương ứng của spec ngay sau khi đọc xong từng module (đừng giữ hết trong đầu):
   1. README, AGENTS.md, CHANGELOG, docs/ có sẵn, ADR — lấy ý định thiết kế.
   2. Cấu hình build và dependency — xem checklist bên dưới.
   3. Điểm vào (`app_main` hoặc `setup()/loop()`), rồi đi theo luồng khởi động tới từng module.
   4. Từng module/component: header (API public) trước, rồi file `.c/.cpp` cho logic quan trọng. File quá lớn thì đọc header + phần khai báo + các hàm public.
   5. Test, CI, script hỗ trợ (`tools/`, `scripts/`, `.github/`).
   6. `git grep -n -E "TODO|FIXME|HACK|XXX"` → tổng hợp vào §10.
3. **Viết spec** theo "Mẫu spec" bên dưới. Điền §12 (bản đồ file → mục) đầy đủ: mọi file được track phải thuộc một dòng glob.
4. Làm "Bước cuối".

### Checklist quét cho dự án ESP32-S3 + IR + Tuya

- **Framework:** ESP-IDF (`CMakeLists.txt` gốc, `main/`, `components/`, `idf_component.yml`, `sdkconfig.defaults*`, `partitions*.csv`), PlatformIO (`platformio.ini`) hay Arduino (`*.ino`). Ghi phiên bản IDF/platform và danh sách dependency kèm phiên bản.
- **Phần cứng:** tên board/module, flash size, PSRAM. Pin map: tìm `GPIO_NUM_`, `gpio_num_t`, `#define .*_PIN`, `pinMode(`, cấu hình RMT/LEDC/I2C/SPI/UART. Lập bảng: chức năng | GPIO | chiều | ghi chú (active-low, pull-up...). Nếu code dùng chân hay gây lỗi trên ESP32-S3 (strapping 0/3/45/46, USB 19/20, chân flash/PSRAM khoảng 26–37 tuỳ module), ghi cảnh báo vào §10 để người dùng đối chiếu datasheet module.
- **IR:** driver/thư viện đang dùng (RMT, IRremoteESP8266, tự viết...), giao thức hỗ trợ, tần số sóng mang/duty, buffer, cơ chế học lệnh (learn) và phát (send), nơi lưu mã IR (NVS/file/cloud), định dạng dữ liệu, danh sách thiết bị/profile, task nào xử lý.
- **Tuya cloud:** SDK/giao thức đang dùng (đọc từ dependency và `#include`, đừng đoán), cách kích hoạt/provisioning, bảng DP (id | code | kiểu | quyền | mô tả | hàm xử lý), luồng "lệnh từ cloud → thực thi IR → báo cáo trạng thái", xử lý mất mạng/reconnect, OTA, nơi cấu hình credential (chỉ tên).
- **Kết nối khác:** Wi-Fi (STA/AP, provisioning), BLE, MQTT/HTTP/WebSocket/mDNS, web server cục bộ, API local.
- **Runtime:** bảng FreeRTOS task (tên, entry, stack, priority, core), queue/event group/mutex/timer/ISR, watchdog, luồng khởi động.
- **Lưu trữ:** partition table, NVS namespace/key, SPIFFS/LittleFS/FATFS, phiên bản dữ liệu và migration.
- **Cấu hình:** `Kconfig.projbuild`, `sdkconfig.defaults*`, build flags, các key menuconfig quan trọng.
- **Build/test/CI:** lệnh build, flash, monitor; unit test (Unity, pytest-embedded...); workflow CI.
- **Thành phần đi kèm:** web UI, app, tích hợp Home Assistant... nếu có trong repo.

## Chế độ INCREMENTAL

1. Có danh sách commit và `--name-status` đã lọc (Bước 0).
2. Với mỗi file thay đổi, tra §12 để biết nó thuộc mục nào:
   - **A (thêm):** xác định module. Module mới → tạo tiểu mục mới trong §5 và thêm dòng vào §12.
   - **M (sửa):** đọc `git diff <last> HEAD -- <file>` và phần liên quan của file hiện tại, rồi cập nhật mục bị ảnh hưởng.
   - **D (xoá):** gỡ tham chiếu; nếu có mục TODO/known issue liên quan thì cập nhật.
   - **R (đổi tên):** sửa đường dẫn ở mọi nơi (kể cả §12 và dòng `Nguồn:`).
3. Tín hiệu liên mục cần nhớ cập nhật:
   - đổi pin/GPIO → §1; đổi dependency/phiên bản/lệnh build/test/CI → §2
   - thêm/sửa task, queue, stack, priority → §4
   - thêm/sửa DP Tuya, topic, endpoint → §6
   - đổi Kconfig, `sdkconfig.defaults`, partitions, NVS key → §7
   - xuất hiện pattern mới lặp lại nhiều lần → §8 và checklist trong §9
   - commit sửa lỗi đã nằm trong §10 → xoá khỏi §10; TODO mới trong diff → thêm vào §10
4. Chỉ sửa các mục bị ảnh hưởng, giữ nguyên phần còn lại để diff của spec nhỏ và dễ review. KHÔNG viết lại cả file.
5. Thêm một dòng vào §11 (nhật ký) cho lần đồng bộ này.

## Bước cuối (cả hai chế độ)

1. Cập nhật front matter của spec: `last_synced_commit` = kết quả `git rev-parse HEAD` (đủ 40 ký tự), `last_synced_at` = ngày hôm nay. Sau FULL, đặt thêm `last_full_scan_commit` = cùng giá trị. Chỉ làm bước này khi spec đã được ghi xong.
2. Đảm bảo `AGENTS.md` ở gốc repo có khối dưới đây. Nếu chưa có file thì tạo file; nếu có file mà thiếu cặp marker thì thêm vào cuối; nếu đã có marker thì không đụng:

```markdown
<!-- docs-skill:start -->
## Đặc tả dự án
Trước khi thêm hoặc sửa tính năng, đọc `docs/PROJECT_SPEC.md` để nắm kiến trúc, pin map, module và quy ước.
Sau khi hoàn thành và commit một tính năng, chạy `$docs` để đồng bộ đặc tả.
<!-- docs-skill:end -->
```

3. **Tự kiểm tra trước khi báo xong:**
   - Mọi đường dẫn nhắc trong spec đều tồn tại (`git ls-files` đối chiếu).
   - Không có secret: tìm lại trong spec các chuỗi dạng hex/base64 dài ≥ 16 ký tự và các từ `secret`, `password`, `token`, `key`, rồi xác nhận chỉ là tên biến, không phải giá trị.
   - Front matter hợp lệ, `last_synced_commit` khớp HEAD.
   - §12 phủ hết thư mục chính của repo; không còn file/module mồ côi.
4. **Báo cáo cho người dùng** (tối đa ~10 dòng): chế độ đã chạy và lý do, khoảng commit (`<last7>..<head7>`), các mục đã sửa, mục mới trong §10 cần họ xác nhận, và nhắc commit file spec (cùng `AGENTS.md` nếu mới tạo).

## Mẫu spec

Dùng nguyên cấu trúc dưới đây cho FULL SCAN. Thay phần trong `<...>` bằng nội dung thật; xoá mục nào chắc chắn không áp dụng (ghi "Không áp dụng" thay vì để trống).

````markdown
---
spec_version: 1
project: <tên dự án>
last_synced_commit: <sha 40 ký tự>
last_synced_at: <YYYY-MM-DD>
last_full_scan_commit: <sha 40 ký tự>
---

# <Tên dự án> — Đặc tả dự án

> File này do skill `$docs` duy trì. Đọc file này trước khi làm tính năng mới.

## 0. Tóm tắt nhanh
<5–10 dòng: dự án làm gì, chạy trên phần cứng nào, điều khiển được những gì, kết nối cloud nào, trạng thái hiện tại.>

## 1. Phần cứng & pin map
<Board/module, flash, PSRAM.>

| Chức năng | GPIO | Chiều | Ghi chú |
|---|---|---|---|

Nguồn: ...

## 2. Toolchain, build, flash, test
<Framework + phiên bản, dependency chính kèm phiên bản, lệnh build/flash/monitor, cách chạy test, CI.>

Nguồn: ...

## 3. Cấu trúc thư mục & kiến trúc
<Cây thư mục rút gọn (chỉ thư mục quan trọng, mỗi dòng một câu mô tả) + sơ đồ phụ thuộc giữa các module bằng chữ hoặc mermaid.>

## 4. Luồng khởi động & runtime
<Trình tự từ `app_main`/`setup()` đến khi sẵn sàng.>

| Task | Entry | Stack | Priority | Core | Vai trò |
|---|---|---|---|---|---|

<Queue / event group / mutex / timer / ISR quan trọng.>

## 5. Module
### 5.1 <Tên module> (ví dụ: IR tx/rx)
- Mục đích:
- File chính:
- API public (chữ ký):
- Phụ thuộc:
- Task/ISR liên quan:
- Lưu ý/ràng buộc:

Nguồn: ...

### 5.2 <Module tiếp theo> (Tuya, Wi-Fi/provisioning, OTA, storage, ...)

## 6. Giao tiếp ngoài
### 6.1 Tuya cloud
| DP id | Code | Kiểu | Quyền | Mô tả | Hàm xử lý |
|---|---|---|---|---|---|

<Luồng lệnh cloud → IR → báo trạng thái; reconnect; OTA.>

### 6.2 Giao tiếp khác (MQTT/HTTP/BLE/local API)

## 7. Dữ liệu & cấu hình
- Partition table:
- NVS namespace/key:
- File system (SPIFFS/LittleFS/...):
- Kconfig / sdkconfig.defaults — các key quan trọng:
- Secret/credential: chỉ ghi TÊN và NƠI cấu hình, không ghi giá trị.

## 8. Quy ước & pattern của codebase
<Đặt tên, xử lý lỗi, logging (tag), cấp phát bộ nhớ, cách đăng ký handler/command, cấu trúc một module chuẩn.>

## 9. Cách thêm tính năng mới
<Checklist theo từng loại, rút ra từ code thực tế. Mỗi checklist liệt kê file cần sửa theo thứ tự.>
- Thêm giao thức/thiết bị IR mới:
- Thêm DP Tuya mới:
- Thêm chân/ngoại vi mới:
- Thêm Kconfig/NVS key mới:

## 10. Vấn đề đã biết, TODO, cần xác nhận
- [ ] <mục> — nguồn: <file hoặc "mâu thuẫn README/code">

## 11. Nhật ký đồng bộ
- <YYYY-MM-DD> FULL @<sha7> — quét toàn bộ.
- <YYYY-MM-DD> INCR <sha7>..<sha7> — <các mục đã cập nhật>.

## 12. Bản đồ file → mục
| Glob | Mục | Ghi chú |
|---|---|---|
| `main/**` | §4, §5.1 | |
| `components/<tên>/**` | §5.x | |
| `CMakeLists.txt`, `idf_component.yml`, `platformio.ini` | §2 | |
| `sdkconfig.defaults*`, `Kconfig*`, `partitions*.csv` | §7 | |
| `.github/**`, `test/**` | §2 | |
````

Giữ §11 tối đa khoảng 30 dòng gần nhất; các dòng cũ hơn gộp thành một dòng tóm tắt theo tháng khi dài quá.
