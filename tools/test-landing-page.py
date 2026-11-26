"""Test the embedded landing page (GET / on port 80) in headless Chromium.

Usage: python tools/test-landing-page.py --browser <path-to-chrome-or-msedge> [--screenshots <dir>]
Requires: python -m pip install playwright (uses your installed browser).
The page is served from a fake origin with the firmware's CSP; no board is contacted.
"""
import argparse
import json
import pathlib
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
ORIGIN = "http://192.168.1.50"
CSP = ("default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
       "img-src data:; connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")
CASES = ("online", "offline", "http_error", "malicious")
VIEWPORTS = (("desktop", 1440, 1000), ("mobile", 390, 844), ("narrow", 360, 740))

HEALTH = {"status": "ok", "device": "KhaiRC-ESP32-Hub", "firmware": "0.4.0-ota1", "boot_id": "1a2b",
          "uptime_sec": 93784, "free_heap": 204800, "build_id": "0123456789abcdef",
          "ota_slot": "app0", "ota_state": "valid"}

CHECK = r"""
() => {
  const $ = id => document.getElementById(id);
  return {
    state: $('connection').dataset.state,
    connection: $('connection').textContent,
    version: $('version').textContent,
    uptime: $('uptime').textContent,
    slot: $('slot').hidden ? null : $('slot').textContent,
    heap: $('heap').hidden ? null : $('heap').textContent,
    ota: $('ota-link').getAttribute('href'),
    ui: $('ui-link').getAttribute('href'),
    vault: $('vault-link').getAttribute('href'),
    endpoints: document.querySelectorAll('.ep').length,
    injected: document.querySelectorAll('main img, #version *').length,
    pwned: window.pwned === 1,
    overflow: document.documentElement.scrollWidth > window.innerWidth + 1,
  };
}
"""


def health_handler(case):
    def handle(route):
        if case == "offline":
            route.abort()
            return
        if case == "http_error":
            route.fulfill(status=500, content_type="application/json", body='{"error":{"code":"x"}}')
            return
        body = dict(HEALTH)
        if case == "malicious":
            body["firmware"] = '<img src=x onerror="window.pwned=1">'
            body["ota_slot"] = "<b>app0</b>"
        route.fulfill(status=200, content_type="application/json", body=json.dumps(body))
    return handle


def verify(case, r):
    problems = []
    if r["overflow"]:
        problems.append("horizontal scroll")
    if r["ota"] != "http://192.168.1.50:8081/update" or r["ui"] != "https://192.168.1.50/ui" \
            or r["vault"] != "https://192.168.1.50/ui/vault":
        problems.append("shortcut links " + repr((r["ota"], r["ui"], r["vault"])))
    if r["endpoints"] < 20:
        problems.append("endpoint count " + str(r["endpoints"]))
    if case == "online":
        if r["state"] != "online" or r["connection"] != "Trực tuyến" or r["version"] != "0.4.0-ota1" \
                or r["uptime"] != "Uptime 1 ngày 2 giờ" or r["slot"] != "app0 · valid" or r["heap"] != "Heap 200 KB":
            problems.append("online values " + repr(r))
    elif case in ("offline", "http_error"):
        if r["state"] != "offline" or r["connection"] != "Mất kết nối" or r["version"] != "—" \
                or r["slot"] is not None or r["heap"] is not None:
            problems.append("offline values " + repr(r))
    elif case == "malicious":
        if r["injected"] or r["pwned"] or not r["version"].startswith("<img"):
            problems.append("health data was rendered as HTML " + repr(r))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True)
    parser.add_argument("--screenshots", type=pathlib.Path, help="Optional directory for online PNG previews")
    args = parser.parse_args()
    source = (ROOT / "src/LandingPage.cpp").read_text(encoding="utf-8")
    page = source.split('R"LANDINGHTML(', 1)[1].split(')LANDINGHTML"', 1)[0]
    if args.screenshots:
        args.screenshots.mkdir(parents=True, exist_ok=True)
    failures = 0
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=args.browser, headless=True)
        try:
            for viewport, width, height in VIEWPORTS:
                for case in CASES:
                    context = browser.new_context(viewport={"width": width, "height": height},
                                                  is_mobile=viewport != "desktop", has_touch=viewport != "desktop")
                    try:
                        context.route(ORIGIN + "/", lambda route: route.fulfill(
                            status=200, content_type="text/html; charset=utf-8", body=page,
                            headers={"Content-Security-Policy": CSP, "Cache-Control": "no-store"}))
                        context.route(ORIGIN + "/api/v1/health", health_handler(case))
                        tab = context.new_page()
                        errors = []
                        tab.on("pageerror", lambda error: errors.append(str(error)))
                        tab.on("console", lambda msg: errors.append(msg.text) if msg.type == "error"
                               and "Content Security Policy" in msg.text else None)
                        tab.goto(ORIGIN + "/")
                        tab.wait_for_function("document.getElementById('connection').dataset.state !== 'checking'",
                                              timeout=10000)
                        problems = verify(case, tab.evaluate(CHECK)) + errors
                        if args.screenshots and case == "online":
                            tab.screenshot(path=str(args.screenshots / f"landing-{viewport}.png"), full_page=True)
                        status = "PASS" if not problems else "FAIL"
                        failures += bool(problems)
                        print(f"{viewport}: {status} {case}" + ("" if not problems else " " + "; ".join(problems)))
                    finally:
                        context.close()
        finally:
            browser.close()
    total = len(CASES) * len(VIEWPORTS)
    if failures:
        raise SystemExit(f"{failures}/{total} landing page scenarios failed")
    print(f"Passed {total} Chromium landing page scenarios (simulated network, CSP enforced).")


if __name__ == "__main__":
    main()
