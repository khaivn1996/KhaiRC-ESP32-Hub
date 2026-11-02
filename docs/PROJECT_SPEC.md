---
spec_version: 1
project: KhaiRC-ESP32-Hub
last_synced_commit: 19f655cc4a3680e362f4c963aaa5b39c2b3d9a4f
last_synced_at: 2026-11-02
last_full_scan_commit: 19f655cc4a3680e362f4c963aaa5b39c2b3d9a4f
---

# KhaiRC-ESP32-Hub — Đặc tả dự án

> File này do skill `$docs` duy trì. Đọc file này trước khi làm tính năng mới.
> Phạm vi: mã nguồn được commit tại HEAD; không lấy cấu hình cục bộ hay thay đổi chưa commit.

## 0. Tóm tắt nhanh

- Firmware Arduino trên ESP32-S3, build bằng PlatformIO; tên môi trường `esp32-s3-n16r8`.
- Cấu hình phần cứng yêu cầu flash 16 MB QIO và PSRAM 8 MB OPI; dung lượng thực tế cần đối chiếu board.
- Firmware tự khai báo phiên bản `0.2.0-dev`, có Wi-Fi STA và REST API HTTP cổng 80.
- Registry có bốn thiết bị mẫu; tất cả mang trạng thái `not_configured`.
- API nhận `power_on`/`power_off`, xác thực bằng `X-API-Key`, nhưng lệnh hợp lệ vẫn trả `503 device_not_ready`.
- IR hiện là các bài thử phát trực tiếp qua RMT trên GPIO4, chưa nối vào API lệnh.
- Mỗi lần khởi động có một lần phát Toshiba tự động khi `millis() >= 10000`; Serial `f`/`F` phát lại.
- Không có triển khai Tuya cloud, học/thu IR, OTA, web UI hay lưu trạng thái bền vững trong mã ứng dụng được track.

Nguồn: `platformio.ini`, `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`.

## 1. Phần cứng & pin map

PlatformIO chọn board `esp32-s3-devkitc-1`; tên environment là `esp32-s3-n16r8`. Cấu hình: `board_build.flash_mode=qio`, `board_upload.flash_size=16MB`, `board_build.arduino.memory_type=qio_opi`, `board_build.psram_type=opi`, `BOARD_HAS_PSRAM`. Repo không chứa sơ đồ mạch hay mã module cụ thể.

| Chức năng | GPIO | Chiều | Ghi chú |
|---|---|---|---|
| IR TX / `IR_PIN` | 4 | Output | GPIO trực tiếp khi thử DC; RMT TX khi phát IR; nghỉ LOW, mark HIGH |
| Serial qua USB-UART | Không gán trong code | Vào/ra | 115200 baud; cấu hình ghi chú CH343, `ARDUINO_USB_CDC_ON_BOOT=0`; không xác nhận chân UART từ repo |

Chỉ GPIO4 được gán tường minh trong ứng dụng. Không có cấu hình RX IR, I2C, SPI, LEDC hay ngoại vi khác. Không thấy ứng dụng gán các chân 0/3/45/46, 19/20 hoặc 26–37; đây không thay thế kiểm tra sơ đồ board.

Nguồn: `platformio.ini`, `src/IrDiagnostics.cpp`, `src/main.cpp`.

## 2. Toolchain, build, flash, test

| Thành phần | Cấu hình/phiên bản xác minh được |
|---|---|
| PlatformIO environment | `esp32-s3-n16r8`, cũng là `default_envs` |
| Platform / framework | `espressif32` / `arduino`; không ghim phiên bản platform, Arduino core hay ESP-IDF |
| Dependency khai báo | `bblanchon/ArduinoJson @ 7.4.3` |
| Thư viện từ framework | `Arduino.h`, `WiFi.h`, `WebServer.h`; IR dùng `driver/rmt_tx.h`, `driver/rmt_encoder.h`, `esp_err.h` |
| Flash / monitor | `upload_speed=115200`, `monitor_speed=115200`; `upload_port` và `monitor_port` chỉ có dòng comment |
| Partition | `board_build.partitions=default_16MB.csv`; file thuộc framework/toolchain, không nằm trong repo |

Từ thư mục gốc, các lệnh sử dụng environment đã khai báo là `pio run -e esp32-s3-n16r8`, `pio run -e esp32-s3-n16r8 -t upload`, `pio device monitor -b 115200`. Cần có cấu hình credential cục bộ nêu tại §7. Đây là cách gọi theo cấu hình; lần đồng bộ tài liệu không chạy build, flash hay kiểm thử phần cứng.

Repo chỉ có README mẫu trong `test/`, không có test thực thi, workflow CI hoặc script build riêng. `include/README` và `lib/README` cũng là hướng dẫn PlatformIO mẫu, không mô tả thiết kế firmware.

Tiện ích `tools/monitor-hub.ps1` chạy trên Windows PowerShell 5.1/PowerShell 7, dùng cmdlet/.NET sẵn có. Lệnh mẫu: `.\tools\monitor-hub.ps1 -HubIP "<IP-hub>" -DurationMinutes 10 -IntervalSeconds 60`. `HubIP` bắt buộc; thời lượng mặc định 10 phút (1–10080), chu kỳ 60 giây (5–3600), timeout 5 giây (1–60). Script gọi GET health rồi system, kiểm tra trường dữ liệu, ghi CSV UTF-8 vào thư mục log bị Git bỏ qua. Các cột là `Time`, `Status`, `UptimeSec`, `FreeHeapBytes`, `RSSIdBm`, `IPAddress`, `Error`; trạng thái `OK`, `ERROR`, `RESET`. `RESET` được suy ra khi uptime giảm, có thể bỏ sót lần khởi động lại giữa hai mẫu. Lịch lấy mẫu theo Stopwatch; cuối lượt in thống kê thành công/lỗi/reset và heap nhỏ/lớn nhất. Script không gửi API key.

Nguồn: `platformio.ini`, `include/README`, `lib/README`, `test/README`, `tools/monitor-hub.ps1`, `src/IrDiagnostics.cpp`.

## 3. Cấu trúc thư mục & kiến trúc

HEAD có 22 file được track: `src/` 6 file `.cpp`, `include/` 5 header và 1 README, `lib/`, `test/`, `tools/`, `.vscode/`, `docs/` mỗi thư mục 1 file, `.agents/` 2 file, cùng `platformio.ini`, `.gitignore` và `AGENTS.md`. Metadata IDE được kiểm kê nhưng bỏ qua khi phân tích chức năng.

| Vị trí | Vai trò |
|---|---|
| `src/main.cpp` | Điểm vào Arduino, gọi vòng lặp dịch vụ và phát IR thử |
| `src/`, `include/` | Năm module: Wi-Fi, API server, command API, device registry, IR diagnostics |
| `tools/monitor-hub.ps1` | Tiện ích PowerShell thăm dò HTTP health/system và lưu CSV |
| `lib/`, `test/` | Chỉ có README mẫu; chưa có thư viện nội bộ hoặc test |
| `platformio.ini` | Board, framework, flags, dependency |
| `.gitignore` | Loại build output, cấu hình IDE, credential cục bộ và log khỏi Git |
| `.agents/skills/docs/`, `AGENTS.md`, `docs/PROJECT_SPEC.md` | Skill đồng bộ, hướng dẫn agent và đặc tả dự án |

Luồng phụ thuộc: `main → WifiService, DeviceRegistry, ApiServer, IrDiagnostics`; `ApiServer → WifiService, DeviceRegistry, CommandApi`; `CommandApi → DeviceRegistry, ArduinoJson`; `IrDiagnostics → GPIO/RMT`. Không có đường gọi `CommandApi → IrDiagnostics`.

Nguồn: `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`, `platformio.ini`, `.gitignore`, `include/README`, `lib/README`, `test/README`, `tools/monitor-hub.ps1`, `.agents/skills/docs/SKILL.md`, `AGENTS.md`.

## 4. Luồng khởi động & runtime

1. `setup()` mở Serial 115200, `delay(1000)`, in tên hub và firmware `0.2.0-dev`.
2. Gọi lần lượt `WifiService::begin()`, `DeviceRegistry::begin()`, `ApiServer::begin()`, `IrDiagnostics::begin()`.
3. `loop()` chạy `WifiService::loop()` trước `ApiServer::loop()`.
4. Biến static `testSent` bảo đảm `IrDiagnostics::testToshibaFix()` tự chạy một lần khi uptime đạt ít nhất 10000 ms, không phụ thuộc Wi-Fi.
5. Mỗi vòng đọc tối đa một ký tự Serial; `f`/`F` gọi lại `testToshibaFix()`, ký tự khác không có nhánh xử lý.
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
- `begin()` đăng ký route GET, gọi `CommandApi::registerRoutes(server)`, đăng ký `onNotFound`; chưa mở socket.
- `loop()` dừng `WebServer` khi mất mạng; khi có mạng thì mở server nếu chưa chạy, sau đó gọi `handleClient()`.
- Phụ thuộc: `WebServer`, Arduino WiFi, `WifiService`, `DeviceRegistry`, `CommandApi`; state nội bộ `server(80)`, `serverRunning`. Không có task riêng.
- Contract chi tiết ở §6.2; phiên bản firmware của health được ghi cố định trong response.

Nguồn: `include/ApiServer.h`, `src/ApiServer.cpp`.

### 5.3 Command gateway (`CommandApi`)

- Mục đích: xác thực và kiểm tra schema lệnh trước khi thực thi phần cứng.
- API public: `void CommandApi::registerRoutes(WebServer& server);`.
- Thu thập header `X-API-Key`, `Content-Type`; lambda của route giữ con trỏ đến server do `ApiServer` sở hữu.
- Helper nội bộ `authenticate()`, `sendError()`, `handleCommand()` nằm trong anonymous namespace. Dùng ArduinoJson để parse; registry để kiểm tra ID thiết bị.
- Hiện chưa dispatch tới driver: mọi request vượt qua validation đều kết thúc bằng `503 device_not_ready`; không cập nhật state hoặc tuyên bố thành công.
- Chạy đồng bộ trong xử lý HTTP, không tạo task hoặc queue. Thứ tự validation và mã lỗi ở §6.2.

Nguồn: `include/CommandApi.h`, `src/CommandApi.cpp`, `src/ApiServer.cpp`.

### 5.4 Danh mục thiết bị (`DeviceRegistry`)

- Mục đích: cung cấp catalog cố định được biên dịch vào firmware.
- API public: `void DeviceRegistry::begin();`, `String DeviceRegistry::listJson();`, `String DeviceRegistry::deviceJson(const String& id);`.
- `begin()` chỉ log số lượng thiết bị. JSON được ghép bằng Arduino `String`; `reserve(768)` cho danh sách, `reserve(256)` cho chi tiết.
- Response có `schema_version:1`; danh sách bọc trong `devices`, chi tiết bọc trong `device`. ID không tồn tại trả chuỗi rỗng để caller xử lý.
- Mỗi thiết bị có `id`, `name`, `type`, `transport`, `availability`; tất cả `availability` là `not_configured`.

| ID logic cục bộ | Name | Type | Transport |
|---|---|---|---|
| `daikin_ac` | Daikin AC | `ir_ac` | `ir` |
| `toshiba_ac` | Toshiba AC | `ir_ac` | `ir` |
| `fan_ir` | IR Fan | `ir_fan` | `ir` |
| `rangdong_plug_01` | Rang Dong Plug | `tuya_plug` | `tuya_cloud` |

Các ID trên là tên logic của catalog, không phải Tuya device ID/credential. Module chỉ phụ thuộc Arduino, không có mapping sang driver, trạng thái AC, task riêng hoặc lưu bền.

Nguồn: `include/DeviceRegistry.h`, `src/DeviceRegistry.cpp`.

### 5.5 IR diagnostics (`IrDiagnostics`)

- Mục đích: kiểm tra đầu phát IR bằng GPIO và RMT TX trực tiếp; không dùng IRremoteESP8266.
- API public: `void IrDiagnostics::begin();`, `void IrDiagnostics::testBurst();`, `void IrDiagnostics::testDc();`, `void IrDiagnostics::testFanHigh();`, `void IrDiagnostics::testToshibaFix();`.
- Phụ thuộc: Arduino GPIO, ESP-IDF RMT TX/copy encoder và `esp_err_to_name()`.
- Cấu hình RMT: GPIO4, clock mặc định, resolution 1 MHz (1 tick = 1 µs), `mem_block_symbols=64`, `trans_queue_depth=4`; carrier 38000 Hz, duty 0.33, active-high, `always_on=false`.
- `begin()` tạo channel, áp carrier, tạo copy encoder, enable channel; chỉ đặt `initialized` khi hoàn tất. Lỗi từng bước được log `[IR]` rồi return. Các hàm phát RMT kiểm tra cờ này.

| Hàm | Dữ liệu / hành vi |
|---|---|
| `testBurst()` | Một symbol MARK 10 ms + SPACE 10 ms, gửi 30 lần; mỗi lần chờ tối đa 1000 ms và delay 80 ms; tổng thời gian bình thường khoảng 3 giây |
| `testDc()` | GPIO LOW 3 giây → HIGH liên tục 15 giây → LOW; không kiểm tra `initialized`, không tạo carrier |
| `testFanHigh()` | Mảng raw 72 thời lượng MARK/SPACE µs, chuyển thành 36 symbol static; một lần phát `POWER_HIGH`; comment nhắc nguồn Android ngoài repo, chưa định danh protocol |
| `testToshibaFix()` | Frame cố định 7 byte / 56 bit, LSB first; 58 symbol local gồm header, data, stop; gửi một lần |

Khung Toshiba dùng header MARK/SPACE 4350/4300 µs; mỗi bit MARK 550 µs, SPACE 1600 µs cho 1 hoặc 550 µs cho 0; stop 550/550 µs. Không suy ra chức năng power/mode/nhiệt độ từ tên bài thử. Mỗi lần truyền RMT dùng `loop_count=0`, mức kết thúc LOW và chờ hoàn tất tối đa 1000 ms.

Trong các hàm thử, chỉ `testToshibaFix()` được gọi trong entrypoint hiện tại. Các hàm thử chạy trong luồng gọi và chặn luồng trong thời gian chờ. Không có RX, decoder, học lệnh, gửi raw qua API, driver Daikin hoặc profile AC tổng quát. Dữ liệu IR là mảng C++ trong firmware; không lưu NVS, file hay cloud.

Nguồn: `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp`, `src/main.cpp`.

## 6. Giao tiếp ngoài

### 6.1 Tuya cloud

Không áp dụng ở mức triển khai hiện tại: repo không có Tuya SDK/dependency, client cloud, credential Tuya, provisioning/activation, DP, reconnect cloud hay OTA. `tuya_cloud` chỉ là nhãn transport của một thiết bị mẫu; capability `tuya_control` và `cloud_access` đều `false`. Chưa có luồng cloud → IR → report trạng thái.

| DP id | Code | Kiểu | Quyền | Mô tả | Hàm xử lý |
|---|---|---|---|---|---|
| Không áp dụng | — | — | — | Chưa có DP trong repo | — |

Nguồn: `platformio.ini`, `src/DeviceRegistry.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`.

### 6.2 HTTP local

Server HTTP cổng 80 trả `application/json`; không cấu hình TLS. GET không kiểm tra API key; chỉ POST commands kiểm tra `X-API-Key`.

| Method / route | Response / hành vi |
|---|---|
| GET `/api/v1/health` | `200`: `status`, `device`, `firmware`, `uptime_sec`, `free_heap`; status `ok`, firmware `0.2.0-dev` |
| GET `/api/v1/system` | `200`: `chip`, `flash_mb`, `psram_mb`, `ip`, `rssi_dbm` lấy từ ESP/Wi-Fi |
| GET `/api/v1/devices` | `200`: `schema_version`, `devices` theo §5.4 |
| GET `/api/v1/devices/<id>` | `200`: `schema_version`, `device`; ID không tồn tại → `404 device_not_found` |
| GET `/api/v1/capabilities` | `200`: `schema_version:1`, object `capabilities` với `ir_send`, `tuya_control`, `cloud_access` đều `false` |
| POST `/api/v1/commands` | Xác thực + validation theo bảng dưới; chưa có response thành công |

Route chi tiết thiết bị được xử lý trong `onNotFound`; toàn bộ hậu tố sau `/api/v1/devices/` là ID. Sai method ở route đã biết trả `405 method_not_allowed` và `Allow: GET` hoặc `Allow: POST`; route khác trả `404 not_found`. JSON lỗi thống nhất dạng `{"error":{"code":"..."}}`.

Thứ tự xử lý POST commands:

| Bước | Điều kiện | HTTP / error khi không đạt |
|---|---|---|
| 1 | Header `X-API-Key` trùng `HUB_API_KEY`; so độ dài trước, rồi XOR toàn bộ byte khi cùng độ dài | `401 unauthorized` |
| 2 | `Content-Type` bằng `application/json` hoặc bắt đầu `application/json;` | `415 unsupported_media_type` |
| 3 | `clientContentLength()` và body đều không vượt `MAX_BODY_SIZE=512` byte | `413 payload_too_large` |
| 4 | Body không rỗng, parse được JSON object; `NestingLimit(3)` | `400 invalid_json` |
| 5 | `device_id`, `action` là string không rỗng; lần lượt tối đa 48 và 32 ký tự | `400 invalid_command` |
| 6 | Registry có ID này | `404 device_not_found` |
| 7 | Action là `power_on` hoặc `power_off` | `422 unsupported_action` |
| 8 | Chưa có driver nối với gateway | Luôn `503 device_not_ready` |

Field JSON dư không bị từ chối. Giới hạn body nằm trong handler ứng dụng. Không có nonce, timestamp chống replay hoặc rate limit trong code. Endpoint capabilities trả `ir_send=false` cố định; các bài thử IR trực tiếp vẫn tồn tại riêng.

Nguồn: `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`.

### 6.3 Wi-Fi, Serial và kết nối khác

Wi-Fi STA dùng hostname `khairc-hub`; auto-reconnect cùng retry 30 giây, HTTP dừng/mở theo trạng thái mạng (§5.1–§5.2). Không có AP/BLE provisioning, mDNS, MQTT, WebSocket, HTTP client cloud, Home Assistant integration hay web UI được track. Serial 115200 nhận `f`/`F` để phát Toshiba và xuất log chẩn đoán.

Nguồn: `src/WifiService.cpp`, `src/ApiServer.cpp`, `src/main.cpp`, `platformio.ini`.

## 7. Dữ liệu & cấu hình

- Partition: dùng tên `default_16MB.csv` từ framework trong `platformio.ini`; repo không có CSV riêng, nên offset/kích thước từng partition chưa được xác minh từ HEAD.
- NVS / filesystem: ứng dụng không khai báo namespace/key, không dùng Preferences, NVS API, EEPROM, SPIFFS, LittleFS hay FATFS. Điều này không xác định hành vi lưu nội bộ của framework Wi-Fi.
- Dữ liệu: catalog và mã IR cố định trong firmware; cờ Wi-Fi/server/RMT và `testSent` trong RAM. JSON API registry/capabilities dùng `schema_version=1`; chưa có định dạng lưu bền hoặc migration.
- Build flags: `BOARD_HAS_PSRAM`, `ARDUINO_USB_CDC_ON_BOOT=0`; cấu hình flash/PSRAM và dependency ở `platformio.ini`. Không có Kconfig, `sdkconfig.defaults`, file `.ino` hay cấu hình ESP-IDF độc lập được track.
- Credential cục bộ: `WIFI_SSID`, `WIFI_PASSWORD` do `src/WifiService.cpp` sử dụng; `HUB_API_KEY` do `src/CommandApi.cpp` sử dụng. Các file này include `secrets.h`; đường dẫn cục bộ `include/secrets.h` bị `.gitignore` loại khỏi Git và **không thuộc HEAD**. Không có template credential được track; spec không ghi giá trị.
- `static_assert(sizeof(HUB_API_KEY) == 65)` yêu cầu biểu diễn khóa tương ứng chuỗi 64 ký tự kèm ký tự kết thúc; code không kiểm tra bảng ký tự hex/base64.
- Monitor ghi thư mục đầu ra `logs/` (bị Git bỏ qua), tên file theo mẫu `hub-monitor-<timestamp>.csv`; đây là đầu ra trên máy chạy PowerShell, không phải storage của ESP32.

Nguồn: `platformio.ini`, `.gitignore`, `src/WifiService.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/main.cpp`, `tools/monitor-hub.ps1`.

## 8. Quy ước & pattern của codebase

- Mỗi module có header trong `include/` và implementation cùng tên trong `src/`; namespace trùng tên module, API nhỏ và rõ. Header dùng `#pragma once`.
- State/helper riêng nằm trong anonymous namespace; constants dùng `constexpr`, trạng thái runtime dùng biến module hoặc static local.
- Service theo cặp `begin()`/`loop()` khi cần được phục vụ liên tục; `main` quyết định thứ tự khởi tạo và polling. Registry và diagnostics không có `loop()`.
- Log qua `Serial`, tag `[WiFi]`, `[API]`, `[CommandAPI]`, `[IR]`, `[IR TEST]` và tag registry; lỗi RMT đổi sang tên bằng `esp_err_to_name()` rồi return.
- HTTP dùng lambda `server.on()`, lỗi JSON thống nhất; command xác thực trước validation nội dung và không báo thành công khi chưa có driver.
- Response thường ghép `String`; ArduinoJson chỉ parse lệnh. Registry reserve dung lượng trước khi ghép, các khung IR dùng mảng fixed-size/static hoặc local, không có allocator riêng.
- Firmware version là chuỗi cố định ở startup và health, chưa có nguồn version dùng chung. Catalog có schema version độc lập với firmware version.
- `AGENTS.md` yêu cầu đọc spec trước khi sửa tính năng và chạy `$docs` sau khi commit. Skill đồng bộ chỉ ghi spec/khối marker, lấy trạng thái đã commit, bỏ qua secret; metadata skill đặt `allow_implicit_invocation: false`.

Nguồn: `include/ApiServer.h`, `include/CommandApi.h`, `include/DeviceRegistry.h`, `include/IrDiagnostics.h`, `include/WifiService.h`, `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`, `AGENTS.md`, `.agents/skills/docs/SKILL.md`, `.agents/skills/docs/agents/openai.yaml`.

## 9. Cách thêm tính năng mới

Các checklist dưới đây là điểm sửa rút ra từ cấu trúc hiện tại, không mô tả tính năng đã có:

- **Thêm thiết bị/giao thức IR:** khai báo API tại `include/IrDiagnostics.h` và phần thực thi tại `src/IrDiagnostics.cpp` nếu vẫn là bài thử; đăng ký thiết bị tại `src/DeviceRegistry.cpp`; nếu cần điều khiển HTTP, nối driver/validation trong `src/CommandApi.cpp`, cập nhật capability trong `src/ApiServer.cpp`; kiểm tra call site tự phát và Serial ở `src/main.cpp`. Code chưa có registry handler động hay driver AC dùng chung.
- **Thêm DP Tuya:** chưa có SDK, transport, bảng DP hay handler để mở rộng trực tiếp. Cần xác định tích hợp mới trước; các điểm nối hiện có là dependency trong `platformio.ini`, catalog trong `src/DeviceRegistry.cpp`, lệnh trong `src/CommandApi.cpp`, capability trong `src/ApiServer.cpp` và lifecycle tại `src/main.cpp`. Không suy ra DP từ nhãn `tuya_plug`.
- **Thêm chân/ngoại vi:** đối chiếu board và `platformio.ini`, chọn vị trí cấu hình tương tự `IR_PIN` trong `src/IrDiagnostics.cpp`, cập nhật header/lifecycle cần thiết và pin map §1; kiểm tra xung đột phần cứng trước thử nghiệm.
- **Thêm Kconfig/NVS key:** hiện chưa có cơ chế này. Cấu hình build nằm ở `platformio.ini`; credential theo include cục bộ được `.gitignore` bảo vệ. Nếu bổ sung storage, cần định nghĩa namespace/key, phiên bản và migration trước khi ghi chúng vào §7.
- **Thêm route/action:** sửa module API liên quan và `include/CommandApi.h` nếu đổi public API; duy trì auth, body limit, mã lỗi và xử lý sai method; đồng bộ registry/capabilities. Có thể cập nhật `tools/monitor-hub.ps1` khi metric health/system đổi.

Sau thay đổi, kiểm tra bằng toolchain/hardware phù hợp (§2); repo chưa có test tự động để dựa vào. Sau khi commit, chạy `$docs` để đồng bộ spec với HEAD mới.

Nguồn: `platformio.ini`, `.gitignore`, `src/main.cpp`, `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp`, `src/DeviceRegistry.cpp`, `include/CommandApi.h`, `src/CommandApi.cpp`, `src/ApiServer.cpp`, `tools/monitor-hub.ps1`, `test/README`.

## 10. Vấn đề đã biết, TODO, cần xác nhận

- [ ] Xác nhận board/module thực tế, flash/PSRAM và mạch phát IR (driver transistor, cực tính, điện trở); repo chỉ có cấu hình board và GPIO4, không có schematic. — Nguồn: `platformio.ini`, `src/IrDiagnostics.cpp`.
- [ ] Xác nhận phiên bản PlatformIO platform/Arduino core/ESP-IDF đang dùng và khả năng build với API `driver/rmt_tx.h`; platform không ghim phiên bản, repo không có CI/test thực thi. Lần đồng bộ này không build. — Nguồn: `platformio.ini`, `src/IrDiagnostics.cpp`, `test/README`.
- [ ] Checkout HEAD chưa đủ credential để build độc lập: cần cấu hình cục bộ cho `WIFI_SSID`, `WIFI_PASSWORD`, `HUB_API_KEY`; không có template được track. — Nguồn: `.gitignore`, `src/WifiService.cpp`, `src/CommandApi.cpp`.
- [ ] Xác nhận có muốn tự phát Toshiba sau mỗi boot hay không. `testSent` được đặt trước lần gọi nên không tự thử lại nếu init/truyền lỗi; startup vẫn log “ready” sau `begin()` dù init có thể return do lỗi. — Nguồn: `src/main.cpp`, `src/IrDiagnostics.cpp`.
- [ ] Xác nhận ý nghĩa và tương thích thiết bị thật của khung Toshiba FIX và raw quạt `POWER_HIGH`; comment dẫn nguồn Android ngoài repo, chưa có dữ liệu học/thu hay test phần cứng trong Git. Không coi tên thiết bị trong catalog là bằng chứng driver hoàn chỉnh. — Nguồn: `src/IrDiagnostics.cpp`, `src/DeviceRegistry.cpp`.
- [ ] Điều khiển API/cloud chưa hoàn thành: mọi thiết bị `not_configured`, lệnh hợp lệ trả `503`; chưa có triển khai Daikin/Tuya, report trạng thái hoặc OTA. Các diagnostics chưa nối vào command gateway. — Nguồn: `src/DeviceRegistry.cpp`, `src/CommandApi.cpp`, `src/ApiServer.cpp`, `src/main.cpp`.

Không tìm thấy marker `TODO`, `FIXME`, `HACK`, `XXX` trong mã ứng dụng, cấu hình build, script và các README mẫu. Không có README dự án để đối chiếu mâu thuẫn; ba README hiện có là tài liệu mẫu PlatformIO.

Nguồn: `platformio.ini`, `src/main.cpp`, `src/IrDiagnostics.cpp`, `src/DeviceRegistry.cpp`, `src/CommandApi.cpp`, `src/ApiServer.cpp`, `src/WifiService.cpp`, `.gitignore`, `include/README`, `lib/README`, `test/README`.

## 11. Nhật ký đồng bộ

- 2026-11-02 FULL @19f655c — tạo đặc tả, kiểm kê đủ 22 file tracked; bắt đầu tại a7240ce, cập nhật thêm tài liệu/skill từ hai commit phát sinh trong lúc quét, firmware không đổi. Metadata IDE chỉ kiểm kê; không đưa cấu hình cục bộ hay thay đổi chưa commit vào đặc tả.

Nguồn: cây Git và lịch sử commit tại mốc đồng bộ trong front matter.

## 12. Bản đồ file → mục

Mỗi file trong 22 file tracked tại mốc đồng bộ thuộc một dòng dưới đây. Năm module chức năng là §5.1–§5.5; thay đổi phần dùng chung cần xét thêm các mục liên quan.

| Glob | Mục | Ghi chú |
|---|---|---|
| `src/main.cpp` | §0, §4, §6.3, §8–§10 | Entry Arduino, trigger diagnostics |
| `include/WifiService.h`, `src/WifiService.cpp` | §4, §5.1, §6.3, §7–§10 | Module Wi-Fi |
| `include/ApiServer.h`, `src/ApiServer.cpp` | §4, §5.2, §6, §8–§10 | Module HTTP server |
| `include/CommandApi.h`, `src/CommandApi.cpp` | §4, §5.3, §6.2, §7–§10 | Module command gateway |
| `include/DeviceRegistry.h`, `src/DeviceRegistry.cpp` | §5.4, §6, §7–§10 | Module catalog |
| `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp` | §1, §2, §4, §5.5, §7–§10 | Module IR |
| `platformio.ini` | §1, §2, §3, §7, §9, §10 | Build, dependency, board |
| `.gitignore` | §2, §3, §7, §9, §10 | File sinh ra, credential và log |
| `include/README`, `lib/README`, `test/README` | §2, §3, §10 | README mẫu; chưa có test thực thi |
| `tools/**` | §2, §3, §7, §9 | Monitor HTTP trên máy phát triển |
| `.vscode/**` | §3 | Metadata IDE; bỏ qua trong bộ lọc đồng bộ |
| `.agents/skills/docs/**` | §3, §8, §9 | Skill và metadata điều khiển đồng bộ đặc tả |
| `docs/PROJECT_SPEC.md` | §0–§12 | Spec tự quản lý; bỏ qua diff đầu vào |
| `AGENTS.md` | §8, §9 | Khối hướng dẫn đọc/đồng bộ spec |

Các tên `include/secrets.h`, `logs/`, mẫu CSV log và `default_16MB.csv` ở §2/§7 là cấu hình cục bộ/đầu ra runtime/tài nguyên framework, không phải đường dẫn nguồn được track. Không có thư mục `docs/spec/` hoặc spec module tách riêng tại lần quét này.

Nguồn: danh sách `git ls-files` tại mốc đồng bộ; `platformio.ini`, `.gitignore`.
