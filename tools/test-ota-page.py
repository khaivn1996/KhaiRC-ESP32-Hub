"""Test the embedded OTA page in headless Chromium with simulated HTTP responses.

Usage: python tools/test-ota-page.py --browser <path-to-chrome-or-msedge>
Requires: python -m pip install playwright (uses your installed browser).
All passwords and firmware data are synthetic; no board is contacted.
"""
import argparse
import json
import pathlib
import tempfile
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
CASES = ("idle", "selection", "success", "wrong_password", "too_large", "partition_limit", "invalid_image", "network_error", "no_reboot")

MOCK = r"""
window.testState = {posts: 0, uploaded: false, headers: {}, size: 0};
const nativeTimeout = window.setTimeout.bind(window);
window.setTimeout = (callback, delay) => nativeTimeout(callback, delay === 2000 ? 0 : delay);
window.fetch = async (url) => {
  const health = {firmware: '0.4.0-ota1', uptime_sec: 120,
    boot_id: testState.uploaded && testCase !== 'no_reboot' ? 'new-boot' : 'old-boot'};
  if (url === '/api/v1/health') return {ok: true, json: async () => health};
  if (testCase === 'wrong_password') return {ok: false, status: 401,
    json: async () => ({error: {code: 'unauthorized'}})};
  return {ok: true, json: async () => ({max_firmware_bytes: testCase === 'partition_limit' ? 100 : 4194304})};
};
window.XMLHttpRequest = class {
  constructor() { this.upload = {}; }
  open(method, url) { testState.method = method; testState.url = url; }
  setRequestHeader(key, value) { testState.headers[key] = value; }
  send(file) {
    testState.posts++;
    testState.size = file.size;
    nativeTimeout(() => {
      if (testCase === 'network_error') { this.onerror(); return; }
      this.upload.onprogress({lengthComputable: true, loaded: file.size, total: file.size});
      this.status = testCase === 'invalid_image' ? 400 : 200;
      this.responseText = JSON.stringify(this.status === 200 ? {status: 'updated'} : {error: {code: 'invalid_firmware_image'}});
      testState.uploaded = this.status === 200;
      this.onload();
    }, 0);
  }
};
"""

DRIVE = r"""
window.addEventListener('DOMContentLoaded', async () => {
  await new Promise(resolve => nativeTimeout(resolve, 20));
  function report(ok) {
    // Check layout in addition to upload semantics. Long filenames must wrap.
    const cards = document.querySelectorAll('.form-card');
    const first = cards[0].getBoundingClientRect();
    const second = cards[1].getBoundingClientRect();
    const stacked = window.innerWidth <= 700;
    ok = ok && window.innerWidth === testViewportWidth &&
      document.documentElement.scrollWidth <= window.innerWidth + 1 &&
      (stacked ? second.top > first.top && Math.abs(first.left - second.left) < 1 : Math.abs(first.top - second.top) < 1);
    const result = document.createElement('pre');
    result.id = 'test-result';
    result.hidden = true;
    result.textContent = (ok ? 'PASS ' : 'FAIL ') + testCase;
    document.body.appendChild(result);
  }
  if (testCase === 'idle') {
    report(document.getElementById('feedback').hidden && document.getElementById('transfer').hidden &&
      document.getElementById('connection').dataset.state === 'online' && testState.posts === 0);
    return;
  }
  const key = document.getElementById('password');
  key.value = 'synthetic-test-password';
  const transfer = new DataTransfer();
  const size = testCase === 'too_large' ? 4194305 : 1024;
  const filename = testCase === 'selection' ? 'firmware-khairc-'.repeat(12) + '.bin' : 'firmware.bin';
  transfer.items.add(new File([new Uint8Array(size)], filename, {type: 'application/octet-stream'}));
  document.getElementById('firmware').files = transfer.files;
  document.getElementById('firmware').dispatchEvent(new Event('change'));
  if (testCase === 'selection') {
    document.getElementById('firmware').focus();
    report(document.getElementById('file-name').textContent === filename &&
      document.activeElement.id === 'firmware' && testState.posts === 0);
    return;
  }
  document.getElementById('upload-form').dispatchEvent(new Event('submit', {cancelable: true}));
  for (let attempt = 0; attempt < 500 && document.getElementById('upload').disabled; ++attempt) {
    await new Promise(resolve => nativeTimeout(resolve, 10));
  }
  const text = document.getElementById('status').textContent;
  const expected = {
    success: 'Cập nhật thành công', wrong_password: 'Mật khẩu OTA không đúng',
    too_large: 'tối đa 4 MiB', partition_limit: 'chỉ nhận tối đa 100 byte',
    invalid_image: 'không phải firmware ứng dụng ESP32-S3 hợp lệ',
    network_error: 'Chưa thể xác nhận kết quả', no_reboot: 'chưa thấy mạch kết nối lại'
  };
  const posts = ['wrong_password', 'too_large', 'partition_limit'].includes(testCase) ? 0 : 1;
  let ok = text.includes(expected[testCase]) && testState.posts === posts && !document.getElementById('upload').disabled;
  if (posts) ok = ok && testState.method === 'POST' && testState.url === '/update' &&
    testState.headers['Content-Type'] === 'application/octet-stream' &&
    testState.headers['X-OTA-Password'] === 'synthetic-test-password' && testState.size === 1024;
  if (testCase !== 'too_large') ok = ok && key.value === '';
  const state = document.getElementById('feedback').dataset.state;
  ok = ok && !document.getElementById('feedback').hidden &&
    state === (testCase === 'success' ? 'success' : testCase === 'no_reboot' ? 'warning' : 'error');
  if (testCase === 'success') ok = ok && document.getElementById('file-name').textContent === 'Chọn file firmware';
  report(ok);
});
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True)
    parser.add_argument("--screenshots", type=pathlib.Path, help="Optional directory for idle/success desktop/mobile PNG previews")
    args = parser.parse_args()
    source = (ROOT / "src/OtaPage.cpp").read_text(encoding="utf-8")
    page = source.split('R"OTAHTML(', 1)[1].split(')OTAHTML"', 1)[0]
    if args.screenshots:
        args.screenshots.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="khairc-page-test-") as directory, sync_playwright() as playwright:
        temp = pathlib.Path(directory)
        html = temp / "ota.html"
        html.write_text(page, encoding="utf-8")
        browser = playwright.chromium.launch(executable_path=args.browser, headless=True)
        try:
            for viewport, width, height in (("desktop", 1440, 1080), ("mobile", 390, 844)):
                for case in CASES:
                    context = browser.new_context(viewport={"width": width, "height": height},
                                                  is_mobile=viewport == "mobile", has_touch=viewport == "mobile")
                    try:
                        prefix = "const testCase = " + json.dumps(case) + "; const testViewportWidth = " + str(width) + ";\n"
                        context.add_init_script(prefix + MOCK + DRIVE)
                        tab = context.new_page()
                        script_errors = []
                        tab.on("pageerror", lambda error: script_errors.append(str(error)))
                        tab.goto(html.as_uri())
                        tab.wait_for_function("document.getElementById('test-result') !== null", timeout=10000)
                        result = tab.locator("#test-result").text_content()
                        if result != "PASS " + case or script_errors:
                            raise RuntimeError(f"Page scenario failed: {viewport}/{case}; {result}; {script_errors}")
                        if args.screenshots and case in ("idle", "success"):
                            tab.screenshot(path=str(args.screenshots / f"soft-glass-{viewport}-{case}.png"), full_page=True)
                        print(viewport + ": " + result)
                    finally:
                        context.close()
        finally:
            browser.close()
    print(f"Passed {len(CASES) * 2} Chromium page scenarios (simulated network, desktop + mobile).")


if __name__ == "__main__":
    main()
