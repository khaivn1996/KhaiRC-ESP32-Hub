---
name: architect
description: Vai Architect - đọc spec và code, lập kế hoạch triển khai vào docs/PLAN.md, không sửa mã nguồn. Chỉ dùng khi người dùng gọi $architect.
---

# Architect (chỉ lập kế hoạch)

Mục tiêu: tạo `docs/PLAN.md` đủ chi tiết (Zero-Discovery) để bước `$implement` (trên Codex hoặc ChatGPT Web) code thẳng một mạch mà không phải đoán hay quét tìm lại codebase. Skill này chạy ở effort cao; chi phí nằm ở bước này để các bước sau rẻ.

## Ràng buộc

- **Thực thi đơn luồng & im lặng:** Không gọi subagent (Task/Explore agent). Không xuất bình luận trung gian khi đang đọc file; chỉ trả lời một lần khi đã ghi xong `docs/PLAN.md`.
- Chỉ tạo/sửa `docs/PLAN.md`. Không sửa file nào khác, không nạp firmware, không lệnh git ghi (commit/checkout/reset/stash). Được chạy: đọc file, `git log`, `git diff`, `git status`.
- **Vùng cấm đọc/quét (`AGENTS.md` §1):** TUYỆT ĐỐI KHÔNG đọc `include/secrets.h`, `include/ota_password.h`. Không đọc/quét `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`, `backups/`, `*.patch`.
- Mọi tên file/hàm/macro/chân trong plan phải đã thấy trong code. Chưa chắc thì ghi vào "Câu hỏi mở", không đoán.

## Quy trình

1. Đọc spec theo mục: `python tools/spec.py 0 8 9`, rồi mục module liên quan (tra §12). Đọc toàn bộ file `.cpp`/`.h` của module sẽ đổi (ngoại lệ `src/OtaPage.cpp` theo `AGENTS.md` §1). Spec lệch code thì tin code và ghi chỗ lệch.
2. Thiếu thông tin quyết định thiết kế (giao thức, mã IR, kiểu xác thực...) thì ghi "Câu hỏi mở" thay vì tự giả định.
3. Phân tích tác động, chỉ những mục áp dụng, mỗi mục một dòng kết luận:
   - Bất biến trong `AGENTS.md` §2-§3: chân cấm, RMT và no-retry, bảo mật HTTP, OTA.
   - Thời gian chặn: firmware chạy một luồng (`loop()`), một lệnh chặn lâu làm HTTP và Serial đứng. Nêu thời gian chặn tối đa của đường mới.
   - Bộ nhớ: heap, buffer static, kích thước `String`/JSON.
   - API: route, mã lỗi, `schema_version`; registry, capabilities và dispatch phải khớp nhau.
   - Chỉ khi plan thêm mới: task/queue FreeRTOS hoặc NVS (hiện repo chưa có). Khi đó nêu namespace/key/phiên bản/migration.
   - Test: test host nào cần thêm/sửa; `docs/OTA.md` có phải cập nhật không.
4. Có đánh đổi thật thì nêu 2 phương án, chọn 1 kèm lý do. Chỉ có một hướng hợp lý thì ghi thẳng, không bịa phương án.
5. Ghi `docs/PLAN.md` theo mẫu dưới đây, tối đa khoảng 160 dòng. Bỏ mục không áp dụng.
6. Dừng. Trả lời tối đa 5 dòng: tóm tắt và câu hỏi mở. Không bắt đầu code.

## Mẫu `docs/PLAN.md`

```markdown
# PLAN: <tên tính năng>

> **Execution Directives (Cho `$implement`):** Thực thi liền mạch toàn bộ các Step được giao trong một lượt duy nhất (Silent Batch). CẤM gọi subagent, CẤM đọc lại `tools/spec.py` hoặc quét file ngoài danh sách. CẤM báo cáo tiến độ hay chạy build sau từng Step lẻ; chỉ chạy build/test và tick `[x]` vào `docs/PLAN.md` đúng 1 lần sau khi đã sửa xong toàn bộ code.

## 1. Mục tiêu & phạm vi
Làm gì, không làm gì (2-6 dòng).

## 2. Hiện trạng & Ngữ cảnh trích xuất (Zero-Discovery Context)
- **File trong phạm vi sửa:** liệt kê chính xác đường dẫn `.h`/`.cpp`.
- **Code/Chữ ký hàm liên quan:** trích xuất ngắn gọn chữ ký hàm, biến trong anonymous namespace hoặc đoạn code đích kèm số dòng để bước `$implement` đối chiếu trực tiếp. Chỗ spec lệch code (nếu có).

## 3. Thiết kế
Quyết định và lý do. Chữ ký hàm, route (method, path, body, mã lỗi), cấu trúc dữ liệu. Không viết code hoàn chỉnh.

## 4. Các Step thực thi (Sửa xong toàn bộ mới chạy Kiểm chứng ở mục 5)
- [ ] **Step 1 - <tiêu đề>**
  - File: `...` (dòng ~...)
  - Việc: ...
  - Xong khi: <tiêu chí kiểm tra được trong code>
- [ ] **Step 2 - <tiêu đề>**
  - File: `...`
  - Việc: ...
  - Xong khi: ...
(Step cuối: cập nhật test host và tài liệu nếu có.)

## 5. Kiểm chứng gom lô (Chạy 1 lần sau khi hoàn tất các Step)
- Lệnh build/test cụ thể theo `AGENTS.md` §4: `...`
- Kết quả: (để `$implement` điền sau khi chạy)

## 6. Không được đụng
File/hành vi phải giữ nguyên.

## 7. Thử trên mạch (người dùng làm)
Kịch bản và log Serial cần thấy.

## 8. Rủi ro & rollback
| Rủi ro | Giảm thiểu |

## 9. Câu hỏi mở
Người dùng phải trả lời trước khi `$implement`.
```