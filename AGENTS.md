# KhaiRC-ESP32-Hub — Agent Directives

<!-- docs-skill:start -->
## Đặc tả dự án
Trước khi thêm hoặc sửa tính năng, đọc `docs/PROJECT_SPEC.md` để nắm kiến trúc, pin map, module và quy ước.
Sau khi hoàn thành và commit một tính năng, chạy `$docs` để đồng bộ đặc tả.
<!-- docs-skill:end -->

## 1. Bảo mật & Giới hạn quét file (Token & Security Guardrails)
- **TUYỆT ĐỐI KHÔNG** đọc, in hoặc đưa nội dung `include/secrets.h`,`include/ota_password.h` vào ngữ cảnh hay lịch sử chat. Khi cần kiểm tra tên macro cấu hình, chỉ đọc nơi sử dụng (`src/WifiService.cpp`, `src/CommandApi.cpp`) hoặc `include/secrets.example.h`,`include/ota_password.example.h`.
- **KHÔNG** quét hoặc tìm kiếm trong các thư mục sinh tự động: `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`.
- Khi sửa file `.cpp`/`.h`, hãy đọc toàn bộ file đích từ đầu đến cuối thay vì chỉ đọc một đoạn cắt qua `grep` để tránh bỏ sót biến trạng thái trong anonymous namespace.

## 2. Ràng buộc Phần cứng (ESP32-S3-DevKitC-1 / N16R8)
- **Flash & PSRAM:** 16MB QIO Flash + 8MB Octal (OPI) PSRAM (`qio_opi`).
- **Chân cấm sử dụng (Reserved/Strapping Pins):**
  - `GPIO26`–`GPIO37`: Đã dùng nội bộ cho Octal Flash và Octal PSRAM (tuyệt đối không gán ngoại vi).
  - GPIO43, GPIO44: UART0 (Serial/CH343), không gán ngoại vi.
  - `GPIO0`, `GPIO3`, `GPIO45`, `GPIO46`: Strapping pins.
  - `GPIO19`, `GPIO20`: USB D-/D+.
- **Chân đang gán:** `GPIO4` = `IR_PIN` (RMT TX, active-high, carrier 38kHz, duty 0.33). Không tự ý đổi chân nếu không được yêu cầu.
- **Mạch phát IR (hàn cứng trên PCB FR4):** 3V3 → R 33Ω → anode LED Vishay TSAL6200 (940 nm) → cathode vào collector 2N2222A; GPIO4 → R 390Ω → base; emitter → GND. Công tắc phía thấp, active-high (GPIO4 HIGH = LED sáng). Không đổi chân hay đảo logic nếu không được yêu cầu.
## 3. Quy ước Kiến trúc & An toàn Lệnh IR
- **Cấu trúc Module:** Mỗi module gồm cặp `include/<Module>.h` (`#pragma once`) và `src/<Module>.cpp` đặt trong `namespace <Module>`, trạng thái nội bộ nằm trong anonymous namespace (`namespace { ... }`).
- **Không dùng thư viện IR bên thứ ba:** Phát IR trực tiếp qua ESP-IDF RMT TX driver (`driver/rmt_tx.h`, `driver/rmt_encoder.h`), giữ buffer `rmt_symbol_word_t` dạng `static`.
- **An toàn phát lệnh (No Auto-Retry):** Các lệnh IR dạng toggle/stateful (như `swing`, `fix`) chỉ được phát đúng 1 lần cho mỗi request. Không tự động retry ở tầng driver hay HTTP khi lỗi để tránh đảo trạng thái thiết bị ngoài ý muốn.
- **Bảo mật HTTP:** Duy trì xác thực `X-API-Key` bằng vòng lặp XOR constant-time, giới hạn `MAX_BODY_SIZE = 512`, kiểm tra embedded NUL và `NestingLimit(3)` khi parse bằng `ArduinoJson`.

## 4. Lệnh Toolchain (PlatformIO)
- Build kiểm tra lỗi biên dịch: `pio run`
- Nạp firmware (chỉ chạy khi người dùng yêu cầu): `pio run -t upload`
- Xem Serial log: `pio device monitor -b 115200`


## 5. Quy trình phân vai
- `$architect <yêu cầu>`: chỉ ghi `docs/PLAN.md`, không sửa mã nguồn. `$reviewer`: chỉ đọc diff và báo cáo.
- Khi được bảo "thực hiện theo `docs/PLAN.md`": làm đúng từng Step; lệch plan phải nêu lý do, không tự mở rộng phạm vi.
- Không tự `git commit`/`git push` trừ khi được yêu cầu. Xong việc = `pio run` build sạch, kèm danh sách bước thử trên mạch người dùng phải tự làm.
- Đọc `docs/PROJECT_SPEC.md` theo mục (§0, §8, §9 và module liên quan, xem §12); chỉ đọc cả file khi thay đổi liên module.
- Build cần `include/secrets.h` cục bộ. Nếu thiếu: dừng và báo, không tự tạo với giá trị thật.