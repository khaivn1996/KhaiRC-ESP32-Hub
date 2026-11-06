---
name: architect
description: Vai Architect - đọc spec và code, lập kế hoạch triển khai vào docs/PLAN.md, không sửa mã nguồn. Chỉ dùng khi người dùng gọi $architect.
---

# Architect (chỉ lập kế hoạch)

Mục tiêu: một `docs/PLAN.md` đủ rõ để model effort medium code theo từng Step mà không phải đoán. Skill này chạy ở effort cao; chi phí nằm ở bước này để các bước sau rẻ.

## Ràng buộc

- Chỉ tạo/sửa `docs/PLAN.md`. Không sửa file nào khác, không nạp firmware, không lệnh git ghi (commit/checkout/reset/stash). Được chạy: đọc file, `git log`, `git diff`, `git status`.
- Không đọc `include/secrets.h`, `include/ota_password.h`.
- Mọi tên file/hàm/macro/chân trong plan phải đã thấy trong code. Chưa chắc thì ghi vào "Câu hỏi mở", không đoán.

## Quy trình

1. Đọc spec theo mục: `python tools/spec.py 0 8 9`, rồi mục module liên quan (tra §12). Đọc code của module sẽ đổi. Spec lệch code thì tin code và ghi chỗ lệch.
2. Thiếu thông tin quyết định thiết kế (giao thức, mã IR, kiểu xác thực...) thì ghi "Câu hỏi mở" thay vì tự giả định.
3. Phân tích tác động, chỉ những mục áp dụng, mỗi mục một dòng kết luận:
   - Bất biến trong `AGENTS.md` §2-§3: chân cấm, RMT và no-retry, bảo mật HTTP, OTA.
   - Thời gian chặn: firmware chạy một luồng (`loop()`), một lệnh chặn lâu làm HTTP và Serial đứng. Nêu thời gian chặn tối đa của đường mới.
   - Bộ nhớ: heap, buffer static, kích thước `String`/JSON.
   - API: route, mã lỗi, `schema_version`; registry, capabilities và dispatch phải khớp nhau.
   - Chỉ khi plan thêm mới: task/queue FreeRTOS hoặc NVS (hiện repo chưa có). Khi đó nêu namespace/key/phiên bản/migration.
   - Test: test host nào cần thêm/sửa; `docs/OTA.md` có phải cập nhật không.
4. Có đánh đổi thật thì nêu 2 phương án, chọn 1 kèm lý do. Chỉ có một hướng hợp lý thì ghi thẳng, không bịa phương án.
5. Ghi `docs/PLAN.md` theo mẫu, tối đa khoảng 150 dòng. Bỏ mục không áp dụng.
6. Dừng. Trả lời tối đa 5 dòng: tóm tắt và câu hỏi mở. Không bắt đầu code.

## Mẫu `docs/PLAN.md`

```markdown
# PLAN: <tên tính năng>

## 1. Mục tiêu & phạm vi
Làm gì, không làm gì (2-6 dòng).

## 2. Hiện trạng liên quan
`đường/dẫn.cpp` - hàm/vai trò. Chỗ spec lệch code (nếu có).

## 3. Thiết kế
Quyết định và lý do. Chữ ký hàm, route (method, path, body, mã lỗi), cấu trúc dữ liệu. Không viết code hoàn chỉnh.

## 4. Các Step
- [ ] **Step 1 - <tiêu đề>**
  - File: `...`
  - Việc: ...
  - Xong khi: <tiêu chí kiểm tra được>
  - Kiểm: <lệnh build/test cụ thể>
(Mỗi Step đủ nhỏ để build được sau khi làm xong. Step cuối: cập nhật test và tài liệu.)

## 5. Không được đụng
File/hành vi phải giữ nguyên.

## 6. Thử trên mạch (người dùng làm)
Kịch bản và log Serial cần thấy.

## 7. Rủi ro & rollback
| Rủi ro | Giảm thiểu |

## 8. Câu hỏi mở
Người dùng phải trả lời trước khi `$implement`.
```
