# Mở khóa Web UI bằng bản sao cloud

## 1. Phạm vi

Trang HTTPS `/ui/vault` đăng nhập Google bằng Firebase Auth popup, đọc duy nhất `users/{uid}/vault/main` trong Firestore `(default)`, giải mã trên trình duyệt rồi dùng `hub.apiKey` để POST `/web/login`. Sau khi Hub tạo cookie phiên, trang chuyển về `/ui`. Firmware không kết nối Firebase; các kết nối cloud do trình duyệt thực hiện.

Trang `/ui` vẫn hỗ trợ đăng nhập thủ công, CSP cũ, phiên 15 phút, CSRF và rate limit cũ. Cloud là tùy chọn; không có API ghi Firestore trong mã web này.

## 2. Cấu hình firmware

Copy `include/firebase_web_config.example.h` thành `include/firebase_web_config.h`, điền bốn macro từ cấu hình **Web app** của cùng Firebase project với app Android. File local được Git-ignore; không đưa cấu hình thật vào commit. Mỗi chuỗi dài 1..200 byte. `FIREBASE_AUTH_DOMAIN` là hostname, không có scheme/path; không tự đặt domain tùy ý vì nó được dùng trong CSP.

Thiếu file này, `/ui/vault` trả trang “Firmware chưa có cấu hình Firebase web”, không nạp script; `/web/vault.js` trả 404 `not_configured`.

## 3. Build bundle

Yêu cầu Node/npm tương thích với phiên bản SDK đã pin trong `web/vault/package.json` và `package-lock.json`.

```powershell
python tools/build-vault-web.py
python tools/test-vault-web.py
pio run *> .pio/build.log
Get-Content .pio/build.log -Tail 25
```

Tool chạy `npm ci`, bundle IIFE ES2020 bằng esbuild, gzip level 9 với mtime 0; dừng nếu gzip vượt 120 KiB. `include/VaultWebBundle.h` và `src/VaultWebBundle.cpp` là file sinh cần đưa vào commit cùng source và lockfile. Build firmware thông thường không cần Node. Không sửa trực tiếp mảng byte trong file sinh. `node_modules` được ignore.

## 4. Định dạng và mật mã

Doc có `schemaVersion=1`, `kdfAlg=PBKDF2-SHA256`, iterations 1..2.000.000, salt 16 byte, IV 12 byte, ciphertext kèm tag 16 byte (tổng 16..1.048.576 byte). PBKDF2 dùng password UTF-8, AES-256-GCM với AAD `khairc-vault-v1|uid`. Decoder đọc thứ tự TUYA/HUB/HTTPS/WOL/LABELS với cờ 0/1, modified UTF-8 của Java `writeUTF`; từ chối version lạ, byte thừa, nhóm cụt, count ngoài 0..6 và key LABELS trùng. Key Hub phải đúng 64 ký tự ASCII in được.

Không tự retry đọc vault hoặc POST login. Sai password/tag, lỗi format hay thiếu HUB không được POST login. Mọi lần thử tiếp theo do người dùng bấm. Sau lần mở khóa, password bị xóa khỏi input, buffer plaintext được ghi đè và Firebase sign-out; chuỗi JavaScript không thể bảo đảm xóa vật lý trước GC.

## 5. Test và KAT

Test Node dùng encoder modified UTF-8 độc lập và `node:crypto` để seal, WebCrypto để open. Có kiểm tra mật khẩu có dấu, AAD uid, ciphertext hỏng và giới hạn decoder. Đây chưa phải bằng chứng tương thích với Kotlin.

KAT thật từ repo app phải được người dùng tạo theo `docs/PLAN.md` §9 rồi đặt tại `web/vault/test/kat-app.json`, dạng `{password,uid,iterations,salt,iv,ct,expected:{hub:{ipv4,apiKey},labels:{...}}}`; các byte dùng Base64, chỉ dùng dữ liệu giả. Chưa có file thì test ghi rõ skip “chưa có KAT từ app”. Không tự tạo KAT bằng encoder JS. JVM KAT chưa kiểm chứng provider PBKDF2 trên Android; phải thử password tiếng Việt trên thiết bị thật.

## 6. Sử dụng

Mở `/ui/vault` bằng tab Safari/Chrome thường trên HTTPS được tin cậy, bấm “Đăng nhập Google”, nhập mật khẩu chủ rồi “Mở khóa”. Không dùng chế độ standalone/PWA; trang sẽ hướng dẫn mở tab thường. Trình duyệt phải hỗ trợ WebCrypto. Cookie phiên Hub vẫn HttpOnly/Secure/SameSite=Strict.

## 7. Thiết lập Firebase

1. Thêm **Web app** vào cùng Firebase project; copy config vào file local ở §2. Bật Google provider trong Authentication nếu chưa bật.
2. Authentication → Settings → Authorized domains: thêm địa chỉ truy cập Hub, ví dụ `192.168.2.200`. Nếu IP bị từ chối hoặc gặp `auth/unauthorized-domain`, dùng hostname với DNS override trên router và authorize hostname đó.
3. Google Cloud Console: giới hạn Browser API key bằng referrer, ví dụ `https://192.168.2.200/*`, và ba API Identity Toolkit, Token Service, Cloud Firestore. Nếu dùng hostname thì cập nhật referrer tương ứng.
4. Giữ rules của app: chỉ `request.auth.uid == uid` được đọc/ghi vault riêng. Mã web này chỉ dùng REST GET với Bearer idToken, không đổi rules. Project hiện không bật App Check; không gửi header App Check.
5. Tạo bản sao cloud có nhóm HUB từ app Android, đảm bảo key khớp Hub. Build lại firmware sau khi thêm cấu hình và tự upload khi sẵn sàng.

## 8. Giới hạn, rủi ro và thử trên mạch

Firebase Auth dùng `inMemoryPersistence`; mã ứng dụng không dùng localStorage/sessionStorage/IndexedDB. Popup resolver và các trang Google/Firebase bên ngoài có hành vi riêng; cần kiểm tra Application/Network trên trình duyệt thật. CSP rộng hơn chỉ áp dụng `/ui/vault`, cho `apis.google.com`, auth iframe và ba endpoint cloud; script bên ngoài bị xâm phạm có thể đọc password/key trong tab này. Không đặt COOP để tránh phá popup.

Popup có thể bị chặn bởi iOS hoặc third-party storage. Chưa có dữ liệu KAT hoặc chưa thử phần cứng thì không coi tương thích app/Android/Safari là đã xác minh. Nếu sau này bật App Check, Firestore có thể trả 403: cần plan riêng cho reCAPTCHA Enterprise, CSP và auth, không tự nới rules.

HTTPS giữ `max_open_sockets=2`, bundle gzip từ flash không cấp phát buffer lớn; tải có thể chậm. Người dùng thử toàn bộ `docs/PLAN.md` §7: route registration, đăng nhập thủ công, Google/password đúng, sai password không POST login, tài khoản chưa có backup, PWA, storage/network chỉ đọc, heap sau 5 lần tải và password tiếng Việt. Kiểm tra điều khiển Toshiba sau khi login; không tự upload hoặc tự phát IR trong test host.

Rollback: bỏ hai route vault, trả `max_uri_handlers` về 15 và bỏ source/bundle vault; luồng đăng nhập thủ công vẫn giữ nguyên.
