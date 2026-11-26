#include <Arduino.h>
#include "LandingPage.h"

namespace {

// Static endpoint catalog: update the lists below whenever a route is added or changed.
const char page[] PROGMEM = R"LANDINGHTML(<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KhaiRC Hub</title>
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
.connection[data-state=offline]{color:#84364c;background:#fbe3e8}
.hero{text-align:center;margin:40px 0 30px}
.eyebrow{margin:0 0 8px;color:#6b619b;letter-spacing:.19em;font-size:10px;font-weight:700}
h1{margin:0;font-size:clamp(28px,4.4vw,49px);font-weight:750;line-height:1.2;letter-spacing:-1.7px;color:#24205c}
.subtitle{margin:12px 0 20px;font-size:17px;color:var(--muted)}
.version{display:inline-flex;align-items:center;justify-content:center;gap:12px;flex-wrap:wrap;padding:7px 18px;border:1px solid #ffffff96;border-radius:30px;background:#ffffff45;color:var(--muted);font-size:13px}
.version strong{font-size:14px;color:#342b80;font-weight:700}
.uptime{border-left:1px solid #c8c3e2;padding-left:12px;font-size:12px}
.card{min-width:0;padding:23px;border:1px solid var(--line);border-radius:23px;background:linear-gradient(135deg,#ffffff75,#ffffff36);box-shadow:inset 0 1px 0 #fff9}
.shortcuts{display:grid;grid-template-columns:repeat(3,1fr);gap:20px}
.shortcut{display:flex;align-items:flex-start;gap:14px;color:var(--ink);text-decoration:none;font-weight:400;transition:transform .15s,box-shadow .15s}
a.shortcut:hover{transform:translateY(-1px);box-shadow:0 10px 25px #5945d51f,inset 0 1px 0 #fff9}
.shortcut .icon{display:flex;align-items:center;justify-content:center;width:42px;height:42px;flex-shrink:0;border-radius:13px;background:linear-gradient(110deg,#6655f4,#4838d6);color:#fff;box-shadow:0 7px 20px #5945d52b}
.shortcut strong{display:block;font-size:16px;font-weight:700;letter-spacing:-.3px;color:#28206f}
.shortcut small{display:block;margin-top:2px;font-size:12px;color:var(--muted);overflow-wrap:anywhere}
.section-title{display:flex;align-items:center;gap:9px;margin:34px 0 14px;font-size:17px;font-weight:700;letter-spacing:-.3px}
.step{display:inline-flex;align-items:center;justify-content:center;width:23px;height:23px;border:1px solid #c9c3ed;border-radius:8px;background:#ffffff50;color:#675bb6;font-size:10px;font-weight:700}
.groups{display:grid;gap:20px}
.group h3{display:flex;align-items:baseline;justify-content:space-between;gap:12px;flex-wrap:wrap;margin:0 0 12px;font-size:15px;font-weight:700;color:#28206f}
.group h3 span{font-size:12px;font-weight:600;color:var(--muted)}
.ep{display:grid;grid-template-columns:52px minmax(0,1.25fr) 128px minmax(0,1.6fr);align-items:center;gap:12px;padding:9px 0;border-top:1px solid #ffffffa6;font-size:13px}
.ep:first-of-type{border-top:0}
.m{display:inline-flex;justify-content:center;padding:2px 0;border-radius:7px;font-size:10px;font-weight:800;letter-spacing:.06em}
.m.get{background:#daf5e5;color:#17643d}.m.post{background:#e6e1ff;color:#4637b8}
code{font:600 12.5px/1.5 ui-monospace,Consolas,monospace;color:#342b80;overflow-wrap:anywhere}
code a{color:inherit;font-weight:inherit}
.a{justify-self:start;padding:2px 9px;border-radius:20px;font-size:11px;font-weight:650;white-space:nowrap}
.a.pub{background:#ffffff8c;color:#625d81;border:1px solid #ddd9ef}
.a.key{background:#fff4dc;color:#785a22}.a.ota{background:#fbe3e8;color:#84364c}.a.ses{background:#e3eefe;color:#24508f}
.d{color:var(--muted);font-size:12.5px}
.note{margin:12px 0 0;padding:10px 14px;border-radius:13px;background:#f4f2ffb3;color:#403779;font-size:12px}
footer{display:flex;justify-content:space-between;align-items:center;gap:20px;margin-top:26px;padding-top:18px;border-top:1px solid #ffffff8c;font-size:12px;color:var(--muted)}
footer p{display:flex;align-items:center;gap:8px;margin:0}footer svg{width:17px;height:17px}
a{color:#51429e;text-underline-offset:3px;font-weight:600}
a:focus-visible{outline:3px solid #8374e8;outline-offset:4px}
@media(min-width:1400px){body{padding-top:64px}}
@media(max-width:900px){.shortcuts{grid-template-columns:1fr}}
@media(max-width:700px){body{padding:20px 12px}main{padding:25px 20px 20px;border-radius:27px}.brand-pill{gap:12px;padding:10px 14px}.brand{font-size:16px}.board{font-size:12px;padding-left:12px}.connection{font-size:11px}.hero{margin:30px 0 24px}h1{letter-spacing:-.9px}.subtitle{font-size:14px}.shortcuts{gap:14px}.card{padding:20px;border-radius:19px}.section-title{font-size:16px;margin-top:28px}.groups{gap:14px}.ep{grid-template-columns:52px minmax(0,1fr) auto;gap:6px 10px}.ep .d{grid-column:1/-1}footer{gap:12px;font-size:11px;flex-direction:column;align-items:flex-start}.version{gap:9px}.uptime{font-size:11px}}
@media(max-width:420px){.brand-pill{border-radius:21px;gap:10px 15px}.brand{font-size:15px}.brand-mark{transform:scale(.85);width:27px}.board{padding-left:15px}.uptime{border:0;padding:0}.version{border-radius:16px;padding:7px 14px}h1{font-size:29px}.ep{grid-template-columns:52px minmax(0,1fr)}.ep .a{grid-column:2}}
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
<p class="eyebrow">TRUNG TÂM ĐIỀU KHIỂN LAN</p>
<h1 id="page-title">KhaiRC Hub</h1>
<p class="subtitle">Điều khiển IR và ổ cắm thông minh ngay trên mạng nhà bạn.</p>
<div class="version"><span>Firmware</span><strong id="version">—</strong><span class="uptime" id="uptime">Đang đọc thông tin…</span><span class="uptime" id="slot" hidden></span><span class="uptime" id="heap" hidden></span></div>
</section>

<nav class="shortcuts" aria-label="Lối tắt">
<a class="card shortcut" id="ota-link">
<span class="icon"><svg aria-hidden="true" viewBox="0 0 24 24"><path d="M12 16V3m-5 5 5-5 5 5M4 16v4a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-4"/></svg></span>
<span><strong>Cập nhật firmware</strong><small id="ota-url">Cổng 8081 · /update</small></span>
</a>
<a class="card shortcut" id="ui-link">
<span class="icon"><svg aria-hidden="true" viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="16" rx="3"/><path d="M8 9h8M8 13h5"/></svg></span>
<span><strong>Bảng điều khiển</strong><small id="ui-url">HTTPS · /ui</small></span>
</a>
<a class="card shortcut" id="vault-link">
<span class="icon"><svg aria-hidden="true" viewBox="0 0 24 24"><rect x="5" y="10" width="14" height="11" rx="3"/><path d="M8 10V7a4 4 0 0 1 8 0v3M12 14v3"/></svg></span>
<span><strong>Web Vault</strong><small id="vault-url">HTTPS · /ui/vault</small></span>
</a>
</nav>
<p class="note">Bảng điều khiển và Web Vault chỉ có khi firmware được build kèm chứng chỉ TLS. Trình duyệt sẽ cảnh báo chứng chỉ tự ký.</p>

<h2 class="section-title"><span class="step" aria-hidden="true">API</span>Danh mục endpoint</h2>
<div class="groups">

<section class="card group" aria-labelledby="g-http">
<h3 id="g-http">HTTP · cổng 80 <span>JSON, không mã hóa</span></h3>
<div class="ep"><span class="m get">GET</span><code>/</code><span class="a pub">Công khai</span><span class="d">Trang này</span></div>
<div class="ep"><span class="m get">GET</span><code><a href="/api/v1/health">/api/v1/health</a></code><span class="a pub">Công khai</span><span class="d">Trạng thái, firmware, boot/build ID, uptime, heap, slot OTA</span></div>
<div class="ep"><span class="m get">GET</span><code><a href="/api/v1/system">/api/v1/system</a></code><span class="a pub">Công khai</span><span class="d">Chip, flash, PSRAM, IP, RSSI</span></div>
<div class="ep"><span class="m get">GET</span><code><a href="/api/v1/devices">/api/v1/devices</a></code><span class="a pub">Công khai</span><span class="d">Danh sách thiết bị trong registry</span></div>
<div class="ep"><span class="m get">GET</span><code>/api/v1/devices/{id}</code><span class="a pub">Công khai</span><span class="d">Chi tiết một thiết bị; ID lạ trả 404</span></div>
<div class="ep"><span class="m get">GET</span><code><a href="/api/v1/capabilities">/api/v1/capabilities</a></code><span class="a pub">Công khai</span><span class="d">Khả năng: phát IR, cập nhật OTA…</span></div>
<div class="ep"><span class="m get">GET</span><code>/api/v1/devices/milife_plug_10a/state</code><span class="a key">X-API-Key</span><span class="d">Đọc trạng thái ổ cắm Milife qua miIO LAN</span></div>
<div class="ep"><span class="m post">POST</span><code>/api/v1/commands</code><span class="a key">X-API-Key</span><span class="d">Gửi lệnh IR. Body JSON <code>{"device_id","action"}</code>, tối đa 512 byte</span></div>
<p class="note">Lệnh hợp lệ: quạt TAIZ <code>power_on</code> · <code>swing</code> · <code>power_off</code>; Toshiba <code>fix</code>. Phản hồi <code>status:"sent"</code> chỉ xác nhận đã phát, không xác nhận trạng thái thiết bị.</p>
</section>

<section class="card group" aria-labelledby="g-ota">
<h3 id="g-ota">OTA · cổng 8081 <span>Chỉ dùng trong LAN</span></h3>
<div class="ep"><span class="m get">GET</span><code>/update</code><span class="a pub">Công khai</span><span class="d">Trang cập nhật firmware</span></div>
<div class="ep"><span class="m get">GET</span><code>/api/v1/health</code><span class="a pub">Công khai</span><span class="d">Giống cổng 80, dùng để xác minh sau khi khởi động lại</span></div>
<div class="ep"><span class="m get">GET</span><code>/api/v1/ota</code><span class="a ota">Mật khẩu OTA</span><span class="d">Kiểm tra mật khẩu và dung lượng firmware tối đa</span></div>
<div class="ep"><span class="m post">POST</span><code>/update</code><span class="a ota">Mật khẩu OTA</span><span class="d">Nạp firmware <code>application/octet-stream</code>, tối đa 4 MiB</span></div>
</section>

<section class="card group" aria-labelledby="g-https">
<h3 id="g-https">HTTPS · cổng 443 <span>Chỉ khi build kèm TLS</span></h3>
<div class="ep"><span class="m get">GET</span><code>/api/v1/health</code><span class="a pub">Công khai</span><span class="d">Trạng thái, uptime, heap, build ID</span></div>
<div class="ep"><span class="m get">GET</span><code>/api/v1/system · /capabilities · /devices</code><span class="a pub">Công khai</span><span class="d">Cùng nội dung với cổng 80</span></div>
<div class="ep"><span class="m get">GET</span><code>/api/v1/devices/milife_plug_10a/state</code><span class="a key">X-API-Key</span><span class="d">Như cổng 80</span></div>
<div class="ep"><span class="m post">POST</span><code>/api/v1/commands</code><span class="a key">X-API-Key</span><span class="d">Toshiba <code>state</code> · <code>fix</code> · <code>swing_on</code> · <code>swing_off</code></span></div>
<div class="ep"><span class="m get">GET</span><code>/ui · /ui/vault</code><span class="a pub">Công khai</span><span class="d">Bảng điều khiển và Web Vault</span></div>
<div class="ep"><span class="m post">POST</span><code>/web/login</code><span class="a key">Mật khẩu / API key</span><span class="d">Đăng nhập (mật khẩu Web UI hoặc API key từ Vault), tạo session cookie</span></div>
<div class="ep"><span class="m get">GET</span><code>/web/session</code><span class="a ses">Session</span><span class="d">Cấp token CSRF</span></div>
<div class="ep"><span class="m post">POST</span><code>/web/command · /web/logout</code><span class="a ses">Session + CSRF</span><span class="d">Lệnh Toshiba, ổ cắm Rạng Đông OC09; đăng xuất</span></div>
</section>

</div>
<footer>
<p><svg aria-hidden="true" viewBox="0 0 24 24"><path d="M12 3 4 6v6c0 4.5 3.4 8.3 8 9 4.6-.7 8-4.5 8-9V6z"/><path d="M12 8v5m0 3h.01"/></svg>Không forward cổng 80 và 8081 ra Internet.</p>
<p>Mất phản hồi khi gửi SWING/FIX: không tự gửi lại.</p>
</footer>
</main>
<script>
(function(){
'use strict';
var $=function(id){return document.getElementById(id);};
var host=location.hostname;
if(host.indexOf(':')>=0)host='['+host+']';
function link(id,urlId,url){var a=$(id);a.href=url;$(urlId).textContent=url;}
link('ota-link','ota-url','http://'+host+':8081/update');
link('ui-link','ui-url','https://'+host+'/ui');
link('vault-link','vault-url','https://'+host+'/ui/vault');

function uptime(sec){
  var s=Math.floor(sec),d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
  if(d>0)return d+' ngày '+h+' giờ';
  if(h>0)return h+' giờ '+m+' phút';
  return m+' phút '+(s%60)+' giây';
}
function setConnection(state,text){var c=$('connection');c.dataset.state=state;c.textContent=text;}
function show(id,text){var el=$(id);el.textContent=text;el.hidden=false;}

function refresh(){
  var ctrl=window.AbortController?new AbortController():null;
  var timer=setTimeout(function(){if(ctrl)ctrl.abort();},5000);
  fetch('/api/v1/health',{cache:'no-store',signal:ctrl?ctrl.signal:undefined})
    .then(function(r){if(!r.ok)throw new Error('http');return r.json();})
    .then(function(j){
      if(!j||j.status!=='ok')throw new Error('status');
      setConnection('online','Trực tuyến');
      $('version').textContent=typeof j.firmware==='string'?j.firmware:'—';
      $('uptime').textContent=Number.isFinite(j.uptime_sec)?'Uptime '+uptime(j.uptime_sec):'Uptime —';
      if(typeof j.ota_slot==='string'&&typeof j.ota_state==='string')show('slot',j.ota_slot+' · '+j.ota_state);
      if(Number.isFinite(j.free_heap))show('heap','Heap '+Math.round(j.free_heap/1024)+' KB');
    })
    .catch(function(){
      setConnection('offline','Mất kết nối');
      $('uptime').textContent='Không đọc được trạng thái';
    })
    .then(function(){clearTimeout(timer);setTimeout(refresh,15000);});
}
refresh();
})();
</script>
</body>
</html>
)LANDINGHTML";

}

namespace LandingPage {

const char* html() {
    return page;
}

}
