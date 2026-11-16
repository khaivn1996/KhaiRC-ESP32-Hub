import {initializeApp} from 'firebase/app';
import {initializeAuth, inMemoryPersistence, browserPopupRedirectResolver,
  GoogleAuthProvider, signInWithPopup, getIdToken, signOut} from 'firebase/auth';
import {decodeVault, VaultFormatError} from './codec.js';
import {openVault} from './crypto.js';

const $ = id => document.getElementById(id);
const google = $('google'), unlock = $('unlock'), password = $('password');
const notice = text => { $('notice').textContent = text; };
let auth, busy = false;
const messages = {
  missing: 'Chưa có bản sao cloud – hãy tạo từ app KhaiRC',
  denied: 'Tài khoản không có quyền đọc bản sao',
  network: 'Không kết nối được cloud. Bấm thử lại.',
  key: 'Bản sao chưa có key Hub',
  login401: 'Key trong bản sao không khớp Hub – cập nhật ở app',
  login429: 'Hub đang tạm khóa đăng nhập, thử lại sau 30 giây',
  login: 'Hub từ chối đăng nhập'
};
function render() {
  google.disabled = busy;
  unlock.disabled = busy || !auth?.currentUser;
}
function showError(error) {
  if (error instanceof VaultFormatError) notice('Bản sao không đúng định dạng');
  else if (error?.name === 'OperationError') notice('Sai mật khẩu chủ hoặc dữ liệu cloud bị hỏng');
  else if (error?.code === 'auth/popup-closed-by-user' || error?.code === 'auth/cancelled-popup-request') {
    notice('Đã hủy đăng nhập Google');
  } else notice(messages[error?.code] || messages.network);
}
function failure(code) { throw {code}; }

if (!isSecureContext || !globalThis.crypto?.subtle) {
  notice('Cần HTTPS được trình duyệt tin cậy và hỗ trợ WebCrypto.');
  google.disabled = unlock.disabled = true;
} else if (navigator.standalone === true || matchMedia('(display-mode: standalone)').matches) {
  notice('Hãy mở trang này bằng Safari/Chrome thường để đăng nhập Google.');
  google.disabled = unlock.disabled = true;
} else {
  try {
    const app = initializeApp(JSON.parse($('cfg').textContent));
    auth = initializeAuth(app, {persistence: inMemoryPersistence,
      popupRedirectResolver: browserPopupRedirectResolver});
    render();
  } catch (_) {
    notice('Cấu hình Firebase web không hợp lệ.');
    google.disabled = unlock.disabled = true;
  }
}
google.addEventListener('click', async () => {
  if (!auth || busy) return;
  busy = true; render();
  try {
    await signInWithPopup(auth, new GoogleAuthProvider());
    notice('Đã đăng nhập Google. Nhập mật khẩu chủ để mở khóa.');
    password.focus();
  } catch (error) { showError(error); }
  finally { busy = false; render(); }
});
$('vault-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (!auth?.currentUser || busy) return;
  busy = true; render();
  let plaintext, vault, key;
  try {
    notice('Đang đọc và mở khóa bản sao cloud…');
    const user = auth.currentUser;
    const token = await getIdToken(user);
    const project = JSON.parse($('cfg').textContent).projectId;
    const url = 'https://firestore.googleapis.com/v1/projects/' + encodeURIComponent(project) +
      '/databases/(default)/documents/users/' + encodeURIComponent(user.uid) + '/vault/main';
    const response = await fetch(url, {headers: {Authorization: 'Bearer ' + token},
      cache: 'no-store', credentials: 'omit', redirect: 'error'});
    if (response.status === 404) failure('missing');
    if (response.status === 401 || response.status === 403) failure('denied');
    if (!response.ok) failure('network');
    let doc;
    try { doc = await response.json(); } catch (_) { throw new VaultFormatError(); }
    plaintext = await openVault(password.value, user.uid, doc);
    vault = decodeVault(plaintext);
    key = vault.hub?.apiKey;
    if (!key) failure('key');
    if (!/^[\x20-\x7e]{64}$/.test(key)) throw new VaultFormatError();
    const login = await fetch('/web/login', {method: 'POST', redirect: 'manual',
      cache: 'no-store', credentials: 'same-origin',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'api_key=' + encodeURIComponent(key)});
    if (login.type === 'opaqueredirect') location.replace('/ui');
    else if (login.status === 401) failure('login401');
    else if (login.status === 429) failure('login429');
    else failure('login');
  } catch (error) { showError(error); }
  finally {
    password.value = '';
    plaintext?.fill(0);
    vault = key = plaintext = null;
    try { await signOut(auth); } catch (_) { /* No automatic retry. */ }
    busy = false; render();
  }
});
