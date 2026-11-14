"""Browser regression for the embedded native login form and Referrer-Policy.

Uses synthetic credentials and mocked HTTPS responses; never contacts the board.
Runs WebKit with an iPhone profile by default (python -m playwright install webkit).
Use --browser <chrome.exe|msedge.exe> to run Chromium instead.
Requires OpenSSL (--openssl <openssl.exe>) for a temporary localhost certificate.
Certificate errors are ignored only against this local test server.
This checks browser behavior and page JS, not ESP-IDF or an actual iPhone.
"""
import argparse
import json
import pathlib
import re
import ssl
import subprocess
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

from playwright.sync_api import expect, sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[1]
KEY = "a" * 64
SID = "b" * 32
CSRF = "c" * 32


def run_case(browser, profile, tls, policy, should_login):
    source = (ROOT / "src/WebUiPage.cpp").read_text(encoding="utf-8")
    html, script = re.findall(r'R"KHAIRCSTAGE3A\((.*?)\)KHAIRCSTAGE3A"', source, re.S)[:2]
    backend = (ROOT / "src/WebUi.cpp").read_text(encoding="utf-8")
    harden = backend.split("void harden(", 1)[1].split("\n}", 1)[0]
    headers = dict(re.findall(r'httpd_resp_set_hdr\(r, "([^"]+)", "([^"]+)"\)', harden))
    headers["Referrer-Policy"] = policy
    posts = []
    commands = []
    errors = []
    context = browser.new_context(**profile, ignore_https_errors=True)

    def handle(request):
        path = urlsplit(request.path).path
        incoming = {name.lower(): value for name, value in request.headers.items()}
        post_data = request.rfile.read(int(incoming.get("content-length", "0"))).decode()
        response_headers = dict(headers)
        status, body, content_type = 200, "", "application/json"
        authenticated = f"khairc_sid={SID}" in incoming.get("cookie", "")
        if path == "/ui":
            body, content_type = html, "text/html; charset=utf-8"
        elif path == "/web/app.js":
            body, content_type = script, "text/javascript"
        elif path == "/web/login":
            assert request.command == "POST"
            posts.append(incoming.get("origin"))
            # Mirror the existing server contract, without changing browser headers.
            if incoming.get("origin") not in (None, origin):
                status, body = 403, '{"error":{"code":"origin_blocked"}}'
            elif parse_qs(post_data).get("api_key") != [KEY]:
                status, body = 401, "Wrong synthetic key"
            else:
                status = 303
                response_headers.update({
                    "Location": "/ui",
                    "Set-Cookie": f"khairc_sid={SID}; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=900",
                })
        elif path == "/web/session":
            status = 200 if authenticated else 401
            body = json.dumps({"authenticated": True, "csrf": CSRF} if authenticated
                              else {"error": {"code": "login_required"}})
        elif path == "/api/v1/health":
            body = '{"status":"ok","device":"KhaiRC-ESP32-Hub"}'
        elif path == "/web/command":
            commands.append(post_data)
            status, body = 500, "Unexpected command during login test"
        elif path == "/web/logout":
            assert authenticated and incoming.get("x-csrf-token") == CSRF
            response_headers["Set-Cookie"] = "khairc_sid=; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=0"
            body = '{"ok":true}'
        else:
            status = 404
        encoded = body.encode("utf-8")
        request.send_response(status)
        request.send_header("Content-Type", content_type)
        request.send_header("Content-Length", str(len(encoded)))
        for name, value in response_headers.items():
            request.send_header(name, value)
        request.end_headers()
        request.wfile.write(encoded)

    class Handler(BaseHTTPRequestHandler):
        do_GET = handle
        do_POST = handle

        def log_message(self, *args):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.socket = tls.wrap_socket(server.socket, server_side=True)
    origin = f"https://127.0.0.1:{server.server_port}"
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    page = context.new_page()
    page.on("pageerror", lambda error: errors.append(str(error)))
    try:
        page.goto(origin + "/ui")
        expect(page.locator("#login-card")).to_be_visible()
        if should_login:
            page.locator("#key").fill("z" * 64)
            with page.expect_response(lambda response: response.url == origin + "/web/login") as failed:
                page.locator('button[type="submit"]').click()
            assert failed.value.status == 401
            page.wait_for_url(origin + "/web/login", wait_until="load")
            page.goto(origin + "/ui")
            expect(page.locator("#login-card")).to_be_visible()
        page.locator("#key").fill(KEY)
        with page.expect_response(lambda response: response.url == origin + "/web/login") as result:
            page.locator('button[type="submit"]').click()
        if not should_login:
            assert result.value.status == 403 and posts == ["null"], posts
        else:
            assert result.value.status == 303 and posts == [origin, origin], posts
            page.wait_for_url(origin + "/ui", wait_until="load")
            expect(page.locator("#remote")).to_be_visible()
            expect(page.locator("#connection")).to_have_text("LAN · TLS")
            expect(page.locator("#power-on")).to_be_enabled()
            cookie = next(c for c in context.cookies() if c["name"] == "khairc_sid")
            assert cookie["httpOnly"] and cookie["secure"]
            # This Windows WebKit build reports SameSite=None through its cookie
            # API; only Chromium's cookie API is used to check that attribute.
            if browser.browser_type.name == "chromium":
                assert cookie["sameSite"] == "Strict"
            assert page.evaluate("document.cookie") == ""
            assert page.evaluate("localStorage.length + sessionStorage.length") == 0
            page.reload()
            expect(page.locator("#connection")).to_have_text("LAN · TLS")
            page.locator("#logout").click()
            expect(page.locator("#login-card")).to_be_visible()
            page.reload()
            expect(page.locator("#connection")).to_have_text("Cần đăng nhập")
        assert not commands, "Login must never send IR commands"
        assert not errors, errors
    finally:
        context.close()
        server.shutdown()
        server.server_close()
        worker.join()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--browser", help="Optional installed Chromium executable")
    parser.add_argument("--openssl", default="openssl", help="OpenSSL executable for an ephemeral test certificate")
    args = parser.parse_args()
    backend = (ROOT / "src/WebUi.cpp").read_text(encoding="utf-8")
    policy = re.search(r'httpd_resp_set_hdr\(r, "Referrer-Policy", "([^"]+)"\)', backend)[1]
    with tempfile.TemporaryDirectory(prefix="khairc-login-") as directory, sync_playwright() as playwright:
        cert = pathlib.Path(directory) / "cert.pem"
        key = pathlib.Path(directory) / "key.pem"
        subprocess.run([args.openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-keyout", str(key), "-out", str(cert), "-days", "1",
                        "-subj", "/CN=localhost"], check=True, capture_output=True)
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(cert, key)
        browser = (playwright.chromium.launch(executable_path=args.browser) if args.browser
                   else playwright.webkit.launch())
        try:
            profile = dict(playwright.devices["iPhone 13"])
            profile.pop("default_browser_type", None)
            run_case(browser, profile, tls, "no-referrer", False)
            print("PASS: original no-referrer reproduces Origin:null / 403")
            run_case(browser, profile, tls, policy, True)
            print("PASS: same-origin: wrong key, login/303, cookie, reload, logout; no IR")
        finally:
            browser.close()


if __name__ == "__main__":
    main()
