---
name: implement
description: Vai Coder - thực hiện docs/PLAN.md theo từng Step, build và test sau mỗi Step, tick tiến độ vào PLAN.md. Chỉ dùng khi người dùng gọi $implement.
---

# Implement (code theo plan)

Đối số: `$implement` (các Step còn lại) | `$implement 2` | `$implement 2-3`.
Tiến độ nằm trong `docs/PLAN.md` (ô `[ ]`/`[x]`), nên chat mới có thể làm tiếp mà không cần lịch sử chat cũ.

## Trước khi làm

1. Đọc `docs/PLAN.md`. Không có file thì dừng và bảo người dùng chạy `$architect`. Mục "Câu hỏi mở" còn chưa được trả lời thì dừng và hỏi.
2. Chọn Step: theo đối số, hoặc Step đầu tiên còn `[ ]`.
3. Đọc spec theo mục chỉ cho module trong Step (`python tools/spec.py <mục>`). Đọc toàn bộ file đích theo `AGENTS.md` §1.

## Mỗi Step

1. Làm đúng việc ghi trong Step, chỉ sửa các file liệt kê (cộng include/header bắt buộc). Cần sửa file ngoài plan thì dừng, nêu lý do và hỏi.
2. Chạy lệnh "Kiểm" của Step (lệnh build/test ở `AGENTS.md` §4).
3. Lỗi thì đọc lỗi gốc rồi sửa. Cùng một lỗi quá 2 lần chưa xong thì dừng và báo cáo, không sửa mò.
4. Pass thì đổi `[ ]` thành `[x]` và thêm dòng `  - Kết quả: <một dòng, ví dụ RAM/Flash hoặc số test pass>`.
5. Làm Step kế tiếp, trừ khi: Step cần thử trên mạch, Step cần quyết định ngoài plan, hoặc đã xong phạm vi đối số.

## Cấm

- Không commit, push, upload firmware.
- Không đổi chân GPIO, không thêm retry cho lệnh IR (`AGENTS.md` §2-§3).
- Không mở rộng phạm vi, không refactor ngoài plan. Điều đáng sửa nhưng ngoài plan thì ghi vào mục "Phát hiện thêm" ở cuối `docs/PLAN.md`.

## Kết thúc

Báo cáo tối đa 8 dòng: Step đã xong, kết quả build/test (kèm RAM/Flash nếu có), việc người dùng phải thử trên mạch, và gợi ý chạy `$reviewer` ở chat mới.
