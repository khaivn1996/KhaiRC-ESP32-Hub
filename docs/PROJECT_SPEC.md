---
spec_version: 1
project: KhaiRC-ESP32-Hub
last_synced_commit: cfd0ae9bbe9965700d1a68e6077874cbf6829ca6
last_synced_at: 2026-11-26
last_full_scan_commit: f065965e4ecdd88c9f21fc338a0fece519e238b1
---

# KhaiRC-ESP32-Hub — Đặc tả dự án

> File này do skill `$docs` duy trì. Đọc file này trước khi làm tính năng mới.
> Phạm vi: mã nguồn được commit tại HEAD; không lấy cấu hình cục bộ hay thay đổi chưa commit.

## 0. Tóm tắt nhanh

- Firmware Arduino trên ESP32-S3, build bằng PlatformIO; tên môi trường `esp32-s3-n16r8`. Cấu hình flash 16 MB QIO, PSRAM 8 MB OPI.
- Firmware tự khai báo phiên bản `0.4.0-ota1`; Wi-Fi STA, ba server: REST HTTP cổng 80, OTA HTTP cổng 8081 (LAN), HTTPS cổng 443 (chỉ bật khi có credential TLS cục bộ).
- Registry có năm thiết bị: quạt TAIZ và Toshiba báo `ready`/`not_ready` theo driver IR, Milife (miIO LAN) báo `configured`/`not_configured`, còn Daikin và mục Tuya cloud `rangdong_plug_01` báo `not_configured`.
- HTTP/80 với `X-API-Key`: quạt HIGH/SWING/OFF, Toshiba FIX và đọc trạng thái Milife. HTTPS/443 hỗ trợ lệnh Toshiba `state` (power/nhiệt độ/quạt), `fix`, `swing_on`, `swing_off` qua API key hoặc Web UI. Web UI dùng session cookie + CSRF và có thêm bật/tắt Ổ cắm 2 Rạng Đông OC09 qua Tuya LAN 3.4 (§5.14, §6.5).
- IR phát qua RMT trên GPIO4, có khóa `TxGate` vì cả Arduino loop và task HTTPS có thể phát. `status: sent`, `state_verified: false` chỉ xác nhận phát xong.
- `OtaSafety` quản lý rollback ảnh `PENDING_VERIFY` (health gate 15 s, hạn 90 s) và ghi nhật ký boot vào NVS; OTA bị khóa nếu không xác minh được rollback.
- Đăng nhập thủ công trên `/ui` dùng mật khẩu Web UI riêng. Web Vault tùy chọn đăng nhập Google, đọc Firestore và giải mã trong trình duyệt để đăng nhập Hub bằng API key (§5.10, §5.12).
- Không phát IR tự động khi khởi động; Serial `s`/`S` phát TAIZ SWING, `f`/`F` phát Toshiba FIX. Chưa có Tuya cloud, học/thu IR, lưu trạng thái thiết bị.

Nguồn: `platformio.ini`, `src/main.cpp`, `src/ApiServer.cpp`, `src/HttpsService.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/OtaSafety.cpp`, `src/WebUi.cpp`, `src/TuyaLocal.cpp`.

## 1. Phần cứng & pin map

PlatformIO chọn board `esp32-s3-devkitc-1`; tên environment là `esp32-s3-n16r8`. Cấu hình: `board_build.flash_mode=qio`, `board_upload.flash_size=16MB`, `board_build.arduino.memory_type=qio_opi`, `board_build.psram_type=opi`, `BOARD_HAS_PSRAM`. AGENTS xác định N16R8 và mạch FR4: 3V3 → R 33Ω → anode TSAL6200 (940 nm), cathode → collector 2N2222A; GPIO4 → R 390Ω → base, emitter → GND. HIGH làm LED sáng; không tự đổi chân/logic. Chưa có bản vẽ sơ đồ riêng.

| Chức năng | GPIO | Chiều | Ghi chú |
|---|---|---|---|
| IR TX / `IR_PIN` | 4 | Output | GPIO trực tiếp khi thử DC; RMT TX khi phát IR; nghỉ LOW, mark HIGH |
| Serial qua USB-UART | Không gán trong code | Vào/ra | 115200 baud; cấu hình ghi chú CH343, `ARDUINO_USB_CDC_ON_BOOT=0`; AGENTS dành GPIO43/44 cho UART0 |

Chỉ GPIO4 được gán tường minh trong ứng dụng. Không có RX IR, I2C, SPI, LEDC hay ngoại vi khác. Quy định dự án cấm gán ngoại vi vào GPIO26–37 (flash/PSRAM nội bộ), GPIO43/44 (UART0), GPIO0/3/45/46 (strapping) và GPIO19/20 (USB D-/D+). Không thấy ứng dụng gán các chân này; điều đó không thay thế kiểm tra sơ đồ board.

Nguồn: `platformio.ini`, `src/IrDiagnostics.cpp`, `src/main.cpp`, `AGENTS.md`.

## 2. Toolchain, build, flash, test

| Thành phần | Cấu hình/phiên bản xác minh được |
|---|---|
| PlatformIO environment | `esp32-s3-n16r8`, cũng là `default_envs` |
| Platform / framework | `espressif32 @ 8.0.0` / `arduino`; comment cấu hình ghi Arduino 3.3.12; `docs/HTTPS_STAGE1.md` ghi ESP-IDF 5.5.5 (không khai báo trực tiếp trong `platformio.ini`) |
| Dependency khai báo | `bblanchon/ArduinoJson @ 7.4.3` |
| Thư viện từ framework | `WiFi.h`, `WiFiUdp.h`, `WebServer.h`, `Preferences.h`, `esp_https_server.h`/`esp_http_server.h`, `esp_ota_ops.h`, `mbedtls/aes.h`, `mbedtls/md5.h`, RMT `driver/rmt_tx.h`, `driver/rmt_encoder.h` |
| Extra script | `extra_scripts = post:tools/fix-intellisense.py` — chỉ khi dump cấu hình IDE, loại hai include path Arduino 3.3.12 liệt kê nhưng không tồn tại; không đổi build firmware |
| Flash / monitor | `upload_speed=115200`, `monitor_speed=115200`; `upload_port`/`monitor_port` chỉ là comment |
| Partition | `board_build.partitions=default_16MB.csv` (của framework, không nằm trong repo) |

Build trên PowerShell: `pio run *> .pio/build.log; Get-Content .pio/build.log -Tail 25`; lỗi thì `Select-String .pio/build.log -Pattern 'error' | Select-Object -First 15`, không in cả log. Không có `pio` trong PATH thì gọi `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"`. Nạp `pio run -t upload` chỉ khi người dùng yêu cầu; `pio device monitor -b 115200` chặn vô hạn, nhờ người dùng dán log. Cùng một lỗi sửa quá 2 lần thì dừng và báo cáo. Task chỉ đổi Markdown/YAML cấu hình agent không chạy `pio run`, kiểm chứng bằng `git diff --stat`, quét ký tự ẩn và số dòng rào code chẵn (`AGENTS.md` §4). Cần credential cục bộ (§7).

Test OTA host: `python tools/test-ota.py --cxx <g++>` chạy 33 kịch bản (gồm `safety_disabled`) trên `src/OtaService.cpp` thật với stub HTTP/flash và stub `test/ota_host/OtaSafety.h` (`safeEnabled`); `g++` ở `$env:USERPROFILE\.platformio\packages\toolchain-gccmingw32\bin`. Test trang: `python tools/test-ota-page.py --browser <chrome-hoac-msedge>` — 17 kịch bản × desktop/mobile = 34 lượt Playwright với mạng giả lập; tùy chọn `--screenshots <thu-muc>`. Chưa có test host cho handler C++ HTTPS/Web UI, MiotLocal, `ToshibaCodec` hay `OtaSafetyPolicy`. Chưa có CI. Lần sync không chạy build/test.

Kiểm thử bổ sung (không thay thế thử trên board):

- `python tools/test-wifi.py --cxx <g++>`: module thật, credential giả và driver bận mô phỏng; kiểm tra hủy trước reconnect, nhịp retry, online, lỗi và tràn millis.
- `python tools/test-web-login.py [--browser <chrome-hoac-msedge>] [--openssl <openssl>]` dùng Playwright, mặc định WebKit với profile iPhone 13 và HTTPS localhost qua OpenSSL. Backend mô phỏng kiểm tra Origin, mật khẩu sai, đăng nhập/303, cookie, reload, logout và không phát IR. Có ca mật khẩu 7 byte.
- `python tools/test-vault-web.py`: chạy Node codec/crypto độc lập; thiếu `web/vault/test/kat-app.json` thì skip KAT app.

Tiện ích khác:
- `tools/monitor-hub.ps1 -HubIP "<IP-hub>" [-DurationMinutes 10] [-IntervalSeconds 60]`: GET health rồi system qua HTTP/80, ghi CSV vào `logs/` (cột `Time`, `Status`, `UptimeSec`, `FreeHeapBytes`, `RSSIdBm`, `IPAddress`, `Error`; trạng thái `OK`/`ERROR`/`RESET` suy từ uptime giảm). Không gửi API key.
- `tools/spec.py`: in mục lục spec; `python tools/spec.py 0 8 9 5.5` đọc chọn lọc; thiếu spec trả mã 1, không thấy mục trả mã 2.
- `tools/create_https_credentials.py`: đọc `include/tls_certificate.pem` + `include/tls_private_key.pem` (người dùng tự tạo bằng OpenSSL theo `docs/HTTPS_STAGE1.md`), sinh `include/tls_credentials.h` chứa `HUB_TLS_CERT_PEM`, `HUB_TLS_KEY_PEM`; từ chối ghi đè, không in khóa. Cả ba file bị Git bỏ qua. `--fingerprint-only` chỉ đọc cert công khai, in SHA-256 DER (64 hex in hoa, không dấu `:`), không ghi file; khi sinh header cũng in fingerprint.

Bundle vault: `python tools/build-vault-web.py` chạy npm ci/esbuild, ghim Firebase `12.14.0`, esbuild `0.28.2`; firmware dùng bundle đã commit nên build thường không cần Node (§5.12).

Nguồn: `platformio.ini`, `web/vault/package.json`, `tools/*.py`, `tools/monitor-hub.ps1`, `test/ota_host/**`, `docs/OTA.md`, `docs/HTTPS_STAGE1.md`, `AGENTS.md`.

## 3. Cấu trúc thư mục & kiến trúc

HEAD có 96 file tracked gồm `src` (16), `include` (23), `test` (7), `tools` (10), `docs` (5), `web` (6), `.agents` (10), `.claude` (10), `backups` (3), `lib` (1), `.vscode` (1) và 4 file gốc.

| Vị trí | Vai trò |
|---|---|
| `src/main.cpp` | Điểm vào Arduino, thứ tự khởi tạo/polling, phím thử IR thủ công |
| `src/`, `include/` | Module gồm Wi-Fi, ApiServer, ApiAuth, CommandApi, DeviceRegistry, IrDiagnostics (+`ToshibaCodec`), OtaService, OtaPage, OtaSafety (+`OtaSafetyPolicy`), HttpsService, WebUi, WebUiPage, MiotLocal, TuyaLocal và VaultWebBundle |
| `tools/` | Monitor HTTP, đọc spec, test OTA/Wi-Fi/login/vault, build bundle, sinh header TLS, sửa IntelliSense |
| `web/vault/` | Source JS, dependency/lockfile và test Node; bundle sinh trong `include/VaultWebBundle.h`, `src/VaultWebBundle.cpp` |
| `docs/WEB_VAULT.md` | Cấu hình Firebase, crypto/codec, KAT và thử board |
| `test/ota_host/` | Test host OTA và stub `Arduino.h`, `WebServer.h`, `esp_ota_ops.h`, `esp_app_format.h`, `OtaSafety.h` |
| `docs/` | `PROJECT_SPEC.md`, `OTA.md`, `HTTPS_STAGE1.md`, `CODEX_WORKFLOW.md`, `WEB_VAULT.md` |
| `.agents/skills/**`, `.claude/skills/**`, `AGENTS.md`, `CLAUDE.md` | Năm skill phân vai và chỉ thị agent cho Codex/Claude CLI |

Luồng phụ thuộc:
- `main → OtaSafety, WifiService, DeviceRegistry, ApiServer, HttpsService, IrDiagnostics`.
- `ApiServer → WifiService, DeviceRegistry, CommandApi, MiotLocal, IrDiagnostics, OtaService, OtaSafety`; `OtaService → OtaPage, OtaSafety, esp_ota_*`.
- `HttpsService → ApiAuth, ApiServer, WifiService, DeviceRegistry, IrDiagnostics, MiotLocal, OtaService, OtaSafety, WebUi`. `WebUi → ApiAuth, WebUiPage, VaultWebBundle, IrDiagnostics, OtaService, ToshibaCodec, TuyaLocal`. `TuyaLocal` dùng `WifiService`, `WiFiClient`, mbedTLS và ArduinoJson. Browser vault dùng Firebase Auth, Firestore REST và WebCrypto trước khi POST `/web/login`.
- `CommandApi → ApiAuth, DeviceRegistry, IrDiagnostics`. `DeviceRegistry → IrDiagnostics, MiotLocal`. `MiotLocal → ApiAuth`. `IrDiagnostics → ToshibaCodec, RMT`.
- `OtaSafety → OtaSafetyPolicy, ApiServer::ready(), HttpsService::ready(), IrDiagnostics::isReady(), Preferences`; `WifiService` gửi lý do disconnect cho `OtaSafety`.

Nguồn: `git ls-files`, `src/*.cpp`, `include/*.h`, `.gitignore`.

## 4. Luồng khởi động & runtime

1. `setup()` mở Serial 115200, `delay(1000)` rồi in banner theo `ApiServer::FIRMWARE_VERSION` (`0.4.0-ota1`).
2. Thứ tự: `OtaSafety::begin()` → `WifiService::begin()` → `OtaSafety::wifiBegin()` → `DeviceRegistry::begin()` → `ApiServer::begin()` → `HttpsService::begin()` → `IrDiagnostics::begin()`. Không phát IR khi boot.
3. `loop()`: `WifiService::loop()` → `ApiServer::loop()` (OTA loop, mở/dừng server 80 + 8081, `handleClient()` cả hai) → `HttpsService::loop()` (mở/dừng server TLS) → `OtaSafety::loop()` (stage NVS, health gate, đánh dấu VALID).
4. Đọc tối đa một ký tự Serial: `s`/`S` → `testFanSwing()`, `f`/`F` → `testToshibaFix()`; ký tự khác bỏ qua. Kết thúc `delay(5)`.

| Task/ngữ cảnh | Entry | Stack | Priority | Vai trò |
|---|---|---|---|---|
| Arduino loop (framework) | `setup()`/`loop()` | Không đặt trong repo | Không đặt | Wi-Fi, HTTP/80, OTA/8081, Serial, OtaSafety |
| HTTPS server (ESP-IDF httpd) | `httpd_ssl_start` | 8192 | Mặc định `HTTPD_SSL_CONFIG_DEFAULT` | Handler HTTPS/Web UI, có thể phát IR, đọc miIO và gửi lệnh Tuya LAN |
| `ota_guard` | `verifyDeadlineTask` | 4096 | 2 | Chỉ tạo khi ảnh đang `PENDING_VERIFY`; sau 90 s nếu vẫn pending thì rollback và reboot |

Phát IR và đọc miIO chạy đồng bộ trong luồng gọi: trong lúc RMT phát (≤1000 ms chờ) hoặc miIO chờ (≤1,2 s hello + 3,2 s đọc), luồng đó không phục vụ request khác. Đồng bộ chéo luồng: IR dùng `std::atomic_flag` `TxGate` (bận → lỗi ngay, không xếp hàng); `initialized` là `std::atomic<bool>`; OtaSafety dùng mutex `verifyMutex` giữa loop và `ota_guard`. Không có watchdog/ISR tùy chỉnh. `MiotLocal::readPower()` không có khóa riêng (§10).

Lệnh Tuya LAN chạy đồng bộ trong task HTTPS. Driver đặt timeout kết nối TCP 1,5 s, chờ phản hồi thương lượng 2 s và chờ phản hồi lệnh 3 s. Khi nhận ACK, hạn chờ báo trạng thái được rút xuống tối đa 1,2 s tính từ ACK, trong giới hạn 3 s ban đầu (§5.14).

Nguồn: `src/main.cpp`, `src/ApiServer.cpp`, `src/HttpsService.cpp`, `src/IrDiagnostics.cpp`, `src/OtaSafety.cpp`, `src/MiotLocal.cpp`, `src/TuyaLocal.cpp`.

## 5. Module

### 5.1 Wi-Fi (`WifiService`)

- API: `void WifiService::begin();`, `void WifiService::loop();`, `bool WifiService::connected();`.
- `begin()` đăng ký `WiFi.onEvent` log `[WiFi EVENT]` cho STA_START/CONNECTED/DISCONNECTED(reason)/GOT_IP; khi DISCONNECTED gọi `OtaSafety::wifiDisconnectReason(reason)`. Sau đó hostname `khairc-hub`, `WIFI_STA`, `setAutoReconnect(false)`, `WiFi.begin(WIFI_SSID, WIFI_PASSWORD)`; không chờ kết nối.
- `loop()` log online/offline, in IP khi vừa online; offline và đủ `RETRY_MS=30000` thì `WiFi.disconnect(false, false)` hủy handshake cũ, giữ credential AP, rồi `WiFi.reconnect()`. Disconnect lỗi thì bỏ reconnect; lỗi driver được log và chờ chu kỳ sau. Loop sở hữu retry; comment ghi Arduino 3.3.12 vẫn có thể tự thử một lần sau lỗi đầu tiên.
- Không có AP, provisioning, mDNS hay task riêng.

Nguồn: `include/WifiService.h`, `src/WifiService.cpp`.

### 5.2 HTTP server (`ApiServer`)

- API gồm `void begin();`, `void loop();`, `bool ready();` (`serverRunning && WifiService::connected()`), `String systemJson();`, `String capabilitiesJson();`. Hai helper JSON dùng chung cho HTTP/80 và HTTPS/443.
- Sở hữu hai `WebServer`: `server(80)` cho REST API và `otaServer(8081)` cho OTA (comment: router không được forward cổng này). Cả hai thu header `X-API-Key`, `Content-Type`, `X-OTA-Password`, `Content-Length`, `Transfer-Encoding`.
- `begin()` tạo `bootId = esp_random()`, đăng ký route GET trên 80 (§6.2), `/api/v1/health` trên cả 80 và 8081 (trang OTA cần health cùng origin), gọi `CommandApi::registerRoutes(server)`, `MiotLocal::registerRoutes(server)`, `OtaService::registerRoutes(otaServer)` và `onNotFound` của cổng 80.
- `loop()` gọi `OtaService::loop()` trước; đang chờ reboot thì return. Mất Wi-Fi thì `stop()` cả hai server; có Wi-Fi thì `begin()` cả hai nếu chưa chạy, rồi `handleClient()` lần lượt 80 và 8081.
- Health cổng 80 và 8081 dùng `ApiServer::FIRMWARE_VERSION` và trả `build_id`, `ota_slot`, `ota_state` từ `OtaSafety`.

Nguồn: `include/ApiServer.h`, `src/ApiServer.cpp`.

### 5.3 Command gateway (`CommandApi`)

- API: `void CommandApi::registerRoutes(WebServer& server);` — POST `/api/v1/commands` trên cổng 80.
- Raw callback nhận body theo từng chunk trước khi handler cuối chạy. Buffer static giữ tối đa 512 byte và chỉ dùng khi nhận `RAW_END`. `RAW_START`, `RAW_ABORTED` và khi handler kết thúc đều đặt lại trạng thái nhận.
- Handler gọi `ApiAuth::hubKeyMatches()` trước khi parse JSON. Thứ tự validation ở §6.2.
- Dispatch: `fan_ir` `power_on`→`sendFanHigh()`, `swing`→`sendFanSwing()`, `power_off`→`sendFanOff()`; `toshiba_ac` `fix`→`sendToshibaFix()`. `milife_plug_10a` luôn `422 unsupported_action` (chỉ đọc). Thứ tự validation ở §6.2.
- Mỗi request hợp lệ phát đúng một lần; `200 sent` chỉ sau RMT transmit/wait thành công. Mọi lỗi send (kể cả `TxGate` bận) trả `500 ir_send_failed`.

Nguồn: `include/CommandApi.h`, `src/CommandApi.cpp`.

### 5.4 Danh mục thiết bị (`DeviceRegistry`)

- API: `void begin();`, `String listJson();`, `String deviceJson(const String& id);` — ID không tồn tại trả chuỗi rỗng.
- JSON ghép bằng `String` (`reserve(768)` danh sách, `reserve(256)` chi tiết), `schema_version:1`, bọc trong `devices`/`device`. Mỗi thiết bị: `id`, `name`, `type`, `transport`, `availability`, `supported_actions`.

| ID | Name | Type | Transport | Availability | Supported actions |
|---|---|---|---|---|---|
| `daikin_ac` | Daikin AC | `ir_ac` | `ir` | `not_configured` | `[]` |
| `toshiba_ac` | Toshiba AC | `ir_ac` | `ir` | `ready`/`not_ready` | `fix` |
| `fan_ir` | IR Fan | `ir_fan` | `ir` | `ready`/`not_ready` | `power_on`, `swing`, `power_off` |
| `rangdong_plug_01` | Rang Dong Plug | `tuya_plug` | `tuya_cloud` | `not_configured` | `[]` |
| `milife_plug_10a` | Milife 10A | `smart_plug` | `miio_lan` | `configured`/`not_configured` | `[]` |

Availability phản ánh driver cục bộ (IR sẵn sàng; Milife có đủ IP/DID/token hợp lệ), không phải nguồn/kết nối của thiết bị. `supported_actions` của Toshiba chỉ liệt kê `fix` dù HTTPS còn nhận `state`/`swing_on`/`swing_off` (§10).

Ổ cắm OC09 dùng ID `rangdong_oc09_s2` riêng trong Web UI (§5.14). Registry chỉ liệt kê mục Tuya cloud `rangdong_plug_01`.

Nguồn: `include/DeviceRegistry.h`, `src/DeviceRegistry.cpp`, `src/WebUi.cpp`.

### 5.5 IR diagnostics (`IrDiagnostics`, `ToshibaCodec`)

- API send gồm `sendFanHigh()`, `sendFanSwing()`, `sendFanOff()`, `sendToshibaFix()`, `sendToshibaState(bool power, int temperatureC, uint8_t fanProtocolByte)` và `sendToshibaSwing(bool enabled)`, trả `esp_err_t`. API khác gồm `begin()`, `isReady()`, `testFanSwing()` và `testToshibaFix()`. Hai hàm thử gọi hàm send tương ứng và bỏ qua mã trả về.
- RMT: GPIO4, resolution 1 MHz, `mem_block_symbols=64`, `trans_queue_depth=4`; carrier 38 kHz, duty 0.33, active-high. `begin()` tạo channel → carrier → copy encoder → enable; chỉ đặt `initialized` khi hoàn tất.
- Đồng bộ: mọi hàm send giữ `TxGate` (`std::atomic_flag`) trước khi đổ buffer `static` và suốt lúc chờ; bận → `ESP_ERR_INVALID_STATE`, không xếp hàng/retry. Chưa sẵn sàng → `ESP_ERR_INVALID_STATE`.
- `transmitSymbols()`: `loop_count=0`, chờ tối đa 1000 ms; lỗi transmit/wait đặt `initialized=false`, thử `rmt_disable()` và trả lỗi gốc. Không tự phục hồi; cần khởi động lại.

| Hàm | Dữ liệu / hành vi |
|---|---|
| `sendFanHigh/Swing/Off()` | `transmitRawPattern()` đổi 72 thời lượng raw thành 36 symbol static. Mỗi lệnh TAIZ HIGH/SWING/OFF phát một lần |
| `sendToshibaFix()` | Frame 7 byte `4F B0 80 7F 84 00 84`, LSB first, 58 symbol, phát một lần qua `transmitToshibaFrames()` |
| `sendToshibaState()` | `ToshibaCodec::stateFrame` 9 byte; frame lặp 2 lần trong một lần transmit, khoảng cách 7350 µs |
| `sendToshibaSwing()` | `ToshibaCodec::swingFrame` 7 byte (`0x80` bật/`0x40` tắt + checksum), lặp 2 lần trong một transmit |

Timing Toshiba: header 4350/4300 µs, bit MARK 550, SPACE 1600 (1)/550 (0), stop 550. `transmitToshibaFrames<N>()` dùng buffer static tối đa 148 symbol. `ToshibaCodec` (header-only, comment ghi phản chiếu `ToshibaAcEncoder.kt` Android): `fanFromName()` ánh xạ `auto`, `1`–`5` sang byte giao thức cho HTTPS/Web UI. `stateFrame` nhận nhiệt độ 17–30 °C (offset đảo bit 4 bit), byte quạt hợp lệ `0x80`(auto), `0x82`, `0x86`, `0x81`, `0x85`, `0x83` (mức 1–5), power off thay byte quạt bằng `0xE0`; byte cuối là XOR checksum. Không có mode (chỉ cool ngầm định theo frame), RX, decoder hay học lệnh. Comment mã quạt dẫn nguồn Android ngoài repo, chưa đối chiếu.

Nguồn: `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp`, `include/ToshibaCodec.h`.

### 5.6 OTA (`OtaService`)

- API: `void registerRoutes(WebServer& server);` (được gắn vào `otaServer` 8081), `void loop();`, `bool available();`, `bool rebootPending();`.
- `available()` = mật khẩu hợp lệ + chưa chờ reboot + có vùng đích + `OtaSafety::canAcceptUpdate()`. GET `/api/v1/ota` và bắt đầu upload đều trả `503 ota_safety_not_ready` nếu safety chưa cho phép, trước mọi thao tác flash.
- Auth so độ dài rồi XOR toàn bộ byte. Kiểm tra auth, body size, prefix trước erase/write: magic ảnh, chip ESP32-S3, segment count 1–16, hash appended, app descriptor magic. Vùng đích là OTA app khác vùng đang chạy và đủ chỗ.
- Raw callbacks ghi từng chunk; đủ byte mới `esp_ota_end()` (checksum/SHA-256) rồi chọn boot partition. Lỗi/ngắt hủy handle; `loop()` dọn upload dở. Reboot sau 1500 ms; không retry upload.
- Route `/` của 8081 trả JSON `{"service":"KhaiRC-ESP32-Hub","status":"ok"}` (không còn redirect sang `/update`).

Nguồn: `include/OtaService.h`, `src/OtaService.cpp`, `test/ota_host/test_ota.cpp`.

### 5.7 Trang OTA (`OtaPage`)

- API: `const char* OtaPage::html();` — HTML/CSS/JS/SVG nhúng PROGMEM, không CDN; favicon SVG data URI.
- Giao diện tiếng Việt, kiểm tra `.bin` không rỗng ≤4 MiB và mật khẩu; GET `/api/v1/ota` (auth/giới hạn), GET health kiểm tra metadata nền (`boot_id`, `build_id` 16 hex thường, slot `app0`/`app1`); thiếu thì chặn trước POST, yêu cầu nạp nền qua USB. Đọc 8 byte hash tại offset 176–183 của file để lấy expected build; XHR POST `/update` một lần, timeout 150 s. Sau upload tối đa 60 GET health cách 2 s, mỗi GET timeout 5 s; chỉ thành công khi `status:ok`, boot ID đổi, build khớp, slot đổi giữa `app0`/`app1` và `ota_state:valid`. Pending/rollback/sai build hoặc slot không được báo thành công.
- Xóa ô mật khẩu sau lượt gửi, không cookie/localStorage; khóa gửi trùng khi busy. Dùng đường dẫn tương đối nên chạy được trên cổng 8081.

Nguồn: `include/OtaPage.h`, `src/OtaPage.cpp`, `tools/test-ota-page.py`.

### 5.8 An toàn OTA (`OtaSafety`, `OtaSafetyPolicy`)

- API: `void begin();` (gọi trước Wi-Fi), `void wifiBegin();`, `void loop();` (sau ApiServer/Https loop), `void wifiDisconnectReason(uint8_t)` (chỉ ghi atomic trong callback), `bool canAcceptUpdate();`, `const char* stateName();`, `slotName();`, `buildId();`.
- Ghi đè hook C yếu `extern "C" bool verifyRollbackLater()` trả `true` để Arduino không tự đánh dấu VALID trong `initArduino()`.
- `begin()`: đọc trạng thái OTA của partition đang chạy, `buildId` = 16 ký tự đầu ELF SHA-256 (đọc trực tiếp 8 byte đầu `esp_app_get_description()->app_elf_sha256` qua con trỏ volatile, tránh formatter SDK bị giới hạn `CONFIG_APP_RETRIEVE_LEN_ELF_SHA`), `slotName` = nhãn partition. Mở NVS namespace `ota_safe`; bản ghi hợp lệ cũ chép sang key `prev`, bản ghi mới ở key `now` (`seq+1`). Nếu `PENDING_VERIFY`: tạo `verifyMutex` và task `ota_guard`; thất bại thì `ESP.restart()` để bootloader rollback.
- Stage nhật ký tăng dần (chỉ ghi khi tăng): 1 BOOT, 2 Wi-Fi begin, 3 GOT_IP, 4 HTTP/OTA chạy, 5 TLS chạy, 6 đã xác nhận VALID. Lý do disconnect mới được ghi khi thay đổi.
- `loop()` khi pending: `HealthGate` yêu cầu Wi-Fi + IP + `ApiServer::ready()` + `HttpsService::ready()` + `IrDiagnostics::isReady()` liên tục `STABLE_MS=15000`, trước `DEADLINE_MS=90000` kể từ boot; thử một lần duy nhất `esp_ota_mark_app_valid_cancel_rollback()` dưới mutex. `ota_guard` sau 90 s nếu vẫn pending gọi `esp_ota_mark_app_invalid_rollback_and_reboot()`; thất bại thì chỉ log (có thể cần USB).
- `canAcceptUpdate()` chỉ đúng khi build có `CONFIG_APP_ROLLBACK_ENABLE`, hook đã được gọi, không pending, trạng thái `VALID`/`UNDEFINED` và có ≥2 app partition; ngược lại OTA bị khóa (fail closed).
- `OtaSafetyPolicy` header-only, so sánh thời gian an toàn khi `millis()` tràn.

Nguồn: `include/OtaSafety.h`, `include/OtaSafetyPolicy.h`, `src/OtaSafety.cpp`.

### 5.9 HTTPS (`HttpsService`)

- API: `void begin();`, `void loop();`, `bool ready();`.
- Chỉ biên dịch phần TLS khi có cả `<esp_https_server.h>` và `tls_credentials.h` cục bộ (`KHAIRC_HTTPS_ENABLED`); thiếu thì log disabled và `ready()` luôn false.
- `loop()`: mất Wi-Fi → `httpd_ssl_stop`, `WebUi::resetSessions()`; có Wi-Fi và chưa chạy → thử start, lần thử lại cách `RETRY_MS=30000`.
- Cấu hình: `port_secure=443`, stack 8192, `max_uri_handlers=17`, `max_req_hdr_len=1536`, `max_open_sockets=2`, `lru_purge_enable`. Cert/khóa từ `HUB_TLS_CERT_PEM`/`HUB_TLS_KEY_PEM`. Đăng ký lỗi bất kỳ route nào → dừng server (fail closed).
- Route API (§6.5) gồm health rút gọn (`phase:"https_probe"`), system, capabilities, devices, Milife state và POST commands dành cho Toshiba. HTTPS dùng helper JSON chung của `ApiServer`, helper phản hồi Milife của `MiotLocal` và xác thực qua `ApiAuth`. Server cũng phục vụ các route Web UI.

Nguồn: `include/HttpsService.h`, `src/HttpsService.cpp`, `tools/create_https_credentials.py`.

### 5.10 Web UI (`WebUi`, `WebUiPage`)

- API: `bool WebUi::registerRoutes(httpd_handle_t server);` (chỉ trên TLS), `void WebUi::resetSessions();`; `WebUiPage::html()`, `script()`, `manifest()`, `icon()`, `iconPng()`, `iconPngLen()`, `vaultHead()`, `vaultTail()`, `vaultMissing()` (PROGMEM, raw string `R"KHAIRCSTAGE3A(`).
- `/ui` gửi trường `password` trong form urlencoded tới `POST /web/login`. Server tính SHA-256 của dữ liệu nhập và `HUB_WEB_PASSWORD`, rồi so 32 byte digest qua `ApiAuth::constantTimeEquals()`. Trường có `autocomplete="current-password"` để trình duyệt có thể lưu nếu người dùng đồng ý. Mật khẩu thiếu/rỗng trong cấu hình làm nhánh này trả `503`.
- Cùng endpoint nhận trường `api_key` (64 byte ASCII in được) cho Web Vault và xác thực qua `ApiAuth::hubKeyMatches()`. Hai nhánh dùng chung bộ đếm: 5 lần sai → khóa 30 s (`429 try_later`). Thành công trả `303` về `/ui` và đặt cookie `khairc_sid` với `Secure`, `HttpOnly`, `SameSite=Strict`, `Path=/`, `Max-Age=900`. Có 2 slot xoay vòng, hết hạn 15 phút. GET `/web/session` cấp token CSRF riêng.
- POST `/web/command`/`/web/logout` cần session + header `X-CSRF-Token` khớp + `Origin` (nếu có) bằng `https://<Host>`. Command dùng Content-Type đúng `application/json`, body ≤512, không NUL, `NestingLimit(3)`. Lệnh Toshiba gồm `state` (đúng 4 field `action`, `power`, `temperature_c`, `fan_speed`) và `fix`/`swing_on`/`swing_off` (chỉ field `action`). Body có `device_id` khác null đi vào nhánh OC09 (§6.5).
- Header bảo mật: no-store, nosniff, `X-Frame-Options: DENY`, `Referrer-Policy: same-origin` để native login form giữ Origin cùng nguồn; trang `/ui` có CSP `script-src 'self'`, `form-action 'self'`, `frame-ancestors 'none'`.
- `/ui` liên kết tới `/ui/vault` (§5.12). Hai cách đăng nhập tạo cùng loại phiên.
- JS trang: nút bật/tắt/làm lạnh/nhiệt độ/quạt gửi `state`, nút FIX/SWING; trạng thái hiển thị là giả định phía client. Lỗi/không rõ → khóa "CHƯA RÕ", không tự gửi lại. PWA manifest `start_url:/ui`.
- Thẻ OC09 bật nút khi kiểm tra health thành công, có CSRF và đang rảnh. Mỗi lần bấm gửi một lệnh. UI hiển thị trạng thái thiết bị báo về, lệnh đã gửi nếu chỉ có ACK, hoặc “Chưa rõ” khi mất xác nhận. Thẻ hoạt động độc lập với khóa “CHƯA RÕ” của Toshiba và xóa trạng thái khi đăng xuất.

Nguồn: `include/WebUi.h`, `src/WebUi.cpp`, `include/WebUiPage.h`, `src/WebUiPage.cpp`.

### 5.11 Milife miIO LAN (`MiotLocal`)

- API: `bool configured();`, `PowerResult readPower();` (`Status` + `on`), `void registerRoutes(WebServer&)`; helper dùng chung `errorCode(Status)`, `isTimeout(Status)`, `stateJson(bool)`. Enum `Status`: `Ok`, `NotConfigured`, `WifiUnavailable`, `NetworkError`, `HelloTimeout`, `DeviceMismatch`, `ReadTimeout`, `InvalidResponse`, `AuthFailed`, `DeviceRejected`.
- Cấu hình tùy chọn từ `include/miot_config.h` cục bộ (`MILIFE_IP`, `MILIFE_DID`, `MILIFE_TOKEN` 32 hex); thiếu → mặc định rỗng/0 → `not_configured`.
- `readPower()`: UDP 54321, hello 32 byte (timeout 1200 ms, kiểm tra sender và DID), rồi một gói `get_properties` siid 2/piid 1 mã hóa AES-128-CBC (key/IV từ MD5 token), checksum MD5; chờ trả lời 3200 ms, xác minh MD5, giải mã, kiểm tra padding, `id`, `code`, `value` bool. Mỗi GET đúng một lần đọc, không ghi/toggle, không retry.
- Route HTTP/80 `GET /api/v1/devices/milife_plug_10a/state` (cần `X-API-Key`); HTTPS có route tương đương trong `HttpsService`.

Nguồn: `include/MiotLocal.h`, `src/MiotLocal.cpp`, `include/miot_config.example.h`.

### 5.12 Web Vault (`VaultWebBundle`, JS browser)

- API: `const std::uint8_t* VaultWebBundle::data();`, `std::size_t VaultWebBundle::size();`; mảng gzip PROGMEM sinh bằng `tools/build-vault-web.py`, không sửa tay. Tool tạo IIFE ES2020, gzip level 9, `mtime=0`, từ chối >120 KiB; commit source/bundle/lockfile cùng nhau.
- `/ui/vault` công khai, không cần session Hub; WebUi stream HTML + JSON cấu hình đã escape. `/web/vault.js` trả gzip. Thiếu cấu hình thì trang hướng dẫn đăng nhập thủ công, script trả `404 not_configured`.
- Firebase Google popup với `inMemoryPersistence`; yêu cầu secure context/WebCrypto và tab thường (standalone/PWA bị chặn). Browser REST GET duy nhất `users/{uid}/vault/main` trong Firestore `(default)` bằng Bearer idToken; firmware không truy cập cloud, mã web không ghi Firestore.
- `openVault(password, uid, doc)` nhận doc thường hoặc typed fields Firestore: `schemaVersion=1`, `kdfAlg=PBKDF2-SHA256`, iterations nguyên 1..2.000.000, salt 16 byte, IV 12 byte, ciphertext kèm tag 16..1.048.576 byte. Password UTF-8 → PBKDF2 → AES-256-GCM, tag 128 bit, AAD `khairc-vault-v1|uid`.
- `decodeVault(bytes)` đọc version 1, TUYA/HUB/HTTPS/WOL/LABELS, cờ 0/1, Java modified UTF-8; từ chối byte thừa/nhóm cụt/count LABELS ngoài 0..6/key trùng. Key Hub đúng 64 ASCII in được; thiếu HUB hoặc giải mã lỗi không POST login.
- POST trường `api_key` dạng urlencoded tới `/web/login` cùng origin; redirect thành công về `/ui` dùng cookie hiện có. Không tự retry vault/login. Sau lượt mở khóa xóa ô password, ghi đè plaintext, bỏ tham chiếu và Firebase sign-out. Chuỗi JS không bảo đảm xóa vật lý trước GC; mã ứng dụng không lưu key/password vào browser storage.
- Riêng vault đã cấu hình dùng `Referrer-Policy: strict-origin-when-cross-origin`, CSP cho `apis.google.com`, iframe auth domain, Identity Toolkit/Token Service/Firestore; no-store/nosniff/frame DENY. `/ui` giữ CSP cũ. Hành vi storage của popup bên ngoài và tương thích Android/Safari cần thử (§10).

Nguồn: `include/VaultWebBundle.h`, `src/VaultWebBundle.cpp`, `src/WebUi.cpp`, `src/WebUiPage.cpp`, `web/vault/src/*.js`, `web/vault/package.json`, `tools/build-vault-web.py`, `docs/WEB_VAULT.md`.


### 5.13 Xác thực dùng chung (`ApiAuth`)

- `bool hubKeyMatches(const char* provided, std::size_t length);` kiểm tra con trỏ và đúng 64 byte trước khi so với `HUB_API_KEY`. Kích thước cấu hình được kiểm tra lúc biên dịch.
- `constantTimeEquals(const char* a, const char* b, std::size_t n)` tích lũy XOR trên toàn bộ `n` byte. `CommandApi`, `MiotLocal`, `HttpsService` và Web Vault dùng helper key. Web UI dùng phép so chung cho digest mật khẩu và CSRF.

Nguồn: `include/ApiAuth.h`, `src/ApiAuth.cpp`, `src/CommandApi.cpp`, `src/MiotLocal.cpp`, `src/HttpsService.cpp`, `src/WebUi.cpp`.

### 5.14 Ổ cắm Rạng Đông OC09 (`TuyaLocal`)

- API: `bool configured();`, `Result setPower(bool on);`, `const char* errorCode(Status status);`, `bool isTimeout(Status status);`. `Result` gồm `status`, `verified` và `on` (hợp lệ khi `verified=true`).
- Dùng Tuya LAN 3.4 qua TCP/6668. Mỗi lần gửi lệnh mở một kết nối, thương lượng session key rồi gửi một `CONTROL_NEW` cho DP cấu hình. Frame dùng AES-128 và HMAC-SHA256. Driver dùng buffer tĩnh 512 byte và khóa `std::atomic_flag` theo cơ chế trả lỗi ngay khi bận.
- Xác nhận nguồn dựa trên DP bool trong phản hồi `CONTROL_NEW` hoặc `STATUS`. ACK thành công mà thiếu DP trả `Ok` với `verified=false`. Timeout trong thương lượng là `Timeout`. Lỗi gửi có thể đã ghi một phần hoặc thiếu ACK sau khi gửi là `NoAck`, kết quả bật/tắt chưa rõ. Driver và Web UI gửi một lần, không retry.
- Chỉ điều khiển qua `POST /web/command` trên HTTPS với ID `rangdong_oc09_s2`. Trạng thái hiển thị lấy từ phản hồi lệnh. Cấu hình cục bộ ở §7.

Nguồn: `include/TuyaLocal.h`, `src/TuyaLocal.cpp`, `src/WebUi.cpp`, `src/WebUiPage.cpp`, `include/tuya_config.example.h`.

## 6. Giao tiếp ngoài

### 6.1 Tuya LAN và nhãn Tuya cloud

Firmware điều khiển OC09 qua Tuya LAN 3.4. `tuya_cloud` là nhãn transport của mục registry `rangdong_plug_01`. Capability `tuya_control` và `cloud_access` luôn `false` trên cả hai cổng API.

| DP id | Code | Kiểu | Quyền | Mô tả | Hàm xử lý |
|---|---|---|---|---|---|
| `TUYA_OC09_DP` (mặc định 2) | `switch_2` | bool | Ghi, nhận trạng thái trong phản hồi | Nguồn Ổ cắm 2 OC09 | `TuyaLocal::setPower(bool)` |

Nguồn: `src/TuyaLocal.cpp`, `include/tuya_config.example.h`, `src/DeviceRegistry.cpp`, `src/ApiServer.cpp`, `src/HttpsService.cpp`.

### 6.2 HTTP local (cổng 80)

JSON, không TLS. GET thông tin chung công khai; POST commands và Milife state dùng `X-API-Key`.

| Method / route | Response / hành vi |
|---|---|
| GET `/` | `200` `{"service":"KhaiRC-ESP32-Hub","status":"ok"}`, `Cache-Control: no-store` |
| GET `/api/v1/health` | `200`: `status:ok`, `device`, `firmware:"0.4.0-ota1"`, `boot_id`, `uptime_sec`, `free_heap`, `build_id`, `ota_slot`, `ota_state` |
| GET `/api/v1/system` | `200`: `chip`, `flash_mb`, `psram_mb`, `ip`, `rssi_dbm` |
| GET `/api/v1/devices`, `/api/v1/devices/<id>` | Theo §5.4; ID lạ → `404 device_not_found` |
| GET `/api/v1/capabilities` | `schema_version:1`, `ir_send`=`isReady()`, `tuya_control`/`cloud_access` false, `ota_update`=`OtaService::available()` |
| GET `/api/v1/devices/milife_plug_10a/state` | Auth → `401`; chưa cấu hình `503 miot_not_configured`; timeout `504` (`miot_hello_timeout`/`miot_read_timeout`); lỗi khác `502` (`wifi_unavailable`, `miot_network_error`, `miot_device_mismatch`, `miot_auth_failed`, `miot_device_rejected`, `miot_protocol_error`); `200` `power`, `transport:"miio_lan"`, `state_verified:true` |
| POST `/api/v1/commands` | Theo bảng dưới |

Route chi tiết thiết bị nằm trong `onNotFound` (hậu tố sau `/api/v1/devices/` là ID; route Milife state đăng ký riêng nên được khớp trước). Sai method: `/` và route GET đã biết → `405` `Allow: GET`; `/api/v1/commands` → `405` `Allow: POST`; còn lại `404 not_found`. Lỗi dạng `{"error":{"code":"..."}}`.

| Bước | Điều kiện | Lỗi |
|---|---|---|
| 1 | `X-API-Key` khớp `HUB_API_KEY` (độ dài rồi XOR) | `401 unauthorized` |
| 2 | `Content-Type` `application/json` hoặc bắt đầu `application/json;` | `415 unsupported_media_type` |
| 3 | `clientContentLength()` ≤ 512 byte. Raw callback giữ tối đa 512 byte trong buffer ứng dụng | `413 payload_too_large` |
| 4 | Body đã nhận xong, không rỗng, parse được JSON object với `NestingLimit(3)` | `400 invalid_json` |
| 5 | `device_id` (≤48), `action` (≤32) là string không rỗng, không NUL nhúng | `400 invalid_command` |
| 6 | ID có trong registry | `404 device_not_found` |
| 7 | Không phải `milife_plug_10a`; action thuộc từ vựng (quạt `power_on`/`swing`/`power_off`, Toshiba `fix`, Daikin và mục `rangdong_plug_01` nhận `power_on`/`power_off` ở bước kiểm tra này) | `422 unsupported_action` |
| 8 | Là quạt/Toshiba và `IrDiagnostics::isReady()` | `503 device_not_ready` |
| 9 | Send trả `ESP_OK` | `500 ir_send_failed` |

Thành công: `schema_version:1`, `device_id`, `action`, `transport:"ir"`, `status:"sent"`, `state_verified:false`. Field dư không bị từ chối. Không có nonce/replay protection/rate limit; mất response không chứng minh chưa phát — client không tự retry POST với SWING/FIX.

Nguồn: `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/MiotLocal.cpp`, `src/DeviceRegistry.cpp`.

### 6.3 Wi-Fi, Serial, miIO, Tuya LAN và kết nối khác

Wi-Fi STA hostname `khairc-hub`, tắt auto-reconnect của Arduino và gọi reconnect mỗi 30 s khi offline (§5.1); server 80/8081/443 dừng/mở theo trạng thái mạng. Hub là client UDP miIO tới ổ cắm Milife trong LAN (cổng 54321, chỉ đọc nguồn, §5.11). Không có AP/BLE provisioning, mDNS, MQTT, WebSocket, cloud hay Home Assistant. Serial 115200: `s`/`S` TAIZ SWING, `f`/`F` Toshiba FIX, log chẩn đoán (`[WiFi EVENT]`, `[OTA SAFE]`, `[HTTPS]`...).

Hub mở kết nối TCP/6668 tới OC09 theo từng lệnh HTTPS Web UI (§5.14).

Nguồn: `src/WifiService.cpp`, `src/MiotLocal.cpp`, `src/TuyaLocal.cpp`, `src/main.cpp`.

### 6.4 OTA HTTP (cổng 8081)

| Method / route | Contract |
|---|---|
| GET `/` | `200` JSON `{"service":"KhaiRC-ESP32-Hub","status":"ok"}` |
| GET `/update` | HTML công khai; no-store/nosniff/frame DENY; CSP inline script/style, `img-src data:`, kết nối cùng origin |
| GET `/api/v1/health` | Giống cổng 80 (trang OTA đối chiếu boot/build/slot/VALID theo §5.7) |
| GET `/api/v1/ota` | `X-OTA-Password`; `503 ota_safety_not_ready` nếu safety khóa; trả `max_firmware_bytes` = min(4 MiB, vùng đích) |
| POST `/update` | Mật khẩu OTA, `application/octet-stream`, Content-Length bắt buộc; không multipart/chunked; `200` `status:updated`, `rebooting:true` |

Lỗi: 401 unauthorized; 503 `ota_not_configured`/`reboot_pending`/`ota_partition_unavailable`/`ota_safety_not_ready`; 415; 411 `content_length_required`; 413 `firmware_too_large`; 408 `upload_timeout`; 400 cho transfer encoding/length/prefix/ảnh/verify; 500 cho begin/write/chọn boot. Request bị reject đóng kết nối. `otaServer` không đăng ký `onNotFound`; route/method khác dùng phản hồi mặc định của `WebServer` (chưa xác minh nội dung).

Upload đồng bộ trong `handleClient()` của Arduino loop (có thể trì hoãn API 80/Serial); timeout nhận 120 s, đọc mạng 5 s. HTTP không mã hóa — chỉ LAN tin cậy, không forward 8081. SHA-256 chỉ kiểm tra toàn vẹn, không xác thực nhà phát hành. Sau reboot, `OtaSafety` quyết định VALID hoặc rollback (§5.8). Hướng dẫn: `docs/OTA.md`.

Nguồn: `src/OtaService.cpp`, `src/ApiServer.cpp`, `src/OtaPage.cpp`, `docs/OTA.md`.

### 6.5 HTTPS (cổng 443)

Chỉ có khi build kèm `tls_credentials.h` (§5.9). Chứng chỉ tự ký theo `docs/HTTPS_STAGE1.md`; tài liệu yêu cầu chưa mở port forward 443 cho tới khi client Android ghim trust anchor.

| Method / route | Contract |
|---|---|
| GET `/api/v1/health` | Công khai; `status`, `device`, `phase:"https_probe"`, `uptime_sec`, `free_heap`, `build_id`, `ota_state` |
| GET `/api/v1/system`, `/api/v1/capabilities`, `/api/v1/devices` | Công khai, cùng nội dung với cổng 80 |
| GET `/api/v1/devices/milife_plug_10a/state` | Như cổng 80 |
| POST `/api/v1/commands` | Xem dưới |
| GET `/ui/vault`, `/web/vault.js` | Công khai, vault tùy chọn (§5.12), không cần session Hub |
| GET `/ui`, `/web/app.js`, `/web/manifest.webmanifest`, `/web/icon.svg`, `/web/icon.png`, `/web/session`; POST `/web/login`, `/web/logout`, `/web/command` | Web UI (§5.10). Login nhận `password` cho form `/ui` hoặc `api_key` cho Web Vault |

POST `/api/v1/commands` (HTTPS): `X-API-Key` (đúng 64 byte, XOR) → `401`; có `Transfer-Encoding` → `400 invalid_command`; Content-Type như HTTP → `415`; `content_len` ≤0 → `400 invalid_json`, >512 → `413`; đọc đủ body (lỗi đọc → đóng socket, không phản hồi); NUL nhúng/JSON sai → `400 invalid_json`; thiếu/không phải string → `400 invalid_command`; `device_id` khác `toshiba_ac` hoặc action ngoài `state`/`fix`/`swing_on`/`swing_off` → `422`. `state` phải có đúng 5 field (`device_id`, `action`, `power` bool, `temperature_c` int 17–30, `fan_speed` `auto`/`1`–`5`); action khác đúng 2 field; sai → `422`. Chờ reboot OTA hoặc IR chưa sẵn sàng/`TxGate` bận → `503 device_not_ready`; lỗi phát → `500 ir_send_failed`. Thành công giống HTTP với `device_id:"toshiba_ac"`, `state_verified:false`. Lỗi route không khớp dùng mặc định của `esp_http_server`.

POST `/web/command` cho OC09 dùng session, CSRF và kiểm tra body theo §5.10. Body có đúng hai trường chuỗi `device_id:"rangdong_oc09_s2"` và `action:"power_on"` hoặc `action:"power_off"`. Thành công trả `200` với `schema_version:1`, `device_id`, `action`, `transport:"tuya_lan"`, `status:"sent"`, `state_verified`. Field `power` chỉ xuất hiện khi ổ cắm báo DP nguồn.

| HTTP | Mã lỗi OC09 |
|---|---|
| 422 | `unsupported_action` khi ID, action hoặc cấu trúc lệnh sai |
| 503 | `device_not_ready` khi chờ reboot OTA, `tuya_not_configured`, `device_busy` |
| 504 | `tuya_timeout` khi hết hạn thương lượng, `tuya_no_ack` khi kết quả gửi chưa rõ |
| 502 | `wifi_unavailable`, `tuya_network_error`, `tuya_auth_failed`, `tuya_device_rejected`, `tuya_protocol_error` |

Nguồn: `src/HttpsService.cpp`, `src/WebUi.cpp`, `src/TuyaLocal.cpp`, `docs/HTTPS_STAGE1.md`.

## 7. Dữ liệu & cấu hình

- Partition: `default_16MB.csv` từ framework; `docs/OTA.md` ghi otadata `0xe000` (size `0x2000`), app0/ota_0 `0x10000`, app1/ota_1 `0x650000`, mỗi app `0x640000`.
- NVS: namespace `ota_safe` của `OtaSafety`, key `now` và `prev` chứa struct `Journal` ≤64 byte (`magic` `0x4B52434F` "KRCO", `version=1`, `stage`, `seq`, địa chỉ partition, reset reason, OTA state, disconnect reason, build id 16 ký tự, checksum FNV-1a). Không chứa secret. Không dùng SPIFFS/LittleFS/EEPROM. Phiên bản bản ghi khác → coi như không hợp lệ, `seq` bắt đầu lại từ 1.
- RAM: catalog/mã IR cố định trong firmware; session Web UI, bộ đếm đăng nhập sai, cờ server/RMT chỉ trong RAM (mất khi reboot).
- Build flags: `BOARD_HAS_PSRAM`, `ARDUINO_USB_CDC_ON_BOOT=0`. Không có Kconfig/`sdkconfig.defaults` được track; `CONFIG_APP_ROLLBACK_ENABLE` phụ thuộc cấu hình prebuilt của Arduino (§10).
- Cấu hình cục bộ bị Git bỏ qua (không ghi giá trị):

| File cục bộ | Macro | Nơi dùng | Template | Thiếu thì |
|---|---|---|---|---|
| `include/secrets.h` | `WIFI_SSID`, `WIFI_PASSWORD`, `HUB_API_KEY` (đúng 64 ký tự, `static_assert`) | `WifiService`, `ApiAuth` | `include/secrets.example.h` | Không build được |
| `include/ota_password.h` | `HUB_OTA_PASSWORD` (1–128 ký tự ASCII 33–126) | `OtaService` | `include/ota_password.example.h` | OTA bị khóa |
| `include/miot_config.h` | `MILIFE_IP`, `MILIFE_DID`, `MILIFE_TOKEN` (32 hex, token miIO LAN) | `MiotLocal` | `include/miot_config.example.h` | Milife `not_configured` |
| `include/tuya_config.h` | `TUYA_OC09_IP` (IPv4 hợp lệ và khác 0), `TUYA_OC09_DEVICE_ID` (khác rỗng), `TUYA_OC09_LOCAL_KEY` (16 byte), `TUYA_OC09_DP` (1–255, mặc định 2) | `TuyaLocal` | `include/tuya_config.example.h` | Lệnh OC09 trả `503 tuya_not_configured` |
| `include/firebase_web_config.h` | `FIREBASE_WEB_API_KEY`, `FIREBASE_AUTH_DOMAIN`, `FIREBASE_PROJECT_ID`, `FIREBASE_APP_ID` (mỗi chuỗi 1–200 byte) | `WebUi` | `include/firebase_web_config.example.h` | Vault báo chưa cấu hình; đăng nhập thủ công dùng mật khẩu Web UI nếu đã cấu hình |
| `include/web_password.h` | `HUB_WEB_PASSWORD` (1–64 byte UTF-8, loại byte điều khiển ASCII và DEL) | `WebUi` | `include/web_password.example.h` | Đăng nhập bằng mật khẩu trả `503`. Vault vẫn dùng `api_key` |
| `include/tls_credentials.h` (+ `tls_certificate.pem`, `tls_private_key.pem`) | `HUB_TLS_CERT_PEM`, `HUB_TLS_KEY_PEM` | `HttpsService` | Sinh bằng `tools/create_https_credentials.py` | HTTPS tắt |

- Vault mã hóa nằm trên Firestore, browser đọc; không thêm NVS trên ESP32. Firebase Web app config được đưa vào HTML khi bật vault, file local được ignore; không ghi giá trị thật vào spec. Auth domain phải là hostname không scheme/path theo tài liệu; code chỉ kiểm tra độ dài.
- `logs/hub-monitor-<timestamp>.csv` là đầu ra của monitor trên PC, không phải storage ESP32.

Nguồn: `platformio.ini`, `.gitignore`, `src/OtaSafety.cpp`, `include/*.example.h`, `src/MiotLocal.cpp`, `src/TuyaLocal.cpp`, `src/HttpsService.cpp`, `src/OtaService.cpp`, `src/WebUi.cpp`, `docs/OTA.md`, `docs/WEB_VAULT.md`.

## 8. Quy ước & pattern của codebase

- Mỗi module: `include/<Module>.h` (`#pragma once`) + `src/<Module>.cpp`, `namespace <Module>`, state/helper trong anonymous namespace; `constexpr` cho hằng. Helper thuần (`ToshibaCodec`, `OtaSafetyPolicy`) là header-only để test host được.
- Service polling theo `begin()`/`loop()`; `main` quyết định thứ tự. `OtaSafety::begin()` phải chạy trước Wi-Fi, `OtaSafety::loop()` sau các server.
- Log Serial theo tag: `[WiFi]`, `[WiFi EVENT]`, `[API]`, `[CommandAPI]`, `[IR]`, `[IR TEST]`, `[Devices]`, `[OTA]`, `[OTA SAFE]`, `[HTTPS]`; lỗi ESP-IDF qua `esp_err_to_name()`.
- HTTP trả lỗi JSON `{"error":{"code":"..."}}`. Các handler xác thực trước khi parse lệnh. HTTP/80 nhận body trước qua raw callback có buffer giới hạn, HTTPS xác thực trước khi handler đọc body. `ApiAuth` tập trung so `X-API-Key` bằng XOR trên 64 byte. Xem kiểm tra NUL và `NestingLimit(3)` ở §6.2/§6.5. IR trả `sent` sau transmit/wait và `state_verified:false`. Milife đọc trực tiếp nên trả `state_verified:true`.
- Không retry: lệnh IR và đọc miIO đúng một lần mỗi request; lỗi đọc body HTTPS đóng socket. Tài nguyên dùng chung giữa task dùng atomic/khóa fail-fast (`TxGate`), không xếp hàng.
- Fail closed: HTTPS đăng ký thiếu route thì dừng; OTA khóa khi thiếu mật khẩu hoặc safety chưa xác nhận; tính năng phụ thuộc file cục bộ bật bằng `__has_include`.
- Trang điều khiển nhúng PROGMEM raw string, không CDN; vault dùng bundle sinh và CSP cloud riêng (§5.12); form mật khẩu hỗ trợ autofill của trình duyệt; vault chỉ xử lý key trong bộ nhớ tab, session cookie `HttpOnly`/`Secure`/`SameSite=Strict` + CSRF.
- Firmware version là chuỗi cố định ở `main.cpp` và health cổng 80; `build_id` từ ELF SHA-256 là định danh build thực.
- Quy định agent (`AGENTS.md`): không đọc/in `include/secrets.h`, `include/ota_password.h`, `.env` (và không đưa nội dung file credential cục bộ khác vào ngữ cảnh); không quét `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`, `backups/`, `*.patch` (trừ `.pio/build.log`); đọc toàn bộ file `.cpp`/`.h` trước khi sửa, riêng `src/OtaPage.cpp` (và tương tự trang nhúng) chỉ đọc phần C++ trừ khi sửa giao diện. Không dùng thư viện IR/OTA bên thứ ba. Context7 chỉ dùng khi cần tra API, không gửi secret. Agent làm đơn luồng, không subagent, không báo cáo giữa chừng.
- Quy định OTA: auth → prefix → write → verify → chọn boot → reboot; không mật khẩu mặc định; giới hạn 4 MiB, không multipart/chunked, không retry upload (chỉ retry GET health); mật khẩu không vào URL/localStorage/cookie; đổi OTA phải cập nhật `docs/OTA.md` và test host.

Nguồn: `include/*.h`, `src/*.cpp`, `AGENTS.md`, `CLAUDE.md`.

## 9. Cách thêm tính năng mới

- **Thêm lệnh/giao thức IR:** encoder thuần đặt header-only kiểu `ToshibaCodec`; hàm send trong `IrDiagnostics` phải giữ `TxGate` trước khi đụng buffer static, kiểm tra `isReady()`, gọi `transmitSymbols()`. Cập nhật `supported_actions` ở `src/DeviceRegistry.cpp`, dispatch ở `src/CommandApi.cpp` (HTTP/80) và/hoặc `src/HttpsService.cpp`/`src/WebUi.cpp` (HTTPS), capability nếu cần. Phím Serial ở `src/main.cpp` chỉ khi cần thử tay.
- **Thêm thiết bị LAN (kiểu miIO):** module riêng theo mẫu `MiotLocal`: cấu hình qua header cục bộ + `__has_include`, template `include/*.example.h`, thêm vào `.gitignore`; một request/không retry; mã lỗi phân biệt timeout (504) và lỗi giao thức (502); đăng ký route cả HTTP và HTTPS nếu cần.
- **Thêm route HTTPS:** tăng `max_uri_handlers` (hiện 17, đang dùng 6 API + 11 Web UI = 17) trước khi thêm route; giữ fail-closed khi đăng ký; handler chạy trên task httpd nên mọi tài nguyên dùng chung với Arduino loop cần đồng bộ.
- **Thêm route HTTP/80:** dùng `ApiAuth` cho route cần API key, giới hạn buffer và kiểm tra body, giữ mã lỗi, nhánh 405 trong `onNotFound`; đồng bộ registry/capabilities; giữ nghĩa `sent`/`state_verified`. Cập nhật `tools/monitor-hub.ps1` nếu health/system đổi.
- **Đổi OTA/khởi động:** giữ `OtaSafety` health gate (thêm điều kiện mới vào `HealthGate::ready`), cập nhật `docs/OTA.md`, `test/ota_host` (stub `OtaSafety.h`) và chạy test host/trang.
- **Đổi Web Vault:** sửa JS, sinh bundle bằng `tools/build-vault-web.py`, commit source/bundle/lockfile cùng nhau; test Node/KAT app và cập nhật `docs/WEB_VAULT.md`. Giữ đọc cloud một lần, không ghi Firestore, không lưu key/password, không tự retry login; thử tab thường và login thủ công.
- **Thêm DP Tuya LAN:** xác nhận phiên bản giao thức và DP của thiết bị, rồi đối chiếu mẫu OC09 ở §5.14. Với nút điều khiển Web UI, giữ session/CSRF, gửi một lệnh mỗi lần bấm và chỉ đánh dấu xác nhận khi nhận được giá trị DP.
- **Thêm chân/ngoại vi:** đối chiếu §1 và `AGENTS.md`, không đổi GPIO4; cập nhật pin map §1.
- **Thêm NVS key:** dùng namespace riêng (đã có `ota_safe`), định nghĩa magic/version/checksum như `Journal`, ghi vào §7; không lưu secret.

Quy trình phân vai (mỗi vai một chat):
- `$architect <yêu cầu>`: chỉ ghi `docs/PLAN.md` (bị Git bỏ qua) dạng Zero-Discovery Plan, trích sẵn ngữ cảnh code.
- `$implement [phạm vi]`: sửa liền mạch mọi Step trong phạm vi, build/test và tick `[x]` một lần ở cuối; không gọi lại `tools/spec.py`.
- `$reviewer`: chỉ đọc diff, chạy kiểm chứng, báo cáo. `$debugger`: chỉ chẩn đoán từ bằng chứng. `$docs`: sau commit, đồng bộ spec.
- `$architect`/`$reviewer`/`$debugger` đọc spec theo mục: luôn §0, §8; thêm §9 khi thêm tính năng; module/§6 theo §12.
- Không tự commit/push/upload. Xong việc = build sạch + test host liên quan pass + danh sách thử trên mạch. Build cần `include/secrets.h` cục bộ; thiếu thì dừng, không tự tạo giá trị thật. `docs/CODEX_WORKFLOW.md` mô tả quy trình này cho người dùng.

Nguồn: `AGENTS.md`, `CLAUDE.md`, `.agents/skills/**`, `src/HttpsService.cpp`, `src/IrDiagnostics.cpp`, `src/MiotLocal.cpp`, `src/TuyaLocal.cpp`, `src/WebUi.cpp`, `src/OtaSafety.cpp`, `docs/CODEX_WORKFLOW.md`.

## 10. Vấn đề đã biết, TODO, cần xác nhận

- [ ] Chưa có bản vẽ sơ đồ riêng; AGENTS mô tả mạch IR. — Nguồn: `AGENTS.md`.
- [ ] ESP-IDF 5.5.5 chỉ được ghi trong `docs/HTTPS_STAGE1.md`, không ghim trong `platformio.ini`. Chưa có CI. — Nguồn: `platformio.ini`, `docs/HTTPS_STAGE1.md`.
- [ ] Cần xác nhận trên board: Arduino 3.3.12 prebuilt có `CONFIG_APP_ROLLBACK_ENABLE` và gọi hook `verifyRollbackLater()`. Nếu không, `canAcceptUpdate()` luôn false → OTA luôn `503 ota_safety_not_ready`; phải nạp USB. — Nguồn: `src/OtaSafety.cpp`.
- [ ] Health gate của ảnh OTA mới yêu cầu `HttpsService::ready()`. Firmware build không có `tls_credentials.h` (HTTPS tắt) hoặc TLS start lỗi sẽ không bao giờ được đánh dấu VALID và bị rollback sau 90 s nếu ảnh đó được nạp qua OTA. — Nguồn: `src/OtaSafety.cpp`, `src/HttpsService.cpp`.
- [ ] `docs/HTTPS_STAGE1.md` vẫn mô tả HTTPS chỉ có health trong khi code có API Toshiba/Milife và Web UI/vault. Spec theo code. — Nguồn: `docs/HTTPS_STAGE1.md`, `docs/OTA.md`, `src/HttpsService.cpp`, `src/WebUi.cpp`.
- [ ] `MiotLocal::readPower()` có thể được gọi đồng thời từ Arduino loop (HTTP/80) và task HTTPS mà không có khóa; chưa xác minh an toàn khi hai request chồng nhau. Mỗi lần đọc chặn luồng gọi tới ~4,4 s. — Nguồn: `src/MiotLocal.cpp`, `src/HttpsService.cpp`.
- [ ] Catalog Toshiba chỉ liệt kê `fix`, nhưng HTTPS/Web UI nhận `state`, `swing_on`, `swing_off`; HTTP/80 chỉ nhận `fix`. — Nguồn: `src/DeviceRegistry.cpp`, `src/CommandApi.cpp`, `src/HttpsService.cpp`.
- [ ] Health HTTPS không có `firmware`, `boot_id`, `ota_slot` như cổng 80. Phiên bản `0.4.0-ota1` cố định dù firmware đã thêm HTTPS/OtaSafety. — Nguồn: `src/HttpsService.cpp`, `src/ApiServer.cpp`, `src/main.cpp`.
- [ ] Driver IR không tự phục hồi sau lỗi transmit/wait (`initialized=false`, các lệnh sau `503`/`500` tới khi reboot). HTTP/80 trả `500 ir_send_failed` khi `TxGate` bận còn HTTPS trả `503`. — Nguồn: `src/IrDiagnostics.cpp`, `src/CommandApi.cpp`, `src/HttpsService.cpp`.
- [ ] Không có RX/readback IR; `state_verified:false`. Chưa có log thử phần cứng được track cho từng lệnh; mã Toshiba state/swing phản chiếu encoder Android ngoài repo, chưa đối chiếu. — Nguồn: `src/IrDiagnostics.cpp`, `include/ToshibaCodec.h`.
- [ ] HTTP/80 nhận `power_on`/`power_off` cho Daikin và mục registry `rangdong_plug_01` ở bước kiểm tra action rồi trả `503 device_not_ready`. OC09 dùng ID `rangdong_oc09_s2` trong Web UI. Registry và capability hiện chưa phản ánh khả năng điều khiển OC09. — Nguồn: `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/ApiServer.cpp`, `src/WebUi.cpp`.
- [ ] Footer Web UI vẫn ghi “Chỉ Toshiba” dù có thẻ OC09. Cần thử trên Hub và OC09 thật để xác nhận bật/tắt, DP báo về, ACK thiếu DP và mất ACK. — Nguồn: `src/WebUiPage.cpp`, `src/TuyaLocal.cpp`, `include/tuya_config.example.h`.
- [ ] `otaServer` (8081) không có `onNotFound`; nhánh 405 cho route OTA không còn — phản hồi sai method/route lạ dùng mặc định `WebServer`. — Nguồn: `src/ApiServer.cpp`, `src/OtaService.cpp`.
- [ ] `backups/` (2 file `.bin`, 1 bản sao `HttpsService.cpp` cũ) đã được track dù `.gitignore` có `backups`. — Nguồn: `git ls-files`, `.gitignore`.
- [ ] Chưa có test host cho handler C++ HTTPS/Web UI/MiotLocal/`ToshibaCodec`/`OtaSafetyPolicy`; rollback OTA, TLS và miIO cần thử trên board. — Nguồn: `test/ota_host/**`, `tools/test-ota.py`.

- [ ] Chưa có KAT app được track; fixture Node chưa chứng minh tương thích Kotlin/Android. Cần KAT dữ liệu giả, thử password tiếng Việt trên Android/Safari, Google popup, storage/network, route/heap (`max_open_sockets=2`) và điều khiển sau login. — Nguồn: `web/vault/test/vault.test.mjs`, `docs/WEB_VAULT.md`, `src/HttpsService.cpp`.

- [ ] Cần thử đăng nhập bằng mật khẩu trên Hub thật: autofill Safari, từ chối khi firmware thiếu `HUB_WEB_PASSWORD` và tạo phiên qua Web Vault bằng `api_key`. Playwright dùng backend mô phỏng. — Nguồn: `src/WebUi.cpp`, `src/WebUiPage.cpp`, `tools/test-web-login.py`.
- [ ] Cần kiểm chứng raw callback của `CommandApi` trên board với body đủ/thiếu và quá giới hạn. Handler HTTP/80 tạo body từ chuỗi C, còn kiểm tra NUL trong JSON áp dụng riêng cho `device_id` và `action`. — Nguồn: `src/CommandApi.cpp`, `platformio.ini`.

Không tìm thấy marker `TODO`, `FIXME`, `HACK`, `XXX` trong mã nguồn và test/tool được quét.

Nguồn: `src/*.cpp`, `include/*.h`, `docs/OTA.md`, `docs/HTTPS_STAGE1.md`, `.gitignore`.

## 11. Nhật ký đồng bộ

- 2026-11-02 FULL @19f655c — tạo đặc tả, kiểm kê 22 file tracked.
- 2026-11-05 FULL 19f655c..5a47234 — HTTP IR `0.3.0-ir1`, lệnh quạt/Toshiba, readiness, Serial thủ công.
- 2026-11-05 INCR 5a47234..f9f0063 — quy định AGENTS về chân cấm, bảo mật, RMT, an toàn lệnh.
- 2026-11-05 FULL f9f0063..af15f65 — OTA, test, toolchain ghim, template credential, quy trình agent.
- 2026-11-05 INCR af15f65..703392f — favicon SVG, CSP data image, cấm đọc credential OTA.
- 2026-11-06 INCR 703392f..21fe217 — đọc spec theo mục bằng `tools/spec.py`, quy trình năm vai.
- 2026-11-13 FULL 21fe217..d66cbf6 — quét lại 76 file vì chạm 7/7 module cũ và thêm 6 module; thêm §5.8–§5.11 (OtaSafety, HttpsService, WebUi, MiotLocal), §6.5 HTTPS, OTA chuyển cổng 8081, IR `TxGate` + Toshiba state/swing, NVS `ota_safe`, credential miIO/TLS, quy định agent mới. Không build/flash; không đọc file credential cục bộ.

- 2026-11-17 INCR d66cbf6..9a48012 — vault Firebase/crypto/bundle và test/cấu hình; OTA boot/build/slot/VALID, build ID descriptor; Wi-Fi reconnect; Referrer-Policy login, fingerprint cert. Chỉ kiểm tra tài liệu, không build/flash hay test firmware.
- 2026-11-19 FULL 9a48012..f065965 — kiểm kê 93 file, thêm §5.13 `ApiAuth`, cập nhật đăng nhập bằng mật khẩu Web UI, raw body HTTP/80, helper dùng chung HTTP/HTTPS, IR và OTA Safety.
- 2026-11-26 INCR f065965..cfd0ae9 — thêm §5.14 `TuyaLocal`, cập nhật Web UI, cấu hình và API điều khiển OC09 qua Tuya LAN 3.4.

Nguồn: cây Git và lịch sử commit tại mốc đồng bộ trong front matter.

## 12. Bản đồ file → mục

Mỗi file trong 96 file tracked tại mốc đồng bộ thuộc một dòng dưới đây.

| Glob | Mục | Ghi chú |
|---|---|---|
| `src/main.cpp` | §0, §4, §6.3, §8–§10 | Entry Arduino, thứ tự khởi tạo, phím IR |
| `include/WifiService.h`, `src/WifiService.cpp` | §4, §5.1, §6.3, §7 | Wi-Fi, event → OtaSafety |
| `include/ApiServer.h`, `src/ApiServer.cpp` | §4, §5.2, §6.2, §6.4, §10 | Server 80 + 8081 |
| `include/CommandApi.h`, `src/CommandApi.cpp` | §5.3, §6.2, §7–§10 | POST commands HTTP/80 |
| `include/ApiAuth.h`, `src/ApiAuth.cpp` | §3, §5.13, §6.2, §6.5, §7–§9 | Xác thực API key và so byte dùng chung |
| `include/DeviceRegistry.h`, `src/DeviceRegistry.cpp` | §5.4, §6, §9, §10 | Catalog |
| `include/IrDiagnostics.h`, `src/IrDiagnostics.cpp`, `include/ToshibaCodec.h` | §0, §1, §4, §5.5, §6, §8–§10 | Driver IR + encoder Toshiba |
| `include/OtaService.h`, `src/OtaService.cpp` | §5.6, §6.4, §7–§10 | OTA stream |
| `include/OtaPage.h`, `src/OtaPage.cpp` | §5.7, §6.4 | Trang OTA nhúng |
| `include/OtaSafety.h`, `include/OtaSafetyPolicy.h`, `src/OtaSafety.cpp` | §0, §4, §5.8, §7–§10 | Rollback, nhật ký NVS |
| `include/HttpsService.h`, `src/HttpsService.cpp` | §4, §5.9, §6.5, §7, §9, §10 | Server TLS 443 |
| `include/WebUi.h`, `src/WebUi.cpp`, `include/WebUiPage.h`, `src/WebUiPage.cpp` | §0, §3, §5.4, §5.10, §5.12, §5.14, §6.5, §7–§10 | Web UI Toshiba, OC09 và vault trên HTTPS |
| `include/MiotLocal.h`, `src/MiotLocal.cpp` | §4, §5.11, §6.2, §6.3, §7, §10 | Đọc Milife miIO LAN |
| `include/TuyaLocal.h`, `src/TuyaLocal.cpp`, `include/tuya_config.example.h` | §0, §3, §4, §5.14, §6.1, §6.3, §6.5, §7, §9, §10 | Điều khiển Ổ cắm 2 OC09 qua Tuya LAN |
| `include/VaultWebBundle.h`, `src/VaultWebBundle.cpp` | §2, §3, §5.12 | Bundle gzip sinh, PROGMEM |
| `web/vault/**` | §2, §3, §5.12, §7–§10 | Source JS, dependency/lockfile và test Node |
| `docs/WEB_VAULT.md` | §2, §5.12, §6.5, §7–§10 | Cấu hình Firebase, định dạng vault, KAT và thử board |
| `include/*.example.h` | §5.10, §7 | Template credential và mật khẩu Web UI |
| `platformio.ini` | §1, §2, §7 | Build, board, extra script |
| `.gitignore` | §3, §7, §10 | File cục bộ/secret bị bỏ qua |
| `tools/**` | §2, §7 | Monitor, spec, test OTA, TLS, IntelliSense |
| `test/ota_host/**`, `test/README`, `lib/README` | §2, §3, §10 | Test host OTA, README mẫu |
| `docs/OTA.md` | §2, §6.4, §7, §10 | Hướng dẫn OTA |
| `docs/HTTPS_STAGE1.md` | §2, §6.5, §10 | Hướng dẫn chứng chỉ/HTTPS |
| `docs/CODEX_WORKFLOW.md` | §3, §9 | Hướng dẫn người dùng về quy trình agent |
| `AGENTS.md`, `CLAUDE.md`, `.agents/skills/**`, `.claude/skills/**` | §1, §2, §8, §9 | Chỉ thị agent và skill |
| `backups/**` | §3, §10 | Không quét nội dung |
| `.vscode/**` | §3 | Chỉ kiểm kê tên |
| `docs/PROJECT_SPEC.md` | §0–§12 | Spec tự quản lý |

Các file cục bộ `include/secrets.h`, `include/ota_password.h`, `include/web_password.h`, `include/miot_config.h`, `include/tuya_config.h`, `include/firebase_web_config.h`, `include/tls_credentials.h`, `include/tls_*.pem`, `.pio/build.log`, `logs/`, `docs/PLAN.md` và `default_16MB.csv` không phải nguồn được track.

Nguồn: `git ls-files` tại mốc đồng bộ; `.gitignore`.
