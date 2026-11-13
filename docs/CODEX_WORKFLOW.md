# Quy trình dùng Codex cho KhaiRC-ESP32-Hub

Tài liệu này dành cho người dùng, không phải cho agent. Quy tắc cho agent nằm ở `AGENTS.md` và `.agents/skills/`.

## 1. Nguyên tắc

- Ngữ cảnh là tài nguyên khan hiếm: mỗi lượt Codex gửi lại toàn bộ ngữ cảnh, nên thứ luôn được nạp phải nhỏ, thứ lớn phải đọc theo mục.
- Mỗi sự thật nằm một nơi: `AGENTS.md` giữ bất biến, `docs/PROJECT_SPEC.md` giữ hiện trạng, skill giữ quy trình.
- Mỗi bước có kiểm chứng chạy được; cùng một lỗi sửa quá 2 lần thì dừng.

## 2. Các vai (mỗi vai một chat mới)

Đầu mỗi chat, gõ `/model` và `/reasoning` để chọn model và effort cho chat đó.

| Bước | Gõ | Model / effort | Kết quả |
|---|---|---|---|
| 1. Plan | `$architect <yêu cầu>` (việc nhỏ: `/plan`) | Hạng cao nhất, high | `docs/PLAN.md` |
| 2. Code | `$implement` (hoặc `$implement 2`, `$implement 2-3`) | Hạng Sol, medium | Code, build, test, Step được tick `[x]` |
| 3. Thử mạch | Tự làm theo mục "Thử trên mạch" của PLAN.md | | Log Serial |
| 4. Review | `$reviewer` (nhanh: `/review`) | Cao hơn model coder, medium | Báo cáo Blocker/Major/Minor |
| 5. Commit | Tự commit | | |
| 6. Docs | `$docs` | Hạng Luna, low | Spec đồng bộ HEAD |
| Lỗi lạ | `$debugger` + dán log Serial | Hạng cao nhất, high | Chẩn đoán, chưa sửa |

## 3. Chạy một tính năng từ đầu

1. Chat mới, chọn model mạnh + effort high. Gõ ví dụ: `$architect Thêm lệnh power_on/power_off cho daikin_ac qua IR`.
2. `$architect` tự tạo `docs/PLAN.md` (không cần tạo trước) rồi dừng và hỏi các câu hỏi mở.
3. Đọc `docs/PLAN.md`, sửa nếu cần, trả lời câu hỏi mở bằng cách ghi vào file hoặc nói trong chat.
4. Chat mới, model Sol + effort medium. Gõ `$implement`. Nó thực hiện liền mạch toàn bộ Step trong phạm vi, chỉ chạy kiểm chứng ở mục 5 của `docs/PLAN.md` và tick `[x]` một lần ở cuối. Dừng khi cần thử trên mạch, cần quyết định ngoài plan, hoặc lỗi lặp quá 2 lần.
5. Nạp firmware và thử trên mạch theo plan (agent không tự nạp).
6. Chat mới: `$reviewer`. Sửa các Blocker/Major (có thể bảo `$implement` hoặc sửa tay).
7. Tự commit, rồi chat mới chạy `$docs`.

Chat bị cắt giữa chừng: mở chat mới và gõ `$implement`; tiến độ nằm trong PLAN.md.

## 4. Tiết kiệm token

- Đọc spec theo mục: `python tools/spec.py` (mục lục), `python tools/spec.py 0 8 9 5.5`, hoặc theo từ khóa `python tools/spec.py OTA`. Đọc cả file là 42 KB, đọc `0 8 9` là 7,6 KB.
- `src/OtaPage.cpp` gần như toàn HTML nhúng; agent chỉ đọc phần C++ trừ khi sửa giao diện.
- Build ghi ra `.pio/build.log`, chỉ đọc 25 dòng cuối. `tool_output_token_limit = 10000` chặn output quá dài.
- Một việc một chat. Chat dài: `/compact`. Thử hướng khác: `/fork`. Hỏi nhanh không làm bẩn chat chính: `/side`. `/status` xem mức dùng ngữ cảnh và quota.
- `$docs` có thể quét lại cả repo khi nhiều module đổi. Gom các commit nhỏ của một tính năng rồi chạy một lần; `$docs check` cho biết nó sẽ làm gì mà không ghi file.

## 5. Chọn model (gói Plus)

Trang giá Codex ghi khoảng sử dụng Plus: hạng Luna khoảng 250-3.000, hạng Sol khoảng 10-160, GPT-6 Astra khoảng 5-45. Luna nhiều lượt hơn Sol khoảng 20 lần. Dùng hạng cao nhất cho Plan và Debug, Sol cho Code, Luna cho Docs và sửa vặt. Danh sách đổi theo thời gian: đối chiếu dropdown và `/status`.

## 6. Cấu hình

- `C:\Users\<tên>\.codex\config.toml`: `model_reasoning_effort = "medium"`, `plan_mode_reasoning_effort = "high"`, `tool_output_token_limit = 10000`, tùy chọn `model` và `review_model` (điền slug từ dropdown). Khóa toàn cục phải đứng trước mọi dòng `[bảng]`.
- Profile (`[profiles.*]`) chỉ có tác dụng với Codex CLI, không có tác dụng trong extension VS Code.
- `.gitignore` có `/docs/PLAN.md`: PLAN.md là file làm việc, không commit. Nếu bộ chọn `@` không thấy nó, gõ thẳng đường dẫn `docs/PLAN.md`.
- Nếu `pio run` báo lỗi quyền ghi vào `.platformio`, bỏ comment `[sandbox_workspace_write]` trong config và điền `writable_roots`. `/status` hiển thị các writable roots hiện có.

## 6.1 Xử lý sự cố nhanh

| Hiện tượng | Cách xử lý |
|---|---|
| Gõ `$` không thấy skill | Khởi động lại Codex; kiểm tra `.agents/skills/<tên>/SKILL.md` có `name` trùng tên thư mục |
| `pio` không nhận | Dùng `"$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"` |
| `$implement` báo không có PLAN.md | Chạy `$architect` trước |
| Chạy `python tools/spec.py` báo lỗi | Chạy từ gốc repo; cần Python 3.7+ |
| Log crash chỉ có địa chỉ `0x...` | Thêm `monitor_filters = esp32_exception_decoder` vào `platformio.ini` |

## 7. Lệnh trong extension VS Code

Có: `/model`, `/reasoning`, `/plan`, `/review`, `/compact`, `/fork`, `/side`, `/status`, `/goal`. Chat mới tạo bằng nút trên giao diện; skill gọi bằng cách gõ `$`.
