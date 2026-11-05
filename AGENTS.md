# KhaiRC-ESP32-Hub — Agent Directives

<!-- docs-skill:start -->
## Đặc tả dự án
Trước khi thêm hoặc sửa tính năng, đọc `docs/PROJECT_SPEC.md` để nắm kiến trúc, pin map, module và quy ước.
Sau khi hoàn thành và commit một tính năng, chạy `$docs` để đồng bộ đặc tả.
<!-- docs-skill:end -->

## 1. Bảo mật & Giới hạn quét file (Token & Security Guardrails)
- **TUYỆT ĐỐI KHÔNG** đọc, in hoặc đưa nội dung `include/secrets.h` vào ngữ cảnh hay lịch sử chat. Khi cần kiểm tra tên macro cấu hình, chỉ đọc nơi sử dụng (`src/WifiService.cpp`, `src/CommandApi.cpp`) hoặc `include/secrets.example.h`.
- **KHÔNG** quét hoặc tìm kiếm trong các thư mục sinh tự động: `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`.
- Khi sửa file `.cpp`/`.h`, hãy đọc toàn bộ file đích từ đầu đến cuối thay vì chỉ đọc một đoạn cắt qua `grep` để tránh bỏ sót biến trạng thái trong anonymous namespace.

## 2. Ràng buộc Phần cứng (ESP32-S3-DevKitC-1 / N16R8)
- **Flash & PSRAM:** 16MB QIO Flash + 8MB Octal (OPI) PSRAM (`qio_opi`).
- **Chân cấm sử dụng (Reserved/Strapping Pins):**
  - `GPIO26`–`GPIO37`: Đã dùng nội bộ cho Octal Flash và Octal PSRAM (tuyệt đối không gán ngoại vi).
  - `GPIO0`, `GPIO3`, `GPIO45`, `GPIO46`: Strapping pins.
  - `GPIO19`, `GPIO20`: USB D-/D+.
- **Chân đang gán:** `GPIO4` = `IR_PIN` (RMT TX, active-high, carrier 38kHz, duty 0.33). Không tự ý đổi chân nếu không được yêu cầu.

## 3. Quy ước Kiến trúc & An toàn Lệnh IR
- **Cấu trúc Module:** Mỗi module gồm cặp `include/<Module>.h` (`#pragma once`) và `src/<Module>.cpp` đặt trong `namespace <Module>`, trạng thái nội bộ nằm trong anonymous namespace (`namespace { ... }`).
- **Không dùng thư viện IR bên thứ ba:** Phát IR trực tiếp qua ESP-IDF RMT TX driver (`driver/rmt_tx.h`, `driver/rmt_encoder.h`), giữ buffer `rmt_symbol_word_t` dạng `static`.
- **An toàn phát lệnh (No Auto-Retry):** Các lệnh IR dạng toggle/stateful (như `swing`, `fix`) chỉ được phát đúng 1 lần cho mỗi request. Không tự động retry ở tầng driver hay HTTP khi lỗi để tránh đảo trạng thái thiết bị ngoài ý muốn.
- **Bảo mật HTTP:** Duy trì xác thực `X-API-Key` bằng vòng lặp XOR constant-time, giới hạn `MAX_BODY_SIZE = 512`, kiểm tra embedded NUL và `NestingLimit(3)` khi parse bằng `ArduinoJson`.

## 4. Lệnh Toolchain (PlatformIO)
- Build kiểm tra lỗi biên dịch: `pio run -e esp32-s3-n16r8`
- Nạp firmware (chỉ chạy khi người dùng yêu cầu): `pio run -e esp32-s3-n16r8 -t upload`
- Xem Serial log: `pio device monitor -b 115200`