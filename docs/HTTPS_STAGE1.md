# KhaiRC HTTPS - Stage 1 (LAN-only probe)

This experimental build adds an ESP-IDF HTTPS server on ESP32 port **443** but only registers `GET /api/v1/health`. It does not expose IR commands, MiLife controls, OTA, or any secret. Existing Arduino `WebServer` port 80 and OTA port 8081 remain unchanged. Do **not** port-forward 443 yet. This is not a production release.

Toolchain in `platformio.ini`: `espressif32 @ 8.0.0`, Arduino Core 3.3.12 / ESP-IDF 5.5.5. API based on `esp_https_server` official ESP-IDF 5.5.x documentation. Validate build because Arduino's prebuilt components may differ from a full ESP-IDF project.

## Make a locally trusted test certificate

From a terminal on your Windows development computer, run `openssl version` to confirm OpenSSL is available. If it is missing, stop rather than generating keys on a third-party website.

From the project root in PowerShell:

```powershell
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -sha256 -days 365 -keyout include/tls_private_key.pem -out include/tls_certificate.pem -subj "/CN=openwrt96.ddns.net" -addext "subjectAltName=DNS:openwrt96.ddns.net,IP:192.168.2.200"
python tools/create_https_credentials.py
```

The key, certificate, and generated `include/tls_credentials.h` are Git-ignored. Never post or commit the private key/header. Retain the certificate (public) for a future Android app-specific trust-anchor; plan rotation/renewal. The example domain/IP must be changed if they change.

## Build before any upload

```powershell
git status --short
pio run -e esp32-s3-n16r8 *> .pio/build.log
Get-Content .pio/build.log -Tail 25
```

The build should finish `SUCCESS`. If compilation or linking fails (including missing `esp_https_server`), stop and report concise error lines. Do **not** change frameworks/dependencies ad hoc. Do not upload a failed build. Run the existing OTA host and page tests before upload. No unattended reflash.

## Hardware test (after successful build and user-initiated upload)

1. Check HTTP LAN `http://192.168.2.200/` still returns JSON.
2. Check OTA LAN `http://192.168.2.200:8081/update` still loads.
3. Check `https://192.168.2.200/api/v1/health` via `curl.exe -k -i` **only for the read-only TLS handshake probe**. The `-k` option ignores cert validation and MUST NOT be used in KhaiRC Android or production requests. Normal Chrome may warn about the self-signed certificate.
4. Inspect `[HTTPS]` log and free heap before/after repeated TLS handshakes and across Wi-Fi reconnect. Check no crash/reset and no degradation to MiLife read-only/IR workflows.
5. Confirm `https://192.168.2.200/update` and `https://192.168.2.200/api/v1/ota` do not expose OTA, and HTTPS currently has no commands endpoint.
6. Keep router WAN forwards `18080` and `999` disabled. Do not create a 443 forward in this stage.

Later stage: implement HTTPS command/state APIs with correct authentication and bounds (not simple `WebServer` socket substitution), Android hostname/port/SSL validation with certificate trust anchor or verified pins, only then open the HTTPS WAN port. Keep MiLife ON/OFF separate and safety-gated.
