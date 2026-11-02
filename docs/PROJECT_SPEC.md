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
| IR TX / `IR_PIN` | 4 | Output | GPIO trực tiếp khi khởi tạo/thử DC; RMT TX khi phát IR; nghỉ LOW, mark HIGH |
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

Nguồn: `platformio.ini`, `include/README`, `lib/README`, `test/README`, `tools/monitor-hub.ps1`, `src/IrDiagnostics.cpp`.

## 3. Cấu trúc thư mục & kiến trúc

HEAD có 18 file được track: `src/` 6 file `.cpp`, `include/` 5 header và 1 README, `lib/`, `test/`, `tools/`, `.vscode/` mỗi thư mục 1 file, cùng `platformio.ini` và `.gitignore`. Metadata IDE được kiểm kê nhưng bỏ qua khi phân tích chức năng.

| Vị trí | Vai trò |
|---|---|
| `src/main.cpp` | Điểm vào Arduino, gọi vòng lặp dịch vụ và phát IR thử |
| `src/`, `include/` | Năm module: Wi-Fi, API server, command API, device registry, IR diagnostics |
| `tools/monitor-hub.ps1` | Tiện ích PowerShell đọc Serial và lưu log |
| `lib/`, `test/` | Chỉ có README mẫu; chưa có thư viện nội bộ hoặc test |
| `platformio.ini` | Board, framework, flags, dependency |
| `.gitignore` | Loại build output, cấu hình IDE, credential cục bộ và log khỏi Git |

Luồng phụ thuộc: `main → WifiService, DeviceRegistry, ApiServer, IrDiagnostics`; `ApiServer → WifiService, DeviceRegistry, CommandApi`; `CommandApi → DeviceRegistry, ArduinoJson`; `IrDiagnostics → GPIO/RMT`. Không có đường gọi `CommandApi → IrDiagnostics`.

Nguồn: `src/main.cpp`, `src/ApiServer.cpp`, `src/CommandApi.cpp`, `src/DeviceRegistry.cpp`, `src/IrDiagnostics.cpp`, `src/WifiService.cpp`, `platformio.ini`, `.gitignore`, `include/README`, `lib/README`, `test/README`, `tools/monitor-hub.ps1`.

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

## 6. Giao tiếp ngoài

## 7. Dữ liệu & cấu hình

## 8. Quy ước & pattern của codebase

## 9. Cách thêm tính năng mới

## 10. Vấn đề đã biết, TODO, cần xác nhận

## 11. Nhật ký đồng bộ

## 12. Bản đồ file → mục
