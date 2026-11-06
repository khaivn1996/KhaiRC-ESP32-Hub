---
spec_version: 1
project: KhaiRC-ESP32-Hub
last_synced_commit: 703392f9990ec3b7f3928fcdb3ba837c8bbe897b
last_synced_at: 2026-11-05
last_full_scan_commit: af15f65f5925459af33332c1623db20488afa0b4
---

# KhaiRC-ESP32-Hub — Đặc tả dự án

> File này do skill `$docs` duy trì. Đọc file này trước khi làm tính năng mới.
> Phạm vi: mã nguồn được commit tại HEAD; không lấy cấu hình cục bộ hay thay đổi chưa commit.

## 0. Tóm tắt nhanh

- Firmware Arduino trên ESP32-S3, build bằng PlatformIO; tên môi trường `esp32-s3-n16r8`.
- Cấu hình phần cứng yêu cầu flash 16 MB QIO và PSRAM 8 MB OPI; dung lượng thực tế cần đối chiếu board.
- Firmware tự khai báo phiên bản `0.4.0-ota1`, có Wi-Fi STA và REST API HTTP cổng 80.
- Registry có bốn thiết bị: quạt và Toshiba báo `ready`/`not_ready` theo driver IR; Daikin và ổ cắm Tuya báo `not_configured`.
- API xác thực bằng `X-API-Key`, phát quạt TAIZ HIGH/SWING/OFF và Toshiba FIX qua RMT trên GPIO4.
- HTTP `200` với `status: sent`, `state_verified: false` chỉ xác nhận hoàn tất phát IR, không xác minh trạng thái thiết bị nhận.
- Không phát IR tự động khi khởi động; Serial `s`/`S` phát TAIZ SWING, `f`/`F` phát Toshiba FIX.
- Có OTA HTTP với trang web nhúng `/update`; chưa có Tuya cloud, học/thu IR hoặc trạng thái thiết bị bền vững. Nguồn OTA: `src/OtaService.cpp`, `src/OtaPage.cpp`.

Nguồn: `platformio.ini`, `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`.

## 1. Phần cứng & pin map

PlatformIO chọn board `esp32-s3-devkitc-1`; tên environment là `esp32-s3-n16r8`. Cấu hình: `board_build.flash_mode=qio`, `board_upload.flash_size=16MB`, `board_build.arduino.memory_type=qio_opi`, `board_build.psram_type=opi`, `BOARD_HAS_PSRAM`. AGENTS xác định N16R8 và mạch FR4: 3V3 → R 33Ω → anode TSAL6200 (940 nm), cathode → collector 2N2222A; GPIO4 → R 390Ω → base, emitter → GND. HIGH làm LED sáng; không tự đổi chân/logic. Chưa có bản vẽ sơ đồ riêng.

| Chức năng | GPIO | Chiều | Ghi chú |
|---|---|---|---|
| IR TX / `IR_PIN` | 4 | Output | GPIO trực tiếp khi thử DC; RMT TX khi phát IR; nghỉ LOW, mark HIGH |
| Serial qua USB-UART | Không gán trong code | Vào/ra | 115200 baud; cấu hình ghi chú CH343, `ARDUINO_USB_CDC_ON_BOOT=0`; AGENTS dành GPIO43/44 cho UART0, không gán ngoại vi |

Chỉ GPIO4 được gán tường minh trong ứng dụng. Không có cấu hình RX IR, I2C, SPI, LEDC hay ngoại vi khác. Không thấy ứng dụng gán các chân 0/3/45/46, 19/20 hoặc 26–37; đây không thay thế kiểm tra sơ đồ board.

`AGENTS.md` xác định phần cứng mục tiêu ESP32-S3-DevKitC-1 / N16R8, flash 16 MB QIO và PSRAM 8 MB OPI (`qio_opi`). Quy định dự án cấm gán ngoại vi vào GPIO26–37 (flash/PSRAM nội bộ), GPIO43/44 (UART0), GPIO0/3/45/46 (strapping) và GPIO19/20 (USB D-/D+); không tự đổi GPIO4 của IR nếu chưa được yêu cầu.

Nguồn: `platformio.ini`, `src/IrDiagnostics.cpp`, `src/main.cpp`, `AGENTS.md`.

## 2. Toolchain, build, flash, test

| Thành phần | Cấu hình/phiên bản xác minh được |
|---|---|
| PlatformIO environment | `esp32-s3-n16r8`, cũng là `default_envs` |
| Platform / framework | `espressif32 @ 8.0.0` / `arduino`; comment cấu hình và tài liệu ghi Arduino 3.3.12; ESP-IDF không khai báo trực tiếp |
| Dependency khai báo | `bblanchon/ArduinoJson @ 7.4.3` |
| Thư viện từ framework | `Arduino.h`, `WiFi.h`, `WebServer.h`; IR dùng `driver/rmt_tx.h`, `driver/rmt_encoder.h`, `esp_err.h` |
| Flash / monitor | `upload_speed=115200`, `monitor_speed=115200`; `upload_port` và `monitor_port` chỉ có dòng comment |
| Partition | `board_build.partitions=default_16MB.csv`; file thuộc framework/toolchain, không nằm trong repo |

Từ thư mục gốc, dùng `pio run` để build, `pio run -t upload` để nạp và `pio device monitor -b 115200` để xem Serial, theo lệnh trong `AGENTS.md`. Hai lệnh PlatformIO dùng default environment `esp32-s3-n16r8`; có thể chọn tường minh bằng `-e esp32-s3-n16r8`. Chỉ nạp firmware khi người dùng yêu cầu. Cần có cấu hình credential cục bộ nêu tại §7. Lần đồng bộ tài liệu không chạy build, flash hay kiểm thử phần cứng.

Có test OTA host: `python tools/test-ota.py --cxx <g++>` chạy 32 kịch bản trên module thật với HTTP/flash giả lập, không đọc credential cục bộ. Test trang: cài Playwright, chạy `python tools/test-ota-page.py --browser <chrome-hoac-msedge>`; 18 kịch bản mạng giả lập tại desktop 1440 px/mobile 390 px; tùy chọn `--screenshots <thu-muc>`. Chưa có CI được track. `lib/README`, `test/README` là mẫu PlatformIO. Không chạy test trong lần sync này. Nguồn: `tools/test-ota.py`, `tools/test-ota-page.py`, `test/ota_host/test_ota.cpp`, `docs/OTA.md`.

Tiện ích `tools/monitor-hub.ps1` chạy trên Windows PowerShell 5.1/PowerShell 7, dùng cmdlet/.NET sẵn có. Lệnh mẫu: `.\tools\monitor-hub.ps1 -HubIP "<IP-hub>" -DurationMinutes 10 -IntervalSeconds 60`. `HubIP` bắt buộc; thời lượng mặc định 10 phút (1–10080), chu kỳ 60 giây (5–3600), timeout 5 giây (1–60). Script gọi GET health rồi system, kiểm tra trường dữ liệu, ghi CSV UTF-8 vào thư mục log bị Git bỏ qua. Các cột là `Time`, `Status`, `UptimeSec`, `FreeHeapBytes`, `RSSIdBm`, `IPAddress`, `Error`; trạng thái `OK`, `ERROR`, `RESET`. `RESET` được suy ra khi uptime giảm, có thể bỏ sót lần khởi động lại giữa hai mẫu. Lịch lấy mẫu theo Stopwatch; cuối lượt in thống kê thành công/lỗi/reset và heap nhỏ/lớn nhất. Script không gửi API key.

Nguồn: `platformio.ini`, `lib/README`, `test/README`, `tools/monitor-hub.ps1`, `src/IrDiagnostics.cpp`, `AGENTS.md`.

## 3. Cấu trúc thư mục & kiến trúc

HEAD có 39 file tracked: src 8 cpp, include 9 header, test 6 file, tools 3 file, .agents 6 file, docs 2 file, lib 1 README, 1 metadata IDE và 3 file gốc. Metadata IDE chỉ được kiểm kê tên, không đọc nội dung; không quét thư mục sinh tự động.

| Vị trí | Vai trò |
|---|---|
| `src/main.cpp` | Điểm vào Arduino, gọi vòng lặp dịch vụ và nhận phím thử IR thủ công |
| `src/`, `include/` | Bảy module: Wi-Fi, API server, command API, device registry, IR diagnostics, OTA service, OTA page |
| `tools/monitor-hub.ps1` | Tiện ích PowerShell thăm dò HTTP health/system và lưu CSV |
| `lib/`, `test/` | README mẫu và test OTA host; chưa có thư viện nội bộ |
| `platformio.ini` | Board, framework, flags, dependency |
| `.gitignore` | Loại build output, cấu hình IDE, credential cục bộ và log khỏi Git |
| `.agents/skills/docs/`, `AGENTS.md`, `docs/PROJECT_SPEC.md` | Skill đồng bộ, hướng dẫn agent và đặc tả dự án |

Luồng phụ thuộc: `main → WifiService, DeviceRegistry, ApiServer, IrDiagnostics`; `ApiServer → WifiService, DeviceRegistry, CommandApi, IrDiagnostics`; `CommandApi → DeviceRegistry, IrDiagnostics, ArduinoJson`; `DeviceRegistry → IrDiagnostics`; `IrDiagnostics → GPIO/RMT`. HTTP commands và Serial dùng chung các hàm phát IR. `ApiServer → OtaService → OtaPage, esp_ota_*`. Nguồn OTA: `src/OtaService.cpp`, `src/OtaPage.cpp`.

Nguồn: `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`, `platformio.ini`, `.gitignore`, `lib/README`, `test/README`, `tools/monitor-hub.ps1`, `.agents/skills/docs/SKILL.md`, `AGENTS.md`.

## 4. Luồng khởi động & runtime

1. `setup()` mở Serial 115200, `delay(1000)`, in tên hub và firmware `0.4.0-ota1`.
2. Gọi lần lượt `WifiService::begin()`, `DeviceRegistry::begin()`, `ApiServer::begin()`, `IrDiagnostics::begin()`.
3. `setup()` log chế độ Serial thủ công và HTTP khả dụng khi driver IR sẵn sàng; không tự phát IR khi boot.
4. `loop()` chạy `WifiService::loop()` trước `ApiServer::loop()`; HTTP commands được thực thi trong lượt xử lý client này.
5. Mỗi vòng đọc tối đa một ký tự Serial; `s`/`S` gọi `testFanSwing()`, `f`/`F` gọi `testToshibaFix()`; CR/LF và ký tự khác bị bỏ qua.
6. Kết thúc vòng bằng `delay(5)`.

| Task/ngữ cảnh | Entry | Stack | Priority | Core | Vai trò |
|---|---|---|---|---|---|
| Vòng lặp Arduino do framework quản lý | `setup()`, `loop()` | Không đặt trong repo | Không đặt trong repo | Không đặt trong repo | Wi-Fi, HTTP và bài thử IR chạy tuần tự |

Ứng dụng không tự tạo FreeRTOS task, queue, event group, mutex, timer, ISR hay cấu hình watchdog. Việc phát RMT chờ hoàn tất đồng bộ; trong thời gian phát/chờ, vòng lặp không phục vụ HTTP hay đọc Serial tiếp. Các luồng nội bộ Wi-Fi/Arduino/RMT do framework quản lý, không có thông số được ghim trong repo.

Nguồn: `src/main.cpp`, `src/WifiService.cpp`, `src/ApiServer.cpp`, `src/IrDiagnostics.cpp`.

## 5. Module

### 5.1 Wi-Fi (`WifiService`)

- Mục đích: kết nối mạng STA, báo thay đổi trạng thái và thử kết nối lại.
- API public: `void WifiService::begin();`, `void WifiService::loop();`, `bool WifiService::connected();`.
- `begin()` đặt hostname `khairc-hub`, `WIFI_STA`, `WiFi.setAutoReconnect(true)`, gọi `WiFi.begin(WIFI_SSID, WIFI_PASSWORD)` rồi lưu thời điểm; không chờ mạng bằng vòng lặp.
- `connected()` so sánh `WiFi.status()` với `WL_CONNECTED`. `loop()` log `[WiFi]` khi online/offline, in IP khi vừa online; nếu đang offline và đã đủ `RETRY_MS=30000` kể từ `begin()` hoặc lần retry trước thì gọi `WiFi.reconnect()` và cập nhật `lastRetry`.
- Phụ thuộc: Arduino WiFi và credential cục bộ; `lastRetry`, `wasConnected` nằm trong RAM. Không có AP, provisioning, mDNS hoặc task riêng trong module.

Nguồn: `include/WifiService.h`, `src/WifiService.cpp`, `src/main.cpp`, `.gitignore`.

### 5.2 HTTP server (`ApiServer`)

- Mục đích: cung cấp API local và gắn vòng đời server với kết nối Wi-Fi.
- API public: `void ApiServer::begin();`, `void ApiServer::loop();`.
- `begin()` đăng ký route GET, tạo boot ID bằng `esp_random()`, thu thập header dùng chung, gọi `CommandApi::registerRoutes(server)` và `OtaService::registerRoutes(server)`, đăng ký `onNotFound`; chưa mở socket.
- `loop()` dừng `WebServer` khi mất mạng; khi có mạng thì mở server nếu chưa chạy, sau đó gọi `handleClient()`.
- Phụ thuộc: `WebServer`, Arduino WiFi, `WifiService`, `DeviceRegistry`, `CommandApi`, `IrDiagnostics`; state nội bộ `server(80)`, `serverRunning`, `bootId`; thêm phụ thuộc OtaService. Không có task riêng.
- Contract chi tiết ở §6.2; phiên bản firmware của health được ghi cố định trong response.

Nguồn: `include/ApiServer.h`, `src/ApiServer.cpp`.

### 5.3 Command gateway (`CommandApi`)

- Mục đích: xác thực và kiểm tra schema lệnh trước khi thực thi phần cứng.
- API public: `void CommandApi::registerRoutes(WebServer& server);`.
- Dùng header `X-API-Key`, `Content-Type` do ApiServer thu thập; lambda của route giữ con trỏ đến server do `ApiServer` sở hữu.
- Helper nội bộ `authenticate()`, `sendError()`, `handleCommand()` nằm trong anonymous namespace. Dùng ArduinoJson để parse; registry để kiểm tra ID thiết bị; `IrDiagnostics` để kiểm tra readiness và phát IR.
- Dispatch `fan_ir` với `power_on` → `sendFanHigh()`, `swing` → `sendFanSwing()`, `power_off` → `sendFanOff()`; `toshiba_ac` với `fix` → `sendToshibaFix()`.
- Mỗi request hợp lệ gọi hàm phát một lần; chỉ trả `200 sent` sau khi RMT transmit/wait thành công. Driver chưa sẵn sàng trả `503 device_not_ready`; lỗi phát trả `500 ir_send_failed`. Không cập nhật trạng thái thực của thiết bị.
- Chạy đồng bộ trong xử lý HTTP, không tạo task hoặc queue. Thứ tự validation và mã lỗi ở §6.2.

Nguồn: `include/CommandApi.h`, `src/CommandApi.cpp`, `src/ApiServer.cpp`, `src/IrDiagnostics.cpp`.

### 5.4 Danh mục thiết bị (`DeviceRegistry`)

- Mục đích: cung cấp catalog cố định được biên dịch vào firmware.
- API public: `void DeviceRegistry::begin();`, `String DeviceRegistry::listJson();`, `String DeviceRegistry::deviceJson(const String& id);`.
- `begin()` chỉ log số lượng thiết bị. JSON được ghép bằng Arduino `String`; `reserve(768)` cho danh sách, `reserve(256)` cho chi tiết.
- Response có `schema_version:1`; danh sách bọc trong `devices`, chi tiết bọc trong `device`. ID không tồn tại trả chuỗi rỗng để caller xử lý.
- Mỗi thiết bị có `id`, `name`, `type`, `transport`, `availability`, `supported_actions`. Với quạt/Toshiba, availability theo `IrDiagnostics::isReady()`; đây là khả năng của driver, không phải trạng thái bật/tắt hay khả năng nhận IR của thiết bị.

| ID logic cục bộ | Name | Type | Transport | Availability | Supported actions |
|---|---|---|---|---|---|
| `daikin_ac` | Daikin AC | `ir_ac` | `ir` | `not_configured` | `[]` |
| `toshiba_ac` | Toshiba AC | `ir_ac` | `ir` | `ready` / `not_ready` | `fix` |
| `fan_ir` | IR Fan | `ir_fan` | `ir` | `ready` / `not_ready` | `power_on`, `swing`, `power_off` |
| `rangdong_plug_01` | Rang Dong Plug | `tuya_plug` | `tuya_cloud` | `not_configured` | `[]` |

Các ID trên là tên logic của catalog, không phải Tuya device ID/credential. Registry phụ thuộc Arduino và `IrDiagnostics`, không tự thực thi lệnh, lưu trạng thái AC hoặc tạo task; mapping thực thi nằm trong `CommandApi`.

Nguồn: `include/DeviceRegistry.h`, `src/DeviceRegistry.cpp`, `src/CommandApi.cpp`, `src/IrDiagnostics.cpp`.

### 5.5 IR diagnostics (`IrDiagnostics`)

- Mục đích: phát các lệnh IR cố định cho HTTP/Serial và cung cấp bài thử GPIO/RMT trực tiếp; không dùng IRremoteESP8266.
- API public: `void IrDiagnostics::begin();`, `bool IrDiagnostics::isReady();`; các hàm `esp_err_t IrDiagnostics::sendFanHigh();`, `esp_err_t IrDiagnostics::sendFanSwing();`, `esp_err_t IrDiagnostics::sendFanOff();`, `esp_err_t IrDiagnostics::sendToshibaFix();`.
- API diagnostics public: `void IrDiagnostics::testBurst();`, `void IrDiagnostics::testDc();`, `void IrDiagnostics::testFanHigh();`, `void IrDiagnostics::testFanSwing();`, `void IrDiagnostics::testToshibaFix();`. Ba wrapper cuối gọi hàm send tương ứng và bỏ return value; lỗi vẫn được log.
- Phụ thuộc: Arduino GPIO, ESP-IDF RMT TX/copy encoder và `esp_err_to_name()`.
- Cấu hình RMT: GPIO4, clock mặc định, resolution 1 MHz (1 tick = 1 µs), `mem_block_symbols=64`, `trans_queue_depth=4`; carrier 38000 Hz, duty 0.33, active-high, `always_on=false`.
- `begin()` tạo channel, áp carrier, tạo copy encoder, enable channel; chỉ đặt `initialized` khi hoàn tất. Lỗi từng bước được log `[IR]` rồi return. `isReady()` yêu cầu cờ này cùng channel/encoder khác null; bốn hàm send trả `ESP_ERR_INVALID_STATE` nếu chưa sẵn sàng.

| Hàm | Dữ liệu / hành vi |
|---|---|
| `testBurst()` | Một symbol MARK 10 ms + SPACE 10 ms, gửi 30 lần; mỗi lần chờ tối đa 1000 ms và delay 80 ms; tổng thời gian bình thường khoảng 3 giây |
| `testDc()` | GPIO LOW 3 giây → HIGH liên tục 15 giây → LOW; không kiểm tra `initialized`, không tạo carrier |
| `sendFanHigh()` | Mảng raw 72 thời lượng MARK/SPACE µs, chuyển thành 36 symbol static; phát toàn mảng TAIZ `POWER_HIGH` một lần |
| `sendFanSwing()` | Mảng raw 72 thời lượng, 36 symbol static; phát toàn mảng TAIZ SWING một lần |
| `sendFanOff()` | Mảng raw 72 thời lượng, 36 symbol static; phát toàn mảng TAIZ OFF một lần |
| `sendToshibaFix()` | Frame cố định 7 byte / 56 bit, LSB first; 58 symbol static gồm header, data, stop; gửi một lần |

Khung Toshiba FIX là `4F B0 80 7F 84 00 84`, dùng header MARK/SPACE 4350/4300 µs; mỗi bit MARK 550 µs, SPACE 1600 µs cho 1 hoặc 550 µs cho 0; stop 550/550 µs. Không ánh xạ FIX thành power/mode/nhiệt độ. Comment mã quạt dẫn nguồn Android ngoài repo; SWING/OFF ghi mốc `323c201`. Lần quét này không đối chiếu source Android.

Bốn hàm send dùng chung `transmitSymbols()`: `loop_count=0`, kết thúc LOW, chờ tối đa 1000 ms khi transmit thành công. Nếu transmit/wait lỗi, helper đặt `initialized=false`, thử `rmt_disable()` và trả lỗi gốc; lỗi disable được log riêng. Không tự retry hoặc tự phục hồi; cần khởi động lại để khởi tạo lại driver. Buffer symbol static vẫn tồn tại sau lỗi wait và readiness chặn lần gửi sau ghi đè buffer.

HTTP gọi các hàm send; Serial gọi `testFanSwing()`/`testToshibaFix()`. `testBurst()` và `testDc()` không được main gọi hoặc mở qua HTTP; `testBurst()` dùng RMT trực tiếp, không qua helper xử lý lỗi trên. Tất cả chạy đồng bộ trong luồng gọi. Không có RX, decoder, học lệnh, gửi raw qua API, driver Daikin hoặc profile AC tổng quát. Dữ liệu IR là mảng C++ trong firmware; không lưu NVS, file hay cloud.

Nguồn: `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp`, `src/main.cpp`, `src/CommandApi.cpp`.

### 5.6 OTA (`OtaService`)

- API public trong namespace OtaService: `void registerRoutes(WebServer& server);`, `void loop();`, `bool available();`, `bool rebootPending();`.
- Phụ thuộc WebServer, OtaPage và ESP-IDF `esp_ota_*`; state/handle/prefix buffer trong anonymous namespace. Không nạp toàn bộ firmware vào RAM, không tạo task riêng.
- Auth so độ dài rồi XOR toàn bộ byte. Kiểm tra auth, body size và prefix trước erase/write flash: magic ảnh, chip ESP32-S3, segment count 1–16, hash appended và app descriptor magic.
- Vùng đích phải là OTA app khác vùng đang chạy, đủ chứa prefix. available còn yêu cầu mật khẩu hợp lệ và chưa chờ reboot.
- Raw callbacks nhận chunk; đủ byte mới `esp_ota_end()` xác minh ảnh/checksum/SHA-256 rồi chọn boot partition. Lỗi/ngắt hủy handle; loop dọn upload bỏ dở giữa các lượt handleClient.
- Reboot sau thành công 1500 ms; không retry upload hoặc tự rollback theo health.

Nguồn: `include/OtaService.h`, `src/OtaService.cpp`, `src/ApiServer.cpp`.

### 5.7 Trang OTA (`OtaPage`)

- API: `const char* OtaPage::html();`; HTML/CSS/JavaScript/SVG nhúng PROGMEM, font hệ thống, không CDN/filesystem.
- Giao diện tiếng Việt, hai cột desktop/một cột mobile, hiển thị version/file/dung lượng/tiến trình và trạng thái.
- Favicon SVG nhúng bằng data URI trong thẻ `link rel="icon"`, chuyển hình học hai thanh bo góc/nghiêng và gradient của `.brand-mark` sang SVG; không cần file ảnh hoặc route riêng. CSP của trang cho phép `img-src data:`.
- Kiểm tra `.bin` không rỗng ≤4 MiB và mật khẩu; GET OTA kiểm tra auth/giới hạn, GET health lấy boot ID rồi XHR POST một lần, timeout 150 giây.
- Sau upload, tối đa 20 GET health cách nhau 2 giây, mỗi GET timeout 5 giây. Chỉ báo thành công khi boot ID đổi; không tự gửi lại firmware.
- Xóa ô mật khẩu sau lượt gửi trong finally, không cookie/localStorage; khóa gửi trùng khi busy.

Nguồn: `include/OtaPage.h`, `src/OtaPage.cpp`, `tools/test-ota-page.py`.

## 6. Giao tiếp ngoài

### 6.1 Tuya cloud

Không áp dụng ở mức triển khai hiện tại: repo không có Tuya SDK/dependency, client cloud, credential Tuya, provisioning/activation, DP, reconnect cloud hay OTA qua cloud. `tuya_cloud` chỉ là nhãn transport của một thiết bị mẫu; capability `tuya_control` và `cloud_access` đều `false`. Chưa có luồng cloud → IR → report trạng thái.

| DP id | Code | Kiểu | Quyền | Mô tả | Hàm xử lý |
|---|---|---|---|---|---|
| Không áp dụng | — | — | — | Chưa có DP trong repo | — |

Nguồn: `platformio.ini`, `src/DeviceRegistry.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`.

### 6.2 HTTP local

Server HTTP cổng 80 trả `application/json`; không cấu hình TLS. GET thông tin chung công khai; POST commands dùng `X-API-Key`; GET OTA và POST upload dùng `X-OTA-Password` riêng (§6.4). Trang OTA trả HTML.

| Method / route | Response / hành vi |
|---|---|
| GET `/api/v1/health` | `200`: `status`, `device`, `firmware`, `boot_id`, `uptime_sec`, `free_heap`; status `ok`, firmware `0.4.0-ota1` |
| GET `/api/v1/system` | `200`: `chip`, `flash_mb`, `psram_mb`, `ip`, `rssi_dbm` lấy từ ESP/Wi-Fi |
| GET `/api/v1/devices` | `200`: `schema_version`, `devices` theo §5.4 |
| GET `/api/v1/devices/<id>` | `200`: `schema_version`, `device`; ID không tồn tại → `404 device_not_found` |
| GET `/api/v1/capabilities` | `200`: `schema_version:1`, object `capabilities`; `ir_send` theo `IrDiagnostics::isReady()`, `tuya_control`/`cloud_access` là `false`; `ota_update` theo `OtaService::available()` |
| POST `/api/v1/commands` | Xác thực + validation theo bảng dưới; `200` sau khi transmit/wait thành công |

Route chi tiết thiết bị được xử lý trong `onNotFound`; toàn bộ hậu tố sau `/api/v1/devices/` là ID. Sai method ở route đã biết trả `405 method_not_allowed` và `Allow: GET` hoặc `Allow: POST`; route khác trả `404 not_found`. JSON lỗi thống nhất dạng `{"error":{"code":"..."}}`.

Thứ tự xử lý POST commands:

| Bước | Điều kiện | HTTP / error khi không đạt |
|---|---|---|
| 1 | Header `X-API-Key` trùng `HUB_API_KEY`; so độ dài trước, rồi XOR toàn bộ byte khi cùng độ dài | `401 unauthorized` |
| 2 | `Content-Type` bằng `application/json` hoặc bắt đầu `application/json;` | `415 unsupported_media_type` |
| 3 | `clientContentLength()` và body đều không vượt `MAX_BODY_SIZE=512` byte | `413 payload_too_large` |
| 4 | Body không rỗng, parse được JSON object; `NestingLimit(3)` | `400 invalid_json` |
| 5 | `device_id`, `action` là string không rỗng, không có NUL nhúng; lần lượt tối đa 48 và 32 ký tự | `400 invalid_command` |
| 6 | Registry có ID này | `404 device_not_found` |
| 7 | Action thuộc từ vựng của thiết bị: quạt `power_on`/`swing`/`power_off`, Toshiba `fix`; Daikin/Tuya chỉ chấp nhận từ vựng `power_on`/`power_off` | `422 unsupported_action` |
| 8 | Chỉ quạt/Toshiba có driver và `IrDiagnostics::isReady()` phải đúng; Daikin/Tuya chưa thực thi được dù vượt qua bước 7 | `503 device_not_ready` |
| 9 | Gọi hàm send một lần, kết quả `ESP_OK` | `500 ir_send_failed` |

Ánh xạ lệnh thực thi:

| `device_id` | `action` | Tín hiệu phát |
|---|---|---|
| `fan_ir` | `power_on` | TAIZ HIGH (`POWER_HIGH`), không phải lựa chọn mọi tốc độ |
| `fan_ir` | `swing` | TAIZ SWING |
| `fan_ir` | `power_off` | TAIZ OFF |
| `toshiba_ac` | `fix` | Toshiba FIX, không phải điều khiển AC tổng quát |

Request mẫu: `{"device_id":"fan_ir","action":"swing"}`. Response `200` có `schema_version:1`, `device_id`, `action`, `transport:"ir"`, `status:"sent"`, `state_verified:false`. `sent` chỉ xác nhận bộ phát hoàn tất; không có readback trạng thái thiết bị nhận.

Field JSON dư không bị từ chối. Giới hạn body nằm trong handler ứng dụng. Không có nonce, timestamp chống replay, idempotency key hoặc rate limit trong code. Mỗi request mới có thể phát lại tín hiệu; mất response/timeout không chứng minh chưa phát. Client cần tránh tự retry POST hoặc tự fallback sang kênh khác với SWING/FIX. Sau lỗi transmit/wait, driver chuyển sang không sẵn sàng (§5.5); các lệnh quạt/Toshiba hợp lệ tiếp theo trả `503` cho đến khi khởi động lại.

Nguồn: `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`.

### 6.3 Wi-Fi, Serial và kết nối khác

Wi-Fi STA dùng hostname `khairc-hub`; auto-reconnect cùng retry 30 giây, HTTP dừng/mở theo trạng thái mạng (§5.1–§5.2). Không có AP/BLE provisioning, mDNS, MQTT, WebSocket, HTTP client cloud, Home Assistant integration hay web UI điều khiển thiết bị; web UI hiện có phục vụ OTA. Serial 115200 nhận `s`/`S` để phát TAIZ SWING, `f`/`F` để phát Toshiba FIX và xuất log chẩn đoán.

Nguồn: `src/WifiService.cpp`, `src/ApiServer.cpp`, `src/main.cpp`, `platformio.ini`.

### 6.4 OTA HTTP

| Method / route | Contract |
|---|---|
| GET `/` | 302, Location `/update` |
| GET `/update` | HTML công khai; no-store/nosniff/frame DENY; CSP cho inline script/style, `img-src data:` cho favicon nhúng, kết nối cùng origin |
| GET `/api/v1/ota` | `X-OTA-Password`; trả `max_firmware_bytes` = min(4 MiB, kích thước vùng đích) |
| POST `/update` | Mật khẩu OTA, `application/octet-stream`, Content-Length thập phân bắt buộc; không multipart/Transfer-Encoding; 200 `status:updated`, `rebooting:true` sau verify/chọn boot |

Sai method trả 405 cùng Allow. Lỗi: 401 unauthorized; 503 ota_not_configured/reboot_pending/ota_partition_unavailable; 415 unsupported_media_type; 411 content_length_required; 413 firmware_too_large; 408 upload_timeout; 400 cho transfer encoding/length/size/prefix/ảnh/thiếu dữ liệu/verify; 500 cho begin/write/chọn boot. Request bị reject đóng kết nối thay vì hút hết body.

`ApiServer::loop()` gọi OTA loop trước kiểm tra Wi-Fi; chờ reboot thì ngừng phục vụ client mới, vẫn reboot nếu mạng rớt. Upload raw đồng bộ trong handleClient, có thể trì hoãn API/Serial; timeout nhận 120 giây, đọc mạng 5 giây.

HTTP không mã hóa, chỉ dùng LAN tin cậy. SHA-256 kiểm tra toàn vẹn, không xác thực nhà phát hành. Không thay bootloader/partition table, không rollback theo health. Firmware hợp lệ nhưng sai Wi-Fi/thiếu OTA có thể cần USB phục hồi. Hướng dẫn tại `docs/OTA.md`.

Nguồn: `src/OtaService.cpp`, `src/OtaPage.cpp`, `src/ApiServer.cpp`, `docs/OTA.md`.

## 7. Dữ liệu & cấu hình

- Partition: dùng tên `default_16MB.csv` từ framework trong `platformio.ini`; repo không có CSV riêng. Bảng trong `docs/OTA.md` ghi otadata offset `0xe000`, size `0x2000`; app0/ota_0 offset `0x10000`, app1/ota_1 offset `0x650000`, mỗi app size `0x640000` (6,25 MiB). Không đọc package sinh tự động trong lần sync; OTA kiểm tra vùng đích thực tế khác vùng đang chạy, đúng loại OTA và đủ kích thước.
- NVS / filesystem: ứng dụng không khai báo namespace/key, không dùng Preferences, NVS API, EEPROM, SPIFFS, LittleFS hay FATFS. Điều này không xác định hành vi lưu nội bộ của framework Wi-Fi.
- Dữ liệu: catalog và mã IR cố định trong firmware; cờ Wi-Fi/server/RMT và các buffer symbol static trong RAM. JSON API registry/capabilities/command thành công dùng `schema_version=1`; chưa có định dạng lưu bền hoặc migration.
- Build flags: `BOARD_HAS_PSRAM`, `ARDUINO_USB_CDC_ON_BOOT=0`; cấu hình flash/PSRAM và dependency ở `platformio.ini`. Không có Kconfig, `sdkconfig.defaults`, file `.ino` hay cấu hình ESP-IDF độc lập được track.
- Credential cục bộ: `WIFI_SSID`, `WIFI_PASSWORD` do `src/WifiService.cpp` sử dụng; `HUB_API_KEY` do `src/CommandApi.cpp` sử dụng. Các file này include `secrets.h`; đường dẫn cục bộ `include/secrets.h` bị `.gitignore` loại khỏi Git và **không thuộc HEAD**. Có template `include/secrets.example.h`; không ghi giá trị thật. OTA dùng `HUB_OTA_PASSWORD` trong file cục bộ `include/ota_password.h` bị Git bỏ qua, template `include/ota_password.example.h`. Mật khẩu độc lập API key, 1–128 ký tự ASCII 33–126; thiếu/rỗng/sai định dạng khóa OTA. Nguồn: `src/OtaService.cpp`, `.gitignore`, `include/ota_password.example.h`.
- `static_assert(sizeof(HUB_API_KEY) == 65)` yêu cầu biểu diễn khóa tương ứng chuỗi 64 ký tự kèm ký tự kết thúc; code không kiểm tra bảng ký tự hex/base64.
- Monitor ghi thư mục đầu ra `logs/` (bị Git bỏ qua), tên file theo mẫu `hub-monitor-<timestamp>.csv`; đây là đầu ra trên máy chạy PowerShell, không phải storage của ESP32.

Nguồn: `platformio.ini`, `.gitignore`, `src/WifiService.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/main.cpp`, `tools/monitor-hub.ps1`.

## 8. Quy ước & pattern của codebase

- Mỗi module có header trong `include/` và implementation cùng tên trong `src/`; namespace trùng tên module, API nhỏ và rõ. Header dùng `#pragma once`.
- State/helper riêng nằm trong anonymous namespace; constants dùng `constexpr`, trạng thái runtime dùng biến module hoặc static local.
- Service theo cặp `begin()`/`loop()` khi cần được phục vụ liên tục; `main` quyết định thứ tự khởi tạo và polling. Registry và diagnostics không có `loop()`.
- Log qua `Serial`, tag `[WiFi]`, `[API]`, `[CommandAPI]`, `[IR]`, `[IR TEST]` và tag registry; lỗi RMT đổi sang tên bằng `esp_err_to_name()` rồi return.
- HTTP dùng lambda `server.on()`, lỗi JSON thống nhất; command xác thực trước validation nội dung, kiểm tra action/readiness rồi gọi send một lần. Chỉ báo thành công sau transmit/wait và luôn giữ `state_verified:false`.
- Các hàm send trả `esp_err_t`, dùng buffer static và helper chung; lỗi phát làm driver không sẵn sàng, không tự thử lại. Các diagnostics GPIO/burst không dùng helper này.
- Response thường ghép `String`; ArduinoJson chỉ parse lệnh. Registry reserve dung lượng trước khi ghép, các khung IR dùng mảng fixed-size/static hoặc local, không có allocator riêng.
- Firmware version là chuỗi cố định ở startup và health, chưa có nguồn version dùng chung. Catalog có schema version độc lập với firmware version.
- `AGENTS.md` yêu cầu đọc spec trước khi sửa tính năng và chạy `$docs` sau khi commit. Skill đồng bộ chỉ ghi spec/khối marker, lấy trạng thái đã commit, bỏ qua secret; metadata skill đặt `allow_implicit_invocation: false`.
- Giới hạn thao tác theo `AGENTS.md`: tuyệt đối không đọc/in nội dung `include/secrets.h` hoặc `include/ota_password.h`; tra tên macro tại nơi sử dụng hoặc template `include/secrets.example.h`, `include/ota_password.example.h`. Không quét `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`. Trước khi sửa `.cpp`/`.h`, đọc toàn bộ file đích để nắm trạng thái nội bộ.
- Ràng buộc triển khai theo `AGENTS.md`: không dùng thư viện IR bên thứ ba, dùng ESP-IDF RMT TX và buffer `rmt_symbol_word_t` static; mỗi request toggle/stateful chỉ phát một lần, không retry ở driver/HTTP. Duy trì XOR constant-time khi xác thực `X-API-Key`, `MAX_BODY_SIZE=512`, kiểm tra NUL nhúng và `NestingLimit(3)`.

Nguồn: `include/ApiServer.h`, `include/CommandApi.h`, `include/DeviceRegistry.h`, `include/IrDiagnostics.h`, `include/WifiService.h`, `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`, `AGENTS.md`, `.agents/skills/docs/SKILL.md`, `.agents/skills/docs/agents/openai.yaml`.

## 9. Cách thêm tính năng mới

Các checklist dưới đây là điểm sửa rút ra từ cấu trúc hiện tại, không mô tả tính năng đã có:

- **Thêm thiết bị/giao thức IR:** khai báo hàm send tại `include/IrDiagnostics.h`, triển khai tại `src/IrDiagnostics.cpp` theo pattern readiness/buffer static/trả lỗi; đăng ký thiết bị và `supported_actions` tại `src/DeviceRegistry.cpp`; nối validation/dispatch trong `src/CommandApi.cpp`, kiểm tra capability trong `src/ApiServer.cpp`; chỉ thêm phím thử thủ công tại `src/main.cpp` nếu cần. Code chưa có registry handler động hay driver AC dùng chung.
- **Thêm DP Tuya:** chưa có SDK, transport, bảng DP hay handler để mở rộng trực tiếp. Cần xác định tích hợp mới trước; các điểm nối hiện có là dependency trong `platformio.ini`, catalog trong `src/DeviceRegistry.cpp`, lệnh trong `src/CommandApi.cpp`, capability trong `src/ApiServer.cpp` và lifecycle tại `src/main.cpp`. Không suy ra DP từ nhãn `tuya_plug`.
- **Thêm chân/ngoại vi:** đối chiếu board, `AGENTS.md` và `platformio.ini`; tuân thủ danh sách chân cấm tại §1 và không tự đổi GPIO4. Chọn vị trí cấu hình tương tự `IR_PIN` trong `src/IrDiagnostics.cpp`, cập nhật header/lifecycle cần thiết và pin map §1; kiểm tra xung đột phần cứng trước thử nghiệm.
- **Thêm Kconfig/NVS key:** hiện chưa có cơ chế này. Cấu hình build nằm ở `platformio.ini`; credential theo include cục bộ được `.gitignore` bảo vệ. Nếu bổ sung storage, cần định nghĩa namespace/key, phiên bản và migration trước khi ghi chúng vào §7.
- **Thêm route/action:** sửa module API liên quan và `include/CommandApi.h` nếu đổi public API; duy trì auth, body limit, kiểm tra NUL nhúng, mã lỗi và xử lý sai method; đồng bộ registry/capabilities với dispatch. Giữ nghĩa `sent`/`state_verified` và không tự phát lại sau lỗi. Có thể cập nhật `tools/monitor-hub.ps1` khi metric health/system đổi.

Trước khi sửa `.cpp`/`.h`, đọc toàn bộ file đích theo §8. Sau thay đổi, kiểm tra bằng toolchain/hardware phù hợp (§2); chỉ upload khi người dùng yêu cầu, test OTA host/trang không thay thế kiểm thử radio, flash và nguồn trên board. Sau khi commit, chạy `$docs` để đồng bộ spec với HEAD mới.

Nguồn: `platformio.ini`, `.gitignore`, `src/main.cpp`, `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp`, `src/DeviceRegistry.cpp`, `include/CommandApi.h`, `src/CommandApi.cpp`, `src/ApiServer.cpp`, `tools/monitor-hub.ps1`, `test/README`, `AGENTS.md`.

Quy trình bổ sung: `$architect` chỉ ghi kế hoạch, `$reviewer` chỉ đọc diff/báo cáo. Làm đúng từng Step khi được giao plan, nêu lý do lệch; không tự commit/push. Sửa OTA cần giữ auth → prefix → write → verify → chọn boot → reboot, đồng bộ host/page tests và hướng dẫn OTA; chỉ retry GET health. Trên board cần thử sai mật khẩu, upload hợp lệ, boot ID đổi, IR sau reboot và cập nhật cả hai chiều phân vùng.

Nguồn: `AGENTS.md`, `.agents/skills/architect/SKILL.md`, `.agents/skills/reviewer/SKILL.md`, `.agents/skills/architect/agents/openai.yaml`, `.agents/skills/reviewer/agents/openai.yaml`, `src/OtaService.cpp`, `docs/OTA.md`.

## 10. Vấn đề đã biết, TODO, cần xác nhận

- [ ] Chưa có bản vẽ sơ đồ riêng; AGENTS đã mô tả mạch IR/linh kiện/điện trở. — Nguồn: `AGENTS.md`.
- [ ] Phiên bản ESP-IDF chưa khai báo trực tiếp; platform đã ghim, Arduino 3.3.12 được ghi trong config/tài liệu. Chưa có CI được track. — Nguồn: `platformio.ini`, `docs/OTA.md`.
- [ ] Checkout HEAD chưa đủ credential để build độc lập: cần cấu hình cục bộ cho `WIFI_SSID`, `WIFI_PASSWORD`, `HUB_API_KEY`; đã có template `include/secrets.example.h`. — Nguồn: `.gitignore`, `src/WifiService.cpp`, `src/CommandApi.cpp`.
- [ ] Driver IR không tự phục hồi sau lỗi transmit/wait: `initialized=false`, các lệnh gửi hợp lệ tiếp theo trả `503`; cần khởi động lại. Đây là hành vi hiện tại, không có API reset driver. — Nguồn: `src/IrDiagnostics.cpp`, `src/CommandApi.cpp`.
- [ ] Chưa có log kiểm thử phần cứng được track để tra cứu từng lệnh; mã IR cố định và comment tham chiếu Android không thay thế hồ sơ kiểm thử. Không có RX/readback để xác minh trạng thái thiết bị, response luôn `state_verified:false`. — Nguồn: `src/IrDiagnostics.cpp`, `src/CommandApi.cpp`, `test/README`.
- [ ] Daikin/Tuya chưa có driver, Toshiba chỉ hỗ trợ FIX; chưa có điều khiển AC tổng quát, report trạng thái; OTA chỉ cập nhật hub qua LAN. Catalog Daikin/Tuya có `supported_actions:[]`, nhưng validation vẫn nhận từ vựng `power_on`/`power_off` trước khi trả `503`. — Nguồn: `src/DeviceRegistry.cpp`, `src/CommandApi.cpp`, `src/ApiServer.cpp`.

Không tìm thấy marker `TODO`, `FIXME`, `HACK`, `XXX` trong mã ứng dụng, cấu hình build, script và các README mẫu. Không có README dự án để đối chiếu mâu thuẫn; hai README hiện có là tài liệu mẫu PlatformIO.

Nguồn: `platformio.ini`, `src/main.cpp`, `src/IrDiagnostics.cpp`, `src/DeviceRegistry.cpp`, `src/CommandApi.cpp`, `src/ApiServer.cpp`, `src/WifiService.cpp`, `.gitignore`, `lib/README`, `test/README`.

- [ ] OTA chưa có rollback theo health/Wi-Fi sau boot; cần xác nhận trên board hai chiều phân vùng và IR sau reboot. Test host/trang chỉ mô phỏng HTTP/flash/mạng. — Nguồn: `src/OtaService.cpp`, `docs/OTA.md`, `test/ota_host/test_ota.cpp`.
- [ ] `docs/OTA.md` còn câu tính năng ở working tree dù OTA đã commit; spec lấy code HEAD làm chuẩn. — Nguồn: `docs/OTA.md`, `src/OtaService.cpp`.

Nguồn: `docs/OTA.md`, `src/OtaService.cpp`.

## 11. Nhật ký đồng bộ

- 2026-11-02 FULL @19f655c — tạo đặc tả, kiểm kê đủ 22 file tracked; bắt đầu tại a7240ce, cập nhật thêm tài liệu/skill từ hai commit phát sinh trong lúc quét, firmware không đổi. Metadata IDE chỉ kiểm kê; không đưa cấu hình cục bộ hay thay đổi chưa commit vào đặc tả.
- 2026-11-05 FULL 19f655c..5a47234 — quét lại 22 file tracked vì thay đổi 4/5 module; cập nhật HTTP IR `0.3.0-ir1`, lệnh quạt/Toshiba, readiness, lỗi phát, Serial thủ công và bỏ tự phát khi boot. Không build/flash; không đọc credential cục bộ.
- 2026-11-05 INCR 5a47234..f9f0063 — đồng bộ quy định AGENTS về chân cấm, bảo mật/quét file, RMT và an toàn lệnh, lệnh toolchain và điều kiện upload; firmware không đổi.

- 2026-11-05 FULL f9f0063..af15f65 — quét 39 file vì chạm 3/5 module cũ; OTA, test, toolchain ghim, template credential, phần cứng và quy trình agent. Không build/flash.
- 2026-11-05 INCR af15f65..703392f — favicon SVG từ brand-mark, CSP cho data image; đồng bộ lệnh PlatformIO mặc định và cấm đọc credential OTA trong AGENTS. Không build/flash.

Nguồn: cây Git và lịch sử commit tại mốc đồng bộ trong front matter.

## 12. Bản đồ file → mục

Mỗi file trong 39 file tracked tại mốc đồng bộ thuộc một dòng dưới đây. Bảy module chức năng là §5.1–§5.7; thay đổi phần dùng chung cần xét thêm các mục liên quan.

| Glob | Mục | Ghi chú |
|---|---|---|
| `src/main.cpp` | §0, §4, §6.3, §8–§10 | Entry Arduino, nhận phím IR thủ công |
| `include/WifiService.h`, `src/WifiService.cpp` | §4, §5.1, §6.3, §7–§10 | Module Wi-Fi |
| `include/ApiServer.h`, `src/ApiServer.cpp` | §4, §5.2, §6, §8–§10 | Module HTTP server |
| `include/CommandApi.h`, `src/CommandApi.cpp` | §4, §5.3, §6.2, §7–§10 | Module command gateway |
| `include/DeviceRegistry.h`, `src/DeviceRegistry.cpp` | §5.4, §6, §7–§10 | Module catalog |
| `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp` | §0–§4, §5.2–§5.5, §6.2, §7–§10 | Driver phát IR dùng chung HTTP/Serial và diagnostics |
| `platformio.ini` | §1, §2, §3, §7, §9, §10 | Build, dependency, board |
| `.gitignore` | §2, §3, §7, §9, §10 | File sinh ra, credential và log |
| `.vscode/**` | §3 | Chỉ kiểm kê tên metadata IDE, không đọc nội dung |
| `lib/README`, `test/README` | §2, §3, §10 | README mẫu |
| `tools/**` | §2, §3, §7, §9 | Monitor HTTP trên máy phát triển |
| `.agents/skills/**` | §3, §8, §9 | Skill và metadata điều khiển đồng bộ đặc tả |
| `docs/PROJECT_SPEC.md` | §0–§12 | Spec tự quản lý; bỏ qua diff đầu vào |
| `include/OtaService.h`, `src/OtaService.cpp` | §4, §5.6, §6.4, §7–§10 | OTA stream |
| `include/OtaPage.h`, `src/OtaPage.cpp` | §5.7, §6.4 | Trang OTA nhúng |
| `include/*.example.h` | §2, §7, §10 | Template credential |
| `test/ota_host/**` | §2, §10 | Test và stub |
| `docs/OTA.md` | §2, §6.4, §7, §9–§10 | Hướng dẫn OTA |
| `AGENTS.md` | §1, §2, §8, §9 | Hướng dẫn đồng bộ, giới hạn quét, phần cứng, kiến trúc và an toàn lệnh |

Các tên `include/secrets.h`, `logs/`, mẫu CSV log và `default_16MB.csv` ở §2/§7 là cấu hình cục bộ/đầu ra runtime/tài nguyên framework, không phải đường dẫn nguồn được track. Không có thư mục `docs/spec/` hoặc spec module tách riêng tại lần quét này.

Nguồn: danh sách `git ls-files` tại mốc đồng bộ; `platformio.ini`, `.gitignore`.
