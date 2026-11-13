# KhaiRC-ESP32-Hub — Agent Directives

<!-- docs-skill:start -->
## Đặc tả dự án
Trước khi thêm hoặc sửa tính năng, đọc các mục liên quan của `docs/PROJECT_SPEC.md` (cách đọc theo mục: §5) để nắm kiến trúc, pin map, module và quy ước.
Sau khi hoàn thành và commit một tính năng, chạy `$docs` để đồng bộ đặc tả.
<!-- docs-skill:end -->

## 1. Bảo mật & Giới hạn quét file (Token & Security Guardrails)
- **TUYỆT ĐỐI KHÔNG** đọc, in hoặc đưa nội dung `include/secrets.h`, `include/ota_password.h`, `.env` vào ngữ cảnh hay lịch sử chat. Khi cần tên macro, chỉ đọc nơi dùng (`src/WifiService.cpp`, `src/CommandApi.cpp`, `src/OtaService.cpp`) hoặc `include/*.example.h`.
- **KHÔNG** quét hoặc tìm kiếm trong `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`, `backups/` và các file `*.patch` (ngoại lệ: đọc đúng file `.pio/build.log` theo §4).
- Khi sửa `.cpp`/`.h`, đọc toàn bộ file đích từ đầu đến cuối để không bỏ sót biến trong anonymous namespace. Ngoại lệ `src/OtaPage.cpp`: phần giữa là HTML/CSS/JS nhúng trong raw string `R"OTAHTML(`; chỉ đọc phần C++ ở đầu/cuối file, và chỉ đọc đoạn HTML liên quan khi đang sửa giao diện.

## 2. Ràng buộc Phần cứng (ESP32-S3-DevKitC-1 / N16R8)
- **Flash & PSRAM:** 16MB QIO Flash + 8MB Octal (OPI) PSRAM (`qio_opi`).
- **Chân cấm sử dụng (Reserved/Strapping Pins):**
  - `GPIO26`–`GPIO37`: Đã dùng nội bộ cho Octal Flash và Octal PSRAM (tuyệt đối không gán ngoại vi).
  - `GPIO43`, `GPIO44`: UART0 (Serial/CH343), không gán ngoại vi.
  - `GPIO0`, `GPIO3`, `GPIO45`, `GPIO46`: Strapping pins.
  - `GPIO19`, `GPIO20`: USB D-/D+.
- **Chân đang gán:** `GPIO4` = `IR_PIN` (RMT TX, active-high, carrier 38kHz, duty 0.33). Không tự ý đổi chân nếu không được yêu cầu.
- **Mạch phát IR (hàn cứng trên PCB FR4):** 3V3 → R 33Ω → anode LED Vishay TSAL6200 (940 nm) → cathode vào collector 2N2222A; GPIO4 → R 390Ω → base; emitter → GND. Công tắc phía thấp, active-high (GPIO4 HIGH = LED sáng). Không đổi chân hay đảo logic nếu không được yêu cầu.

## 3. Quy ước Kiến trúc & An toàn Lệnh IR
- **Cấu trúc Module:** Mỗi module gồm cặp `include/<Module>.h` (`#pragma once`) và `src/<Module>.cpp` đặt trong `namespace <Module>`, trạng thái nội bộ nằm trong anonymous namespace (`namespace { ... }`).
- **Không dùng thư viện IR bên thứ ba:** Phát IR trực tiếp qua ESP-IDF RMT TX driver (`driver/rmt_tx.h`, `driver/rmt_encoder.h`), giữ buffer `rmt_symbol_word_t` dạng `static`.
- **An toàn phát lệnh (No Auto-Retry):** Các lệnh IR dạng toggle/stateful (như `swing`, `fix`) chỉ được phát đúng 1 lần cho mỗi request. Không tự động retry ở tầng driver hay HTTP khi lỗi để tránh đảo trạng thái thiết bị ngoài ý muốn.
- **Bảo mật HTTP:** Duy trì xác thực `X-API-Key` bằng vòng lặp XOR constant-time, giới hạn `MAX_BODY_SIZE = 512`, kiểm tra embedded NUL và `NestingLimit(3)` khi parse bằng `ArduinoJson`.
- **OTA (`OtaService`/`OtaPage`):** giữ thứ tự xác thực mật khẩu → đọc prefix → ghi → xác minh → chọn boot → reboot; xác thực trước khi xóa/ghi flash. Không có mật khẩu mặc định (thiếu/rỗng = khóa OTA). Chỉ dùng `esp_ota_*`, không thư viện OTA bên thứ ba. Không tự retry upload (chỉ retry GET health). Mật khẩu không vào URL/localStorage/cookie. Giới hạn 4 MiB, không nhận multipart/chunked. Đổi OTA thì cập nhật `docs/OTA.md` và test host (§4).

## 4. Toolchain & Kiểm chứng (PowerShell trên Windows)
- **Build:** `pio run *> .pio/build.log; Get-Content .pio/build.log -Tail 25`. Nếu lỗi: `Select-String .pio/build.log -Pattern 'error' | Select-Object -First 15`. Không in cả log build.
- Nếu `pio` không có trong PATH: dùng `"$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"`.
- **Test host OTA** (khi đụng `OtaService`/`OtaPage`/`test/ota_host`): `python tools/test-ota.py --cxx <g++.exe>`; `g++.exe` nằm trong `$env:USERPROFILE\.platformio\packages\toolchain-gccmingw32\bin` (xem `docs/OTA.md` §6), không thấy thì hỏi người dùng. Test trang: `python tools/test-ota-page.py --browser <chrome.exe|msedge.exe>`.
- Nạp firmware (chỉ khi người dùng yêu cầu): `pio run -t upload`. Xem Serial log: `pio device monitor -b 115200` (chặn vô hạn: không tự chạy, nhờ người dùng dán log).
- Cùng một lỗi build/test sửa quá 2 lần chưa xong thì dừng, đọc lại lỗi gốc rồi báo cáo; không sửa mò.
- **Task chỉ đổi cấu hình agent (Markdown/YAML):** không chạy `pio run`. Kiểm chứng: `git diff --stat`; quét ký tự ẩn `Select-String -Path CLAUDE.md,AGENTS.md,.agents\skills\*\SKILL.md,.agents\skills\*\agents\openai.yaml -Pattern '[\u200B-\u200F\u00A0\uFEFF\u2060\u202A-\u202E\u00AD]'` (mong đợi: không có kết quả); mỗi file `.md` có số dòng bắt đầu bằng ``` hoặc `~~~` là số chẵn.

## 5. Quy trình phân vai & Đọc đặc tả (Mỗi vai một chat riêng)
- `$architect <yêu cầu>`: chỉ ghi `docs/PLAN.md` (Zero-Discovery Plan, trích xuất sẵn ngữ cảnh code).
- `$implement [phạm vi]`: sửa code liền mạch cho toàn bộ các Step trong phạm vi; chỉ chạy build/test và tick `[x]` vào `docs/PLAN.md` 1 lần ở cuối. Không tự gọi lại `tools/spec.py` (dùng thẳng thông tin trong `PLAN.md`).
- `$reviewer`: chỉ đọc diff, chạy kiểm chứng và báo cáo. `$debugger`: chỉ chẩn đoán từ bằng chứng. `$docs`: chạy sau khi commit để đồng bộ `docs/PROJECT_SPEC.md`.
- **Cách đọc `docs/PROJECT_SPEC.md` (áp dụng cho `$architect`, `$reviewer`, `$debugger`):** không đọc cả file (~40 KB); chạy `python tools/spec.py` in mục lục; `python tools/spec.py 0 8 9 5.5` in các mục cần. Luôn đọc §0 và §8; thêm §9 khi thêm tính năng; thêm module/§6 liên quan (tra bản đồ file → mục ở §12).
- Không tự `git commit`/`git push`/upload. Xong việc = build sạch + test host liên quan pass + liệt kê các bước người dùng phải thử trên mạch. Ngoại lệ: task chỉ đổi Markdown/YAML cấu hình agent thì không chạy `pio run`; kiểm chứng theo §4 (bullet "Task chỉ đổi cấu hình agent").
- Build cần `include/secrets.h` cục bộ (và `include/ota_password.h` nếu bật OTA). Thiếu thì dừng và báo, không tự tạo với giá trị thật.

## 6. Context7 MCP — Tra cứu tài liệu kỹ thuật
- Chỉ gọi Context7 MCP khi cần tra cứu API/cú pháp thư viện chưa rõ sau khi đã kiểm tra `platformio.ini` và source code cục bộ.
- Ưu tiên tài liệu đúng phiên bản đã cài đặt; các ràng buộc trong `AGENTS.md` và `PROJECT_SPEC.md` luôn có quyền ưu tiên cao hơn tài liệu mẫu.
- Tuyệt đối không gửi secret, token, mật khẩu hay nội dung file nhạy cảm vào truy vấn Context7.

## 7. Chế độ thực thi im lặng & Đơn luồng (Strict Quiet & Single-Agent Mode)
- **CẤM gọi Subagents:** Thực hiện 100% công việc trực tiếp trên luồng agent chính (main thread). Tuyệt đối không khởi tạo subagent, background task hay explore agent dưới mọi hình thức để tránh lãng phí token.
- **CẤM báo cáo trung gian (Zero Mid-Turn Commentary):** Trong một lượt thực hiện nhiệm vụ, âm thầm gọi nối tiếp các công cụ đọc/sửa/kiểm tra cho đến khi hoàn tất toàn bộ yêu cầu; không xuất tin nhắn cập nhật tiến độ xen giữa các bước gọi tool. Chỉ dừng giữa chừng khi thiếu file cấu hình bắt buộc, phát hiện mâu thuẫn với mã nguồn, hoặc lỗi build/test lặp quá 2 lần (§4).
- **Trung thực bằng chứng:** Trước khi kết luận, đối chiếu với mã nguồn và kết quả build/test thực tế. Phân biệt rõ thông tin **đã xác minh**, **chưa xác minh** và **giả định**; không tự nhận đã test pass nếu chưa chạy lệnh kiểm chứng.
- **Định dạng báo cáo cuối cùng (Chỉ xuất 1 lần khi hoàn tất):** Nếu đang chạy một `$skill` trong `.agents/skills/`, tuân thủ đúng mẫu báo cáo đầu ra của skill đó. Nếu làm việc tự do không qua skill, báo cáo ngắn gọn gồm: việc đã làm, kết quả kiểm chứng, tồn đọng/rủi ro (nếu có) và thao tác người dùng cần thực hiện.