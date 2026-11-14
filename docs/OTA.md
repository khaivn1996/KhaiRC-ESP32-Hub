# Nạp firmware qua Wi-Fi cho KhaiRC ESP32-S3 Hub

Tính năng OTA cung cấp trang `http://<IP-mạch>:8081/update` trên cổng riêng biệt với REST API (cổng 80). Bạn nhập mật khẩu OTA riêng, chọn `firmware.bin` và upload. Mạch kiểm tra file, ghi vào vùng ứng dụng còn lại rồi tự khởi động lại khi thành công. Trang web được phục vụ hoàn toàn từ ESP32, không cần Internet/CDN. **Không forward cổng 8081 trên router**; tách cổng không thay thế xác thực và firewall.

## 1. Giới hạn đã chọn

| Nội dung | Giới hạn | Lý do |
|---|---:|---|
| Body API JSON `/api/v1/commands` | 512 byte | Đủ cho các trường `device_id`, `action` và các tham số nhỏ; giữ quy tắc bảo mật hiện có |
| File firmware OTA | 4.194.304 byte (4 MiB) | Firmware hiện tại khoảng 1 MiB; còn dư cho việc mở rộng, nằm dưới dung lượng mỗi vùng OTA |

Giới hạn JSON áp dụng tại handler ứng dụng như trước; không phải giới hạn cấp phát chung cho mọi request của thư viện WebServer. OTA nhận dữ liệu nhị phân theo từng khối, không đưa toàn bộ file vào RAM và không đi qua bộ parse JSON.

Với toolchain được ghim trong `platformio.ini` (`espressif32 @ 8.0.0`, Arduino 3.3.12), `default_16MB.csv` có:

| Phân vùng | Offset | Dung lượng |
|---|---:|---:|
| `otadata` | `0xe000` | `0x2000` |
| `app0` / `ota_0` | `0x10000` | `0x640000` = 6,25 MiB |
| `app1` / `ota_1` | `0x650000` | `0x640000` = 6,25 MiB |

Firmware còn kiểm tra phân vùng đang có trên mạch. Giới hạn thực tế là giá trị nhỏ hơn giữa 4 MiB và kích thước phân vùng OTA đích; từ chối nếu không có vùng ứng dụng khác phù hợp. OTA không thay bảng phân vùng hay bootloader.

## 2. Tự đặt mật khẩu

1. Sao chép `include/ota_password.example.h` thành `include/ota_password.h`. Chỉ tạo bản sao nếu file đích chưa tồn tại.
2. Mở **file mới** và điền mật khẩu của bạn vào chuỗi của macro:

   ```cpp
   #define HUB_OTA_PASSWORD ""
   ```

   Điền mật khẩu **giữa hai dấu nháy kép**, thay cho chuỗi rỗng. Mật khẩu phân biệt chữ hoa/thường, hỗ trợ 1–128 ký tự ASCII hiển thị, không có khoảng trắng hoặc dấu tiếng Việt. Khuyến nghị ít nhất 12 ký tự kết hợp chữ, số, ký hiệu. Nếu dùng dấu `"` hoặc `\`, cần escape theo cú pháp chuỗi C++; có thể chọn ký hiệu khác để nhập dễ hơn.

3. Lưu file trước khi build. File này được `.gitignore` bảo vệ. Mật khẩu OTA độc lập với `HUB_API_KEY` và mật khẩu Wi-Fi.

Thiếu file, mật khẩu rỗng hoặc không đúng định dạng sẽ **khóa OTA**, không có mật khẩu mặc định. Không cần gửi mật khẩu cho người khác. Mật khẩu được biên dịch vào firmware; trang web không cung cấp chức năng thay mật khẩu trực tiếp.

Muốn đổi mật khẩu sau này: sửa file cục bộ, build lại, dùng **mật khẩu cũ** để upload; sau khi mạch chạy firmware mới, dùng **mật khẩu mới**. Mỗi bản firmware tiếp theo phải tiếp tục chứa chức năng OTA và mật khẩu đã cấu hình.

## 3. Nạp USB một lần để cài OTA

Firmware đang chạy trên mạch chưa có trình nhận OTA, nên cần bước này một lần sau khi cấu hình mật khẩu.

1. Kết nối mạch với PC bằng USB, mở terminal tại thư mục project.
2. Build:

   ```powershell
   pio run -e esp32-s3-n16r8
   ```

3. Đóng Serial Monitor nếu đang mở, rồi nạp:

   ```powershell
   pio run -e esp32-s3-n16r8 -t upload
   ```

   Nếu có nhiều cổng COM, chọn đúng cổng bằng `--upload-port COMx` (thay `COMx` bằng cổng thực tế).

4. Xem IP:

   ```powershell
   pio device monitor -b 115200
   ```

   Tìm dòng `[WiFi] IP: ...`. Khi mật khẩu hợp lệ, log có `[OTA] Web upload: /update`; nếu chưa cấu hình sẽ có `[OTA] Disabled: ...`. Dùng `Ctrl+C` để thoát monitor.

5. Cho PC vào cùng mạng LAN với ESP32, mở `http://<IP-mạch>:8081/update`. Cổng 80 giờ dùng cho REST API; truy cập `/` trả JSON thông tin Hub thay vì chuyển sang OTA. Nếu không truy cập được, kiểm tra IP, Wi-Fi và chế độ cách ly thiết bị trên router/guest network.

Sau bước này có thể rút cáp nối PC và cấp nguồn cho mạch bằng nguồn phù hợp. OTA thay đường truyền firmware; mạch vẫn cần nguồn điện.

## 4. Các lần cập nhật qua Wi-Fi

1. Sửa code, giữ cấu hình Wi-Fi/mật khẩu OTA hợp lệ, rồi chạy:

   ```powershell
   pio run -e esp32-s3-n16r8
   ```

2. Mở `http://<IP-mạch>:8081/update` trên trình duyệt cùng LAN.
3. Nhập mật khẩu OTA đang có trên mạch.
4. Chọn đúng file ứng dụng:

   ```text
   .pio/build/esp32-s3-n16r8/firmware.bin
   ```

   Chọn file từ bản build thành công vừa chạy. `bootloader.bin`, `partitions.bin`, file `.elf` hoặc binary gộp để nạp USB không dùng cho trang OTA này. File ứng dụng của project khác trên cùng chip có thể hợp lệ về định dạng nhưng thiếu Wi-Fi/OTA của hub, vì vậy chỉ dùng firmware của project này.

5. Bấm **Upload và khởi động lại**. Giữ nguồn điện ổn định. 100% thanh tiến trình chỉ có nghĩa trình duyệt đã gửi xong file; tiếp tục chờ xác minh và khởi động lại.
6. Trang chỉ báo thành công khi health trả `status: ok`, `boot_id` mới, `build_id` khớp 16 ký tự hex đầu của ELF SHA-256 trong file đã chọn, phân vùng đổi giữa `app0`/`app1` và `ota_state: valid`. Firmware nền thiếu metadata OTA Safety bị chặn trước POST; cần nạp nền qua USB. Trang thử tối đa 60 GET health, cách nhau 2 giây, mỗi GET timeout 5 giây. Boot ID đổi do rollback, sai build hoặc còn `pending_verify` không được báo thành công. Phiên bản hiển thị `0.4.0-ota1` không dùng để xác minh build.

Không cần lệnh `-t upload` cho các lần OTA qua trình duyệt. Nếu IP đổi sau khi khởi động, xem danh sách DHCP trên router để tìm `khairc-hub`; có thể đặt DHCP reservation trên router để IP ổn định.

## 5. Xử lý lỗi và phạm vi sử dụng

| Hiện tượng | Cách xử lý |
|---|---|
| Báo chưa bật OTA | Đặt mật khẩu, build và nạp USB lần đầu; chỉ sửa file trên PC không thay firmware đang chạy |
| Sai mật khẩu | Nhập mật khẩu đã được build vào firmware đang chạy, kiểm tra hoa/thường |
| File quá lớn | Chọn đúng `firmware.bin`; ngưỡng tối đa 4 MiB, không đổi tên file để vượt giới hạn |
| File sai định dạng/chip/hash | Build đúng environment `esp32-s3-n16r8`; chọn file ứng dụng có SHA-256 do toolchain tạo |
| Upload mất kết nối | Chờ mạch ổn định rồi kiểm tra health/IP trước khi thử thủ công; trang không tự gửi lại firmware |
| Đã ghi thành công nhưng chưa thấy mạch online | Kiểm tra nguồn, DHCP/IP và cấu hình Wi-Fi của firmware mới |

- Dùng trong LAN tin cậy trên cổng 8081 (không forward từ Internet). HTTP không mã hóa mật khẩu hoặc firmware; tách cổng chỉ ngăn OTA đi qua rule Port Forward dành cho API trên cổng 80, không tự bảo vệ khi router định tuyến 8081 từ WAN. Mật khẩu không nằm trong URL, không được lưu vào localStorage/cookie và được xóa khỏi ô nhập sau lượt gửi.
- Firmware được xác thực mật khẩu trước khi xóa/ghi flash; kiểm tra chip ESP32-S3, header ứng dụng, số byte và checksum/SHA-256 trước khi chọn bản khởi động mới. SHA-256 ở đây kiểm tra tính toàn vẹn ảnh, không phải chữ ký xác thực nhà phát hành.
- Mất kết nối/ghi lỗi trước bước chọn bản khởi động mới sẽ không kích hoạt ảnh đang ghi dở. Code không tự retry upload.
- Trong lúc nhận file, WebServer xử lý đồng bộ; các API điều khiển/Serial có thể tạm chờ. Có giới hạn 120 giây cho luồng upload và timeout đọc mạng 5 giây.
- `OtaSafety` trì hoãn xác nhận ảnh `PENDING_VERIFY`: Wi-Fi/IP, HTTP, HTTPS và IR phải sẵn sàng liên tục 15 giây trước khi mark VALID; task độc lập thử rollback sau 90 giây. Đây là kiểm tra readiness nội bộ, không chứng minh mọi client truy cập được. Cần bootloader hỗ trợ rollback và slot dự phòng dùng được; cấu hình build không thay thế test trên mạch. File hợp lệ nhưng code lỗi, sai Wi-Fi, thiếu OTA hoặc quên mật khẩu vẫn có thể cần USB để khôi phục.
- Wi-Fi tắt auto-reconnect của Arduino; loop thử lại mỗi 30 giây khi offline, dừng lần kết nối cũ bằng `disconnect(false, false)` trước `reconnect()`, không xóa cấu hình AP. Arduino 3.3.12 vẫn có thể tự thử một lần sau lỗi đầu tiên. Thay đổi này xử lý reconnect chồng nhau; không tự sửa sai mật khẩu hoặc lỗi bắt tay của router. `reason=15` là timeout bắt tay WPA 4 bước, chưa đủ để kết luận duy nhất là sai mật khẩu.
- Build ID lấy trực tiếp 8 byte đầu `app_elf_sha256` trong app descriptor và định dạng thành 16 ký tự hex. Không dùng `esp_app_get_elf_sha256()` cho trường này: SDK đang ghim giới hạn chuỗi sẵn có bằng `CONFIG_APP_RETRIEVE_LEN_ELF_SHA=9`, dù truyền buffer 17 byte.

## 6. Điểm tích hợp và kiểm thử

- `OtaService`: nhận raw binary tại `POST /update`, header `X-OTA-Password`, `Content-Type: application/octet-stream`, `Content-Length` bắt buộc. Giới hạn 4 MiB; không nhận multipart hoặc transfer encoding chunked. Dùng API `esp_ota_*` từ ESP-IDF, không thêm thư viện OTA bên thứ ba.
- `GET /api/v1/ota`: yêu cầu mật khẩu, trả `max_firmware_bytes`; dùng để kiểm tra trước upload.
- `OtaPage`: giao diện Soft Glass với nền pastel, thẻ kính mờ, nút indigo; hai cột trên desktop, một cột trên điện thoại. HTML/CSS/JavaScript và icon SVG nhúng trong flash, font hệ thống, không cần tài nguyên Internet. Hiển thị tên/dung lượng file đã chọn và phân biệt trạng thái đang tải/thành công/lỗi/chưa kết nối lại. Dùng XHR cho tiến trình upload, chỉ tự thử lại các GET health khi chờ reboot.
- `ApiServer`: thu thập header dùng chung một lần, thêm `ota_update` vào capabilities, `boot_id` vào health; gọi `OtaService::loop()` cả khi mất Wi-Fi để hoàn tất reboot đã lên lịch.
- Body JSON vẫn giới hạn 512 byte, XOR auth `X-API-Key`, kiểm tra NUL và nesting 3; lệnh IR vẫn được phát một lần/request.

Build kiểm tra: `pio run -e esp32-s3-n16r8`.

Kiểm thử host không truy cập mạch hoặc credential cục bộ:

```powershell
python tools/test-ota.py --cxx <duong-dan-g++.exe>
python tools/test-wifi.py --cxx <duong-dan-g++.exe>
python -m pip install playwright
python tools/test-ota-page.py --browser <duong-dan-chrome-hoac-msedge.exe>
```

Trên Windows có thể cài compiler host bằng `pio pkg install --global --tool platformio/toolchain-gccmingw32`, sau đó dùng `g++.exe` trong thư mục `bin` của package. Đây là công cụ kiểm thử, không cần để build hoặc sử dụng firmware bình thường.

Kiểm thử C++ chạy module OTA thật với HTTP/flash giả lập: mật khẩu, giới hạn, header chia nhỏ, upload thiếu/ngắt, lỗi ghi/xác minh, thứ tự chọn boot, reboot và file đúng ngưỡng 4 MiB. Test Wi-Fi chạy module thật với credential giả và driver mô phỏng đang bận, kiểm tra hủy trước reconnect, nhịp retry, online, lỗi driver và tràn millis. Kiểm thử Playwright dùng Chrome/Edge đã cài, chạy JavaScript trang thật với mạng giả lập ở viewport desktop 1440 px và điện thoại 390 px; kiểm tra tràn ngang, tên file dài, chờ VALID, rollback, sai build/slot và thiếu metadata. Có thể thêm `--screenshots <thu-muc>` để lưu ảnh giao diện ở trạng thái chờ/thành công. Chúng không thay thế kiểm thử radio Wi-Fi, flash và nguồn trên board.

Sau nạp USB lần đầu, cần kiểm tra trên mạch: log `rollback compile=enabled hook=1`, Wi-Fi có IP và dịch vụ 80/8081/443; mở trang; thử sai mật khẩu; OTA A→B→A với hai build nền Safe Update; đối chiếu build/slot/VALID và test lại Toshiba. Giữ USB phục hồi và Serial khi thử rollback có kiểm soát; chưa thử VPN trước khi LAN đạt. Không dùng firmware cũ thiếu hook Safety làm bản thử rollback. Không gửi lại POST tự động khi kết quả chưa rõ.

Tài liệu này đi cùng thay đổi OTA trong working tree. `PROJECT_SPEC.md` mô tả HEAD đã commit; chạy `$docs` sau khi commit tính năng để đồng bộ theo quy ước repo.

Tham khảo cơ chế OTA và phân vùng: [ESP-IDF OTA cho ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html), [Arduino ESP32 OTA Web Update](https://docs.espressif.com/projects/arduino-esp32/en/latest/ota_web_update.html).
