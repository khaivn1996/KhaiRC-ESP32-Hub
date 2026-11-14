#include <Arduino.h>
#include "OtaPage.h"

namespace {

const char page[] PROGMEM = R"OTAHTML(<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KhaiRC Hub · Cập nhật firmware</title>
<!-- Reuse .brand-mark geometry, rounded corners and gradient for the tab icon. -->
<link rel="icon" type="image/svg+xml" sizes="any" href="data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20viewBox%3D%22-6%20-5%2042%2042%22%3E%3Cdefs%3E%3ClinearGradient%20id%3D%22brand%22%20x1%3D%220%25%22%20y1%3D%220%25%22%20x2%3D%220%25%22%20y2%3D%22100%25%22%3E%3Cstop%20stop-color%3D%22%237262ff%22%2F%3E%3Cstop%20offset%3D%221%22%20stop-color%3D%22%234738ce%22%2F%3E%3C%2FlinearGradient%3E%3C%2Fdefs%3E%3Cg%20fill%3D%22url%28%23brand%29%22%3E%3Crect%20width%3D%2211%22%20height%3D%2227%22%20rx%3D%223%22%20transform%3D%22translate%282%204.29707%29%20skewY%28-38%29%22%2F%3E%3Crect%20width%3D%2211%22%20height%3D%2227%22%20rx%3D%223%22%20transform%3D%22translate%2816%209.29707%29%20skewY%28-38%29%22%2F%3E%3C%2Fg%3E%3C%2Fsvg%3E">
<style>
:root{color-scheme:light;--ink:#24204f;--muted:#615d83;--accent:#5145e5;--line:rgba(255,255,255,.85)}
*{box-sizing:border-box}[hidden]{display:none!important}
body{margin:0;min-height:100vh;padding:48px 24px;background:#e8eafa;color:var(--ink);font:15px/1.6 "Segoe UI",system-ui,sans-serif;
background-image:radial-gradient(ellipse at 6% 12%,#bcd5ff 0,transparent 48%),radial-gradient(ellipse at 96% 90%,#d9c6fa 0,transparent 48%),radial-gradient(ellipse at 65% 0%,#eee6ff 0,transparent 55%);background-attachment:fixed}
main{max-width:1080px;margin:0 auto;padding:36px 48px 26px;border:1px solid var(--line);border-radius:36px;background:rgba(255,255,255,.48);box-shadow:0 24px 80px #4c469b13,inset 0 1px 0 #fff}
@supports(backdrop-filter:blur(20px)){main{background:rgba(255,255,255,.34);backdrop-filter:blur(20px)}}
svg{width:22px;height:22px;flex-shrink:0;fill:none;stroke:currentColor;stroke-width:1.7;stroke-linecap:round;stroke-linejoin:round}
.topbar{display:flex;justify-content:center}
.brand-pill{display:flex;align-items:center;justify-content:center;gap:22px;flex-wrap:wrap;padding:12px 22px;border:1px solid var(--line);border-radius:999px;background:#ffffff59;box-shadow:inset 0 1px 1px #fff9}
.brand{display:flex;align-items:center;gap:11px;font-size:19px;font-weight:800;letter-spacing:-.5px;color:#28206f}
.brand-mark{position:relative;display:inline-block;width:30px;height:32px}
.brand-mark:before,.brand-mark:after{content:"";position:absolute;width:11px;height:27px;border-radius:3px;transform:skewY(-38deg);background:linear-gradient(180deg,#7262ff,#4738ce)}
.brand-mark:before{left:2px;top:0}.brand-mark:after{left:16px;top:5px}
.board{font-weight:650;border-left:1px solid #b6b2d2;padding-left:22px;letter-spacing:.02em}
.connection{display:flex;align-items:center;gap:8px;padding:5px 12px;border-radius:30px;background:#e6e4f4;color:#625d81;font-size:12px;font-weight:650}
.connection:before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor}
.connection[data-state=online]{color:#17643d;background:#daf5e5}
.hero{text-align:center;margin:40px 0 30px}
.eyebrow{margin:0 0 8px;color:#6b619b;letter-spacing:.19em;font-size:10px;font-weight:700}
h1{margin:0;font-size:clamp(28px,4.4vw,49px);font-weight:750;line-height:1.2;letter-spacing:-1.7px;color:#24205c}
.subtitle{margin:12px 0 20px;font-size:17px;color:var(--muted)}
.version{display:inline-flex;align-items:center;justify-content:center;gap:12px;flex-wrap:wrap;padding:7px 18px;border:1px solid #ffffff96;border-radius:30px;background:#ffffff45;color:var(--muted);font-size:13px}
.version strong{font-size:14px;color:#342b80;font-weight:700}
.uptime{border-left:1px solid #c8c3e2;padding-left:12px;font-size:12px}
.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}
.form-card{min-width:0;padding:23px;border:1px solid var(--line);border-radius:23px;background:linear-gradient(135deg,#ffffff75,#ffffff36);box-shadow:inset 0 1px 0 #fff9}
.field-heading{display:flex;align-items:center;gap:9px;margin:0 0 18px;font-size:17px;font-weight:700;letter-spacing:-.3px}
.step{display:inline-flex;align-items:center;justify-content:center;width:23px;height:23px;border:1px solid #c9c3ed;border-radius:8px;background:#ffffff50;color:#675bb6;font-size:10px;font-weight:700}
.password-wrap{display:flex;align-items:center;gap:12px;padding:0 16px;margin-top:26px;border:1px solid #c8c4e2;border-radius:13px;background:#ffffff8c;color:#77709f;transition:box-shadow .15s,border-color .15s}
.password-wrap:focus-within{border-color:var(--accent);box-shadow:0 0 0 3px #5145e51a}
input[type=password]{min-width:0;width:100%;height:55px;border:0;outline:0;background:transparent;color:var(--ink);font:inherit;letter-spacing:.02em}
input::placeholder{color:#77718e;font-size:13px;letter-spacing:0}
.help{margin:12px 0 0;font-size:12px;color:var(--muted)}
.file-picker{position:relative;display:flex;min-width:0;min-height:142px;flex-direction:column;align-items:center;justify-content:center;gap:5px;padding:16px;border:1px dashed #aaa1e3;border-radius:15px;background:#ffffff42;text-align:center;cursor:pointer;transition:background .15s,border-color .15s}
.file-picker:hover{background:#ffffff80;border-color:var(--accent)}
.file-picker:focus-within{outline:3px solid #5145e54d;outline-offset:3px}
.file-picker>svg{width:31px;height:31px;margin-bottom:3px;color:var(--accent)}
.file-picker input{position:absolute;inset:0;width:100%;height:100%;opacity:0;cursor:pointer}
.file-picker strong{max-width:100%;font-size:14px;font-weight:650;overflow-wrap:anywhere}
.file-picker small{color:var(--muted);font-size:12px}
.file-picker.has-file{border-style:solid;border-color:#a59bdc;background:#ffffff70}
button{display:flex;align-items:center;justify-content:center;gap:11px;width:100%;min-height:58px;margin-top:22px;padding:15px 22px;border:1px solid #ffffff6b;border-radius:15px;background:linear-gradient(110deg,#6655f4,#4838d6);box-shadow:0 7px 20px #5945d52b,inset 0 1px 0 #ffffff38;color:white;font:650 16px "Segoe UI",system-ui,sans-serif;cursor:pointer;transition:box-shadow .15s,transform .15s}
button:hover:not(:disabled){transform:translateY(-1px);box-shadow:0 10px 25px #5945d540}
button:focus-visible,a:focus-visible{outline:3px solid #8374e8;outline-offset:4px}
button:disabled{opacity:.7;cursor:wait}
form[data-busy=true] .file-picker{opacity:.65;pointer-events:none}
.button-caption{margin:10px 0 0;text-align:center;font-size:11px;color:var(--muted)}
.transfer{margin-top:22px}.progress-caption{display:flex;justify-content:space-between;font-size:12px;color:var(--muted);margin-bottom:6px}
progress{display:block;appearance:none;width:100%;height:6px;border:0;border-radius:9px;overflow:hidden;background:#dcd8f0;accent-color:var(--accent)}
progress::-webkit-progress-bar{background:#dcd8f0}progress::-webkit-progress-value{background:linear-gradient(90deg,#8a79ff,#5145e5);border-radius:9px}progress::-moz-progress-bar{background:#5145e5;border-radius:9px}
.feedback{display:flex;align-items:center;gap:14px;padding:18px 22px;margin-top:20px;border:1px solid var(--line);border-radius:17px;background:#f4f2ffb3;color:#403779}
.status-icon{width:30px;height:30px;display:flex;align-items:center;justify-content:center;flex-shrink:0;border-radius:50%;background:#e3def7;font-weight:700}
.feedback[data-state=working] .status-icon{border:2px solid #d2cbef;border-top-color:#6150dc;background:transparent;animation:spin 1s linear infinite}
.feedback[data-state=success]{background:linear-gradient(110deg,#d8f5e9c9,#e9f8f1a6);color:#18593e}.feedback[data-state=success] .status-icon{background:#239b61;color:white}
.feedback[data-state=error]{background:#fff0f1c9;color:#84364c}.feedback[data-state=error] .status-icon{background:#f8dbe2}
.feedback[data-state=warning]{background:#fff7e6c9;color:#785a22}.feedback[data-state=warning] .status-icon{background:#f6e9c8}
#status{margin:0;font-size:13px;white-space:pre-line;overflow-wrap:anywhere}
footer{display:flex;justify-content:space-between;align-items:center;gap:20px;margin-top:26px;padding-top:18px;border-top:1px solid #ffffff8c;font-size:12px;color:var(--muted)}
.power-note{display:flex;align-items:center;gap:8px;margin:0}.power-note svg{width:17px;height:17px}
a{color:#51429e;text-underline-offset:3px;font-weight:600}footer a{white-space:nowrap}
.privacy{margin:13px 0 0;text-align:center;color:#746c8c;font-size:10px;line-height:1.7}
@keyframes spin{to{transform:rotate(360deg)}}
@media(min-width:1400px){body{padding-top:64px}}
@media(max-width:700px){body{padding:20px 12px}main{padding:25px 20px 20px;border-radius:27px}.brand-pill{gap:12px;padding:10px 14px}.brand{font-size:16px}.board{font-size:12px;padding-left:12px}.connection{font-size:11px}.hero{margin:30px 0 24px}h1{letter-spacing:-.9px}.subtitle{font-size:14px}.form-grid{grid-template-columns:1fr;gap:14px}.form-card{padding:20px;border-radius:19px}.password-wrap{margin-top:0}.field-heading{font-size:16px;margin-bottom:14px}button{font-size:14px;margin-top:18px}.feedback{padding:16px}footer{gap:12px;font-size:11px}.version{gap:9px}.uptime{font-size:11px}}
@media(max-width:420px){.brand-pill{border-radius:21px;gap:10px 15px}.connection{flex-basis:auto}.brand{font-size:15px}.brand-mark{transform:scale(.85);width:27px}.board{padding-left:15px}footer{align-items:flex-start;flex-direction:column;gap:10px}.uptime{border:0;padding:0;flex-basis:100%}.version{border-radius:16px;padding:7px 14px}h1{font-size:29px}}
@media(prefers-reduced-motion:reduce){*,*:before,*:after{animation:none!important;transition:none!important}}
</style>
</head>
<body><main aria-labelledby="page-title">
<header class="topbar">
<div class="brand-pill">
<div class="brand"><span class="brand-mark" aria-hidden="true"></span>KHAIRC HUB</div>
<span class="board">ESP32-S3</span>
<span class="connection" id="connection" data-state="checking" role="status">Đang kết nối</span>
</div>
</header>
<section class="hero">
<p class="eyebrow">CẬP NHẬT QUA WI-FI</p>
<h1 id="page-title">Cập nhật firmware</h1>
<p class="subtitle">Không dây. Ngay trên mạng của bạn.</p>
<div class="version"><span>Phiên bản hiện tại</span><strong id="version">—</strong><span class="uptime" id="device">Đang đọc thông tin…</span></div>
</section>
<form id="upload-form" data-busy="false">
<div class="form-grid">
<section class="form-card">
<label class="field-heading" for="password"><span class="step" aria-hidden="true">01</span>Mật khẩu OTA</label>
<div class="password-wrap">
<svg aria-hidden="true" viewBox="0 0 24 24"><rect x="5" y="10" width="14" height="11" rx="3"/><path d="M8 10V7a4 4 0 0 1 8 0v3M12 14v3"/></svg>
<input id="password" type="password" autocomplete="off" maxlength="128" required aria-describedby="password-help" placeholder="Nhập mật khẩu của bạn">
</div>
<p class="help" id="password-help">Dùng mật khẩu OTA đã cấu hình trên mạch.</p>
</section>
<section class="form-card">
<label class="field-heading" for="firmware"><span class="step" aria-hidden="true">02</span>Firmware mới</label>
<label class="file-picker" id="file-picker" for="firmware">
<svg aria-hidden="true" viewBox="0 0 24 24"><path d="M12 16V3m-5 5 5-5 5 5M4 16v4a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-4"/></svg>
<strong id="file-name">Chọn file firmware</strong>
<small id="file-detail">.bin · Tối đa 4 MiB</small>
<input id="firmware" type="file" accept=".bin" required aria-label="Chọn file firmware .bin" aria-describedby="file-detail file-help">
</label>
<p class="help" id="file-help">Chọn firmware.bin được build cho KhaiRC ESP32-S3.</p>
</section>
</div>
<button id="upload" type="submit">
<svg aria-hidden="true" viewBox="0 0 24 24"><path d="M12 16V3m-5 5 5-5 5 5M4 16v4a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-4"/></svg>
<span id="button-label">Upload và khởi động lại</span>
</button>
<p class="button-caption">Mạch sẽ tự khởi động lại sau khi xác minh firmware.</p>
</form>
<div class="transfer" id="transfer" hidden>
<div class="progress-caption"><span>Tiến trình gửi file</span><span id="percent">0%</span></div>
<progress id="progress" max="100" value="0" aria-label="Tiến trình gửi firmware"></progress>
</div>
<div class="feedback" id="feedback" data-state="working" role="status" aria-live="polite" hidden>
<span class="status-icon" id="status-icon" aria-hidden="true"></span><p id="status"></p>
</div>
<footer>
<p class="power-note"><svg aria-hidden="true" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7h.01"/></svg>Giữ nguồn ổn định khi cập nhật.</p>
<a href="/api/v1/health" target="_blank" rel="noopener">Trạng thái mạch <span aria-hidden="true">↗</span></a>
</footer>
<p class="privacy">Chỉ sử dụng trong mạng LAN tin cậy. Kết nối HTTP không mã hóa mật khẩu.</p>
<noscript><p>Cần bật JavaScript để upload firmware.</p></noscript>
</main>
<script>
'use strict';
const form = document.getElementById('upload-form');
const password = document.getElementById('password');
const firmware = document.getElementById('firmware');
const button = document.getElementById('upload');
const progress = document.getElementById('progress');
const status = document.getElementById('status');
const device = document.getElementById('device');
const version = document.getElementById('version');
const connection = document.getElementById('connection');
const feedback = document.getElementById('feedback');
const transfer = document.getElementById('transfer');
const percentLabel = document.getElementById('percent');
const filePicker = document.getElementById('file-picker');
let busy = false;
function showStatus(text, state = 'working') {
  status.textContent = text;
  feedback.dataset.state = state;
  feedback.hidden = false;
  document.getElementById('status-icon').textContent = state === 'success' ? '✓' : state === 'working' ? '' : '!';
}
function showFile() {
  const file = firmware.files[0];
  filePicker.classList.toggle('has-file', Boolean(file));
  document.getElementById('file-name').textContent = file ? file.name : 'Chọn file firmware';
  document.getElementById('file-detail').textContent = file
    ? (file.size / 1048576).toLocaleString('vi-VN', {maximumFractionDigits: 2}) + ' MiB · Tối đa 4 MiB'
    : '.bin · Tối đa 4 MiB';
}
firmware.addEventListener('change', showFile);
const errors = {
  unauthorized: 'Mật khẩu OTA không đúng.',
  ota_not_configured: 'OTA chưa được bật. Hãy đặt HUB_OTA_PASSWORD trong include/ota_password.h, build và nạp USB lần đầu.',
  ota_partition_unavailable: 'Mạch chưa có phân vùng OTA phù hợp. Cần nạp USB với cấu hình phân vùng của project.',
  reboot_pending: 'Mạch đang chuẩn bị khởi động lại. Hãy chờ rồi mở lại trang.',
  firmware_too_large: 'File vượt giới hạn dung lượng OTA.',
  invalid_firmware_size: 'File rỗng, quá ngắn hoặc khai báo dung lượng không hợp lệ.',
  invalid_firmware_image: 'File không phải firmware ứng dụng ESP32-S3 hợp lệ có SHA-256. Hãy chọn firmware.bin của project.',
  firmware_verification_failed: 'Kiểm tra firmware thất bại. Hãy build lại và chọn đúng firmware.bin.',
  ota_begin_failed: 'Không thể bắt đầu ghi flash. Hãy kiểm tra nguồn điện và cấu hình phân vùng.',
  ota_write_failed: 'Ghi flash thất bại. Firmware hiện tại vẫn được chọn để khởi động.',
  ota_boot_selection_failed: 'Không thể chọn firmware mới để khởi động.',
  upload_timeout: 'Upload vượt thời gian cho phép (120 giây).',
  incomplete_upload: 'Chưa nhận đủ firmware.',
  firmware_size_mismatch: 'Dung lượng nhận được không khớp với file.',
  unsupported_media_type: 'Định dạng upload không hợp lệ. Hãy dùng trang này để chọn file .bin.'
};
function message(code, httpStatus) {
  return errors[code] || ('Yêu cầu thất bại (HTTP ' + httpStatus + ', ' + (code || 'unknown') + ').');
}
async function getJson(url, headers = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 5000);
  try {
    const response = await fetch(url, {headers, cache: 'no-store', signal: controller.signal});
    const data = await response.json();
    if (!response.ok) throw new Error(message(data.error && data.error.code, response.status));
    return data;
  } finally { clearTimeout(timer); }
}
function showHealth(data) {
  version.textContent = data.firmware;
  device.textContent = 'Hoạt động: ' + data.uptime_sec + ' giây';
  connection.textContent = 'Đã kết nối';
  connection.dataset.state = 'online';
}
function setBusy(value) {
  busy = value;
  button.disabled = password.disabled = firmware.disabled = value;
  form.dataset.busy = String(value);
  document.getElementById('button-label').textContent = value ? 'Đang cập nhật…' : 'Upload và khởi động lại';
}
function sendFirmware(file, key) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update');
    xhr.setRequestHeader('Content-Type', 'application/octet-stream');
    xhr.setRequestHeader('X-OTA-Password', key);
    xhr.timeout = 150000;
    xhr.upload.onprogress = event => {
      if (!event.lengthComputable) return;
      const percent = Math.round(event.loaded * 100 / event.total);
      progress.value = percent;
      percentLabel.textContent = percent + '%';
      showStatus(percent < 100 ? 'Đang upload: ' + percent + '%' : 'Đã gửi file. Đang chờ mạch xác minh firmware…');
    };
    xhr.onload = () => {
      let data;
      try { data = JSON.parse(xhr.responseText); }
      catch (_) { reject(new Error('Phản hồi không hợp lệ. Hãy kiểm tra trạng thái mạch trước khi upload lại.')); return; }
      if (xhr.status === 200 && data.status === 'updated') resolve();
      else reject(new Error(message(data.error && data.error.code, xhr.status)));
    };
    xhr.onerror = xhr.ontimeout = xhr.onabort = () => reject(new Error(
      'Kết nối bị gián đoạn hoặc hết thời gian. Chưa thể xác nhận kết quả. Chờ mạch khởi động, kiểm tra trạng thái rồi quyết định upload lại.'));
    xhr.send(file);
  });
}
async function waitForReboot(before, expectedBuild) {
  // Only retry read-only health checks. Never automatically resend firmware.
  for (let attempt = 0; attempt < 60; ++attempt) {
    await new Promise(resolve => setTimeout(resolve, 2000));
    try {
      const health = await getJson('/api/v1/health');
      if (health.status === 'ok' && health.boot_id && health.boot_id !== before.boot_id &&
          health.build_id === expectedBuild && health.ota_state === 'valid' &&
          ['app0', 'app1'].includes(health.ota_slot) && health.ota_slot !== before.ota_slot) {
        showHealth(health);
        return true;
      }
    } catch (_) { /* Expected while the board reboots/reconnects. */ }
  }
  return false;
}
form.addEventListener('submit', async event => {
  event.preventDefault();
  if (busy) return;
  const file = firmware.files[0];
  if (!file || !file.name.toLowerCase().endsWith('.bin') || file.size === 0 || file.size > 4194304) {
    showStatus('Hãy chọn file .bin không rỗng, tối đa 4 MiB (4.194.304 byte).', 'error');
    return;
  }
  if (!/^[\x21-\x7e]{1,128}$/.test(password.value)) {
    showStatus('Mật khẩu cần 1–128 ký tự ASCII hiển thị, không có khoảng trắng, đúng như cấu hình trên mạch.', 'error');
    return;
  }
  setBusy(true);
  transfer.hidden = false;
  progress.value = 0;
  percentLabel.textContent = '0%';
  showStatus('Đang kiểm tra mật khẩu và dung lượng…');
  try {
    const info = await getJson('/api/v1/ota', {'X-OTA-Password': password.value});
    if (file.size > info.max_firmware_bytes) throw new Error('Phân vùng OTA trên mạch chỉ nhận tối đa ' + info.max_firmware_bytes + ' byte.');
    const before = await getJson('/api/v1/health');
    if (!before.boot_id || !/^[0-9a-f]{16}$/.test(before.build_id || '') ||
        !['app0', 'app1'].includes(before.ota_slot)) {
      throw new Error('Firmware hiện tại thiếu thông tin OTA Safety. Cần nạp bản nền qua USB trước.');
    }
    // ESP-IDF image: 24-byte image header + 8-byte segment header;
    // app descriptor ELF SHA-256 starts at offset 144 within the descriptor.
    // Match the same first 8 hash bytes exposed by OtaSafety::buildId().
    const hash = new Uint8Array(await file.slice(176, 184).arrayBuffer());
    if (hash.length !== 8) throw new Error('File firmware không đủ thông tin build.');
    const expectedBuild = Array.from(hash, byte => byte.toString(16).padStart(2, '0')).join('');
    await sendFirmware(file, password.value);
    password.value = '';
    progress.value = 100;
    percentLabel.textContent = '100%';
    connection.textContent = 'Đang khởi động';
    connection.dataset.state = 'checking';
    showStatus('Firmware đã được xác minh. Đang chờ mạch khởi động lại…');
    if (await waitForReboot(before, expectedBuild)) {
      showStatus('Cập nhật thành công. Đúng bản firmware đã chạy ở phân vùng mới và được xác nhận VALID.', 'success');
      firmware.value = '';
      showFile();
    } else {
      connection.textContent = 'Chưa kết nối lại';
      showStatus('Firmware đã được ghi và xác minh, nhưng chưa xác nhận đúng bản mới ở trạng thái VALID; có thể chưa thấy mạch kết nối lại hoặc đã rollback. Kiểm tra Serial, nguồn, Wi-Fi và IP trên router; không tự động upload lại.', 'warning');
    }
  } catch (error) {
    showStatus(error.name === 'AbortError' ? 'Mạch không phản hồi đúng hạn. Kiểm tra kết nối Wi-Fi rồi thử lại.' : error.message, 'error');
  } finally {
    password.value = '';
    setBusy(false);
  }
});
window.addEventListener('beforeunload', event => {
  if (busy) { event.preventDefault(); event.returnValue = ''; }
});
getJson('/api/v1/health').then(showHealth).catch(() => {
  device.textContent = 'Chưa đọc được trạng thái mạch.';
  connection.textContent = 'Chưa kết nối';
});
</script>
</body></html>)OTAHTML";

}

namespace OtaPage {

const char* html() {
    return page;
}

}
