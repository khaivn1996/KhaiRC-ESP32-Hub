---
name: implement
description: Vai Coder - thực hiện liền mạch toàn bộ các Step được giao trong docs/PLAN.md, chỉ chạy build/test và tick tiến độ một lần ở cuối. Chỉ dùng khi người dùng gọi $implement.
effort: medium
---

# Implement (code theo plan - Silent Batch Mode)

Đối số: `$implement` (toàn bộ các Step còn `[ ]`) | `$implement 2` | `$implement 2-3`.
Tiến độ nằm trong `docs/PLAN.md` (ô `[ ]`/`[x]`), nên chat mới có thể làm tiếp mà không cần lịch sử chat cũ.

## Quy tắc Thực thi Im lặng & Tiết kiệm Token (Bắt buộc)

- **CẤM gọi Subagent:** Thực hiện 100% thao tác trực tiếp trên luồng agent chính. Không tạo subagent hay background task dưới mọi hình thức.
- **CẤM báo cáo trung gian (Zero Mid-Turn Commentary):** Khi thực hiện nhiều Step, gọi nối tiếp các công cụ đọc/sửa file cho đến khi xong toàn bộ các Step trong phạm vi. Tuyệt đối không xuất tin nhắn kiểu "Đã xong Step 1, chuyển sang Step 2" xen giữa các lần gọi tool.
- **Không đọc lặp Spec:** Mọi thông tin thiết kế đã có trong `docs/PLAN.md`. Không tự gọi lại `python tools/spec.py` trừ khi `PLAN.md` yêu cầu rõ.
- **Vùng cấm (`AGENTS.md` §1):** TUYỆT ĐỐI KHÔNG đọc `include/secrets.h`, `include/ota_password.h`. Không quét `.pio/`, `build/`, `logs/`, `.git/`, `.vscode/`, `backups/`, `*.patch`.

## Quy trình thực hiện (1 mạch duy nhất)

1. **Đọc & Kiểm tra điều kiện:**
   - Đọc `docs/PLAN.md`. Không có file thì dừng và bảo người dùng chạy `$architect`. Mục "Câu hỏi mở" còn chưa được trả lời thì dừng và hỏi.
   - Xác định danh sách các Step cần làm (theo đối số, hoặc tất cả các Step còn `[ ]`).
   - Đọc một lượt toàn bộ các file `.cpp`/`.h` đích cần sửa theo `AGENTS.md` §1 (chú ý ngoại lệ `src/OtaPage.cpp`).
2. **Sửa code liền mạch (Batch Edit):**
   - Lần lượt áp dụng chính xác thay đổi của tất cả các Step trong phạm vi vào mã nguồn.
   - Chỉ sửa các file liệt kê trong plan (cộng include/header bắt buộc). Cần sửa file ngoài plan hoặc phát hiện plan mâu thuẫn với code thực tế thì dừng ngay, nêu lý do và hỏi.
3. **Kiểm chứng gom lô 1 lần ở cuối (`AGENTS.md` §4):**
   - Sau khi đã sửa xong code của tất cả các Step trong phạm vi, mới chạy lệnh build/test ở mục "Kiểm chứng" của `docs/PLAN.md`. Plan sửa firmware: `pio run *> .pio/build.log; Get-Content .pio/build.log -Tail 25` và test host nếu có. Plan chỉ sửa Markdown/YAML: chỉ chạy đúng lệnh plan chỉ định, không `pio run`.
   - Nếu build firmware lỗi: đọc lỗi gốc (`Select-String .pio/build.log -Pattern 'error' | Select-Object -First 15`) rồi sửa. Cùng một lỗi quá 2 lần chưa xong thì dừng và báo cáo, không sửa mò.
4. **Cập nhật `docs/PLAN.md` 1 lần:**
   - Khi build/test đã pass, cập nhật tất cả các Step vừa hoàn thành từ `[ ]` thành `[x]` và ghi dòng kết quả (RAM/Flash, số test pass) vào `docs/PLAN.md` trong **đúng một thao tác ghi file duy nhất** (gồm cả mục "Phát hiện thêm" nếu có).

## Cấm

- Không commit, push, upload firmware, không chạy `pio device monitor`.
- Không đổi chân GPIO, không thêm retry cho lệnh IR (`AGENTS.md` §2-§3).
- Không mở rộng phạm vi, không refactor hay format lại code ngoài plan. Điều đáng sửa nhưng ngoài plan thì ghi vào mục "Phát hiện thêm" ở cuối `docs/PLAN.md` ngay trong thao tác ghi ở Quy trình bước 4 (không ghi thêm lần nữa).

## Kết thúc

Xuất **1 báo cáo duy nhất** (tối đa 8 dòng): các Step đã xong, kết quả build/test (kèm RAM/Flash nếu có), việc người dùng phải thử trên mạch, và gợi ý chạy `$reviewer` ở chat mới.