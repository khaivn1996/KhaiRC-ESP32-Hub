"""Test the embedded HTTPS controls in Chromium with a simulated Hub."""

import argparse
import pathlib
import re

from playwright.sync_api import expect, sync_playwright


ROOT = pathlib.Path(__file__).resolve().parents[1]
ORIGIN = "https://hub.test"
PLUG = "rangdong_oc09_s4"


def run_case(browser, html, script, csp, hub_online):
    requested = []
    commands = []
    errors = []
    with browser.new_context() as context:
        def handle(route):
            request = route.request
            path = request.url.removeprefix(ORIGIN)
            requested.append(path)
            if path == "/ui":
                route.fulfill(content_type="text/html; charset=utf-8", body=html,
                              headers={"Content-Security-Policy": csp})
            elif path == "/web/app.js":
                route.fulfill(content_type="text/javascript; charset=utf-8", body=script)
            elif path == "/api/v1/health":
                if hub_online:
                    route.fulfill(json={"status": "ok", "device": "KhaiRC-ESP32-Hub"})
                else:
                    route.fulfill(status=503, json={"status": "offline"})
            elif path == "/web/command":
                headers = request.all_headers()
                assert request.method == "POST"
                assert headers.get("origin") == ORIGIN
                assert headers.get("content-type") == "application/json"
                assert not {"cookie", "x-api-key", "x-csrf-token"} & headers.keys()
                payload = request.post_data_json
                commands.append(payload)
                reply = {"schema_version": 1, "action": payload["action"], "status": "sent"}
                if payload.get("device_id") == PLUG:
                    reply.update(device_id=PLUG, transport="tuya_lan", state_verified=True,
                                 power=payload["action"] == "power_on")
                else:
                    reply.update(device_id="toshiba_ac", transport="ir", state_verified=False)
                route.fulfill(json=reply)
            else:
                route.fulfill(status=404)

        context.route(ORIGIN + "/**", handle)
        page = context.new_page()
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.goto(ORIGIN + "/ui")
        expect(page.locator("#remote")).to_be_visible()
        expect(page.locator("#plug-card")).to_be_visible()
        expect(page.locator("form, #logout")).to_have_count(0)
        if hub_online:
            expect(page.locator("#connection")).to_have_text("LAN · TLS")
            for button, badge, text in (
                ("power-on", "power-badge", "Đã gửi ON"),
                ("power-off", "power-badge", "Đã gửi OFF"),
                ("plug-on", "plug-badge", "Đang BẬT"),
                ("plug-off", "plug-badge", "Đang TẮT"),
            ):
                page.locator("#" + button).click()
                expect(page.locator("#" + badge)).to_have_text(text)
            assert commands == [
                {"action": "state", "power": True, "temperature_c": 24, "fan_speed": "auto"},
                {"action": "state", "power": False, "temperature_c": 24, "fan_speed": "auto"},
                {"device_id": PLUG, "action": "power_on"},
                {"device_id": PLUG, "action": "power_off"},
            ], commands
        else:
            expect(page.locator("#connection")).to_have_text("Không kết nối Hub")
            for button in ("power-on", "power-off", "plug-on", "plug-off"):
                expect(page.locator("#" + button)).to_be_disabled()
            assert not commands, commands
        assert requested.count("/api/v1/health") == 1, requested
        assert not {"/web/login", "/web/logout", "/web/session", "/ui/vault"} & set(requested)
        assert not errors, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", required=True, help="Path to Chrome or Edge")
    args = parser.parse_args()
    source = (ROOT / "src/WebUiPage.cpp").read_text(encoding="utf-8")
    html, script = re.findall(r'R"KHAIRCSTAGE3A\((.*?)\)KHAIRCSTAGE3A"', source, re.S)[:2]
    backend = (ROOT / "src/WebUi.cpp").read_text(encoding="utf-8")
    view = backend.split('"Content-Security-Policy",', 1)[1].split(");", 1)[0]
    csp = "".join(re.findall(r'"([^"]*)"', view))
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=args.browser)
        for hub_online in (True, False):
            run_case(browser, html, script, csp, hub_online)
        browser.close()
    print("Passed direct /ui control and offline scenarios (simulated Hub).")


if __name__ == "__main__":
    main()
