---
name: docs
description: Tạo và đồng bộ file đặc tả dự án (docs/PROJECT_SPEC.md) cho repo firmware KhaiRC-ESP32-Hub (ESP32-S3, RMT IR, HTTP CommandApi, OTA). Lần chạy đầu quét toàn bộ repo; các lần sau chỉ đọc commit mới kể từ lần đồng bộ trước để cập nhật. Chỉ dùng khi người dùng gọi $docs.
---

# docs — Đặc tả dự án tự đồng bộ

Skill này duy trì MỘT file đặc tả duy nhất: `docs/PROJECT_SPEC.md`. Mục tiêu: trước khi làm tính năng mới, chỉ cần đọc các mục liên quan của file này (qua `python tools/spec.py`) là nắm được kiến trúc dự án (phần cứng, pin map, module, HTTP API, OTA, quy ước, cách thêm tính năng).

- Lần đầu (chưa có spec): **FULL SCAN** toàn bộ repo.
- Các lần sau: **INCREMENTAL** — chỉ xem các commit mới kể từ `last_synced_commit` ghi trong front matter của spec.

Đối số tuỳ chọn:
- `$docs full` — ép quét lại toàn bộ.
- `$docs check` — chỉ báo cáo sẽ làm gì (chế độ nào, file nào đổi), KHÔNG ghi file.

## Nguyên tắc bắt buộc

1. **Thực thi đơn luồng & im lặng:** Không gọi subagent. Thực hiện toàn bộ các bước kiểm tra/cập nhật trong một mạch, chỉ xuất báo cáo tổng kết 1 lần ở cuối.
2. Chỉ ghi điều xác minh được từ repo (code, cấu hình, README, test). Không suy đoán. Chưa chắc → ghi vào §10 "Cần xác nhận". Nếu README mâu thuẫn với code, tin code và ghi mâu thuẫn vào §10.
3. **BẢO MẬT TUYỆT ĐỐI (`AGENTS.md` §1):** TUYỆT ĐỐI KHÔNG đọc, mở hay đưa nội dung `include/secrets.h`, `include/ota_password.h`, `.env` vào ngữ cảnh. Khi cần biết tên macro cấu hình hoặc nơi sử dụng, **chỉ được đọc `include/*.example.h` hoặc nơi gọi macro trong `src/WifiService.cpp`, `src/CommandApi.cpp`, `src/OtaService.cpp`**. Không ghi bất kỳ giá trị mật khẩu, SSID, API key, chứng chỉ nào vào spec.
4. Chỉ được sửa: (a) `docs/PROJECT_SPEC.md`, (b) khối đánh dấu trong `AGENTS.md` (nếu chưa có). Không sửa firmware, không build/flash, không tự commit.
5. Viết bằng tiếng Việt; giữ nguyên identifier, đường dẫn, tên lệnh, tên macro.
6. Spec mô tả hiện trạng (thì hiện tại). Lịch sử chỉ nằm ở §11. Mỗi mục kết thúc bằng dòng `Nguồn: ...` liệt kê file chính để người sau kiểm chứng.
7. Không dán code dài. Chỉ chữ ký hàm public, bảng, hoặc ví dụ ≤ 10 dòng khi thật sự cần.
8. Spec nên gọn (khoảng ≤ 800 dòng) và giữ nguyên cấu trúc đánh số mục (`## 0.`, `## 1.`, `### 5.1`...) để tương thích hoàn toàn với `python tools/spec.py`.

## Bước 0 — Xác định chế độ

Chạy các lệnh sau (dùng cú pháp phù hợp PowerShell; đặt đối số có `^{}` trong dấu nháy):

~~~powershell
git rev-parse --show-toplevel
git rev-parse HEAD
git status --porcelain
~~~

Không phải git repo → báo người dùng rồi dừng.

Đọc phần đầu (front matter) của `docs/PROJECT_SPEC.md` (nếu có) để lấy `last_synced_commit`. Sau đó chọn chế độ:

| Điều kiện | Chế độ |
|---|---|
| Spec chưa có, hoặc thiếu front matter / `last_synced_commit`, hoặc người dùng gọi `full` | FULL |
| `git rev-parse --verify --quiet "<last>^{commit}"` thất bại (commit mất do rebase/force-push) | FULL, nêu rõ lý do |
| Sau khi lọc (xem dưới) không còn file nào thay đổi | UP-TO-DATE: báo "spec đã khớp HEAD", KHÔNG ghi gì (kể cả front matter) |
| Thay đổi > 60 file, hoặc đụng > một nửa số module trong §12 | FULL |
| Còn lại | INCREMENTAL |

Lấy thay đổi kể từ lần đồng bộ trước:

~~~powershell
git log --no-merges --format="%h %s" <last>..HEAD
git diff --name-status -M <last> HEAD
~~~

Nếu `git merge-base --is-ancestor <last> HEAD` thất bại (đã đổi nhánh/rebase) vẫn dùng `git diff <last> HEAD` nhưng ghi chú điều này trong báo cáo.

**Bộ lọc — bỏ qua các đường dẫn:** `docs/PROJECT_SPEC.md` và `docs/spec/**` (chính spec, tránh vòng lặp), `include/secrets.h`, `include/ota_password.h`, `backups/`, `*.patch`, `build/`, `.pio/`, `logs/`, `.git/`, `.vscode/`, `.idea/`, `*.lock`, `*.bin`, `*.elf`, `*.map`, ảnh và file nhị phân khác.

Nếu có thay đổi chưa commit, KHÔNG đưa vào spec. Spec luôn phản ánh đúng HEAD. Nhắc người dùng commit trước rồi chạy lại `$docs` nếu muốn ghi nhận chúng.

Với `$docs check`: in chế độ, khoảng commit, danh sách file sẽ ảnh hưởng, rồi dừng.

## Chế độ FULL SCAN

1. **Kiểm kê:** `git ls-files` (loại trừ vùng cấm ở Bước 0).
2. **Đọc theo thứ tự:**
   1. `AGENTS.md`, `README.md`, `docs/OTA.md` — lấy ý định thiết kế và ràng buộc.
   2. `platformio.ini` — cấu hình board, flash/PSRAM (`qio_opi`), build flags, thư viện.
   3. Điểm vào (`src/main.cpp`: `setup()`/`loop()`), rồi đi theo luồng tới từng module trong `include/*.h` và `src/*.cpp` (chú ý ngoại lệ `src/OtaPage.cpp` theo `AGENTS.md` §1).
   4. `include/*.example.h` để nắm danh sách macro cấu hình/secret.
   5. Test và script hỗ trợ (`test/`, `tools/`).
   6. `git grep -n -E "TODO|FIXME|HACK|XXX" -- src/ include/ test/ tools/` → tổng hợp vào §10.
3. **Viết spec** theo cấu trúc mục hiện có (`§0` đến `§12`). Điền §12 (bản đồ file → mục) đầy đủ.
4. Làm "Bước cuối".

## Chế độ INCREMENTAL

1. Có danh sách commit và `--name-status` đã lọc (Bước 0).
2. Chạy `python tools/spec.py 12` để xem bản đồ file → mục, sau đó chỉ đọc các mục bị ảnh hưởng bằng `python tools/spec.py <mục>`:
   - **A (thêm):** xác định module. Module mới → tạo tiểu mục mới trong §5 và thêm dòng vào §12.
   - **M (sửa):** đọc `git diff <last> HEAD -- <file>` và phần liên quan của file hiện tại, rồi cập nhật đúng mục bị ảnh hưởng.
   - **D (xoá):** gỡ tham chiếu; nếu có mục TODO/known issue liên quan thì cập nhật.
   - **R (đổi tên):** sửa đường dẫn ở mọi nơi (kể cả §12 và dòng `Nguồn:`).
3. Tín hiệu liên mục cần nhớ cập nhật:
   - đổi pin/GPIO/phần cứng → §1; đổi `platformio.ini`/dependency/lệnh build/test → §2
   - đổi luồng `setup()`/`loop()`/thời gian chặn → §4
   - thêm/sửa module trong `src/` & `include/` → §5.x
   - thêm/sửa HTTP endpoint, mã lỗi, `schema_version`, lệnh IR, OTA API → §6
   - đổi cấu hình macro trong `include/*.example.h`, partition → §7
   - xuất hiện quy ước mới → §8 và checklist trong §9
   - commit sửa lỗi đã nằm trong §10 → xoá khỏi §10; TODO mới trong diff → thêm vào §10
4. Chỉ sửa các mục bị ảnh hưởng, giữ nguyên phần còn lại để diff của spec nhỏ và dễ review. KHÔNG viết lại cả file.
5. Thêm một dòng vào §11 (nhật ký) cho lần đồng bộ này (giữ tối đa ~30 dòng gần nhất).

## Bước cuối (cả hai chế độ)

1. Cập nhật front matter của spec: `last_synced_commit` = kết quả `git rev-parse HEAD` (đủ 40 ký tự), `last_synced_at` = ngày hôm nay (`YYYY-MM-DD`). Sau FULL, đặt thêm `last_full_scan_commit` = cùng giá trị.
2. Đảm bảo `AGENTS.md` ở gốc repo có cặp marker `<!-- docs-skill:start -->` ... `<!-- docs-skill:end -->`. Nếu đã có marker thì **không đụng vào**; nếu chưa có mới chèn khối sau:

~~~markdown
<!-- docs-skill:start -->
## Đặc tả dự án
Trước khi thêm hoặc sửa tính năng, đọc các mục liên quan của `docs/PROJECT_SPEC.md` (cách đọc theo mục: §5) để nắm kiến trúc, pin map, module và quy ước.
Sau khi hoàn thành và commit một tính năng, chạy `$docs` để đồng bộ đặc tả.
<!-- docs-skill:end -->
~~~

3. **Tự kiểm tra trước khi báo xong:**
   - Chạy thử `python tools/spec.py` để đảm bảo mục lục của `docs/PROJECT_SPEC.md` parse bình thường, không bị lỗi cú pháp tiêu đề.
   - Không có secret thực tế nào bị lọt vào spec.
   - Front matter hợp lệ, `last_synced_commit` khớp HEAD.
4. **Báo cáo cho người dùng** (tối đa ~10 dòng): chế độ đã chạy và lý do, khoảng commit (`<last7>..<head7>`), các mục đã sửa, mục mới trong §10 cần xác nhận, và nhắc commit file spec.