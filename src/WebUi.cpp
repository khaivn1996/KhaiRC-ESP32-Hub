#include "WebUi.h"
#include "WebUiPage.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_random.h>
#include <cstring>

#include "IrDiagnostics.h"
#include "OtaService.h"
#include "secrets.h"

namespace {
constexpr size_t MAX_BODY_SIZE = 512;
constexpr unsigned long SESSION_MS = 15UL * 60UL * 1000UL;
constexpr size_t SESSION_SLOTS = 2;
constexpr size_t TOKEN_HEX_LEN = 32;
static_assert(sizeof(HUB_API_KEY) == 65, "HUB_API_KEY must have 64 chars");

struct Session {
    char id[TOKEN_HEX_LEN + 1] = {};
    char csrf[TOKEN_HEX_LEN + 1] = {};
    uint32_t issued = 0;
    bool active = false;
};
Session sessions[SESSION_SLOTS];
size_t nextSession = 0;
uint32_t loginBlockedUntil = 0;
uint8_t failedLogins = 0;

void harden(httpd_req_t* r) {
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
    // Native login forms need their same-origin Origin (no-referrer makes it null).
    httpd_resp_set_hdr(r, "Referrer-Policy", "same-origin");
}

esp_err_t respond(httpd_req_t* r, const char* status, const char* type,
                  const char* data) {
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, type);
    harden(r);
    return httpd_resp_send(r, data, HTTPD_RESP_USE_STRLEN);
}

esp_err_t error(httpd_req_t* r, const char* status, const char* code) {
    char payload[100];
    snprintf(payload, sizeof(payload), "{\"error\":{\"code\":\"%s\"}}", code);
    return respond(r, status, "application/json; charset=utf-8", payload);
}

bool equalConstantTime(const char* a, const char* b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

void randomToken(char out[TOKEN_HEX_LEN + 1]) {
    uint8_t bytes[TOKEN_HEX_LEN / 2];
    esp_fill_random(bytes, sizeof(bytes));
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 15];
    }
    out[TOKEN_HEX_LEN] = '\0';
}

bool expired(uint32_t issued) {
    return static_cast<uint32_t>(millis() - issued) >= SESSION_MS;
}

bool getHeader(httpd_req_t* r, const char* name, char* out, size_t capacity) {
    const size_t len = httpd_req_get_hdr_value_len(r, name);
    return len > 0 && len < capacity &&
           httpd_req_get_hdr_value_str(r, name, out, capacity) == ESP_OK;
}

// Ignore unrelated cookies. Match the exact cookie name and 32-byte hex value.
Session* currentSession(httpd_req_t* r) {
    char cookie[512] = {};
    if (!getHeader(r, "Cookie", cookie, sizeof(cookie))) return nullptr;
    const char* cur = cookie;
    while (*cur) {
        while (*cur == ' ' || *cur == ';') ++cur;
        const char* end = strchr(cur, ';');
        if (!end) end = cur + strlen(cur);
        constexpr char name[] = "khairc_sid=";
        const size_t pairLen = static_cast<size_t>(end - cur);
        if (pairLen == sizeof(name) - 1 + TOKEN_HEX_LEN &&
            strncmp(cur, name, sizeof(name) - 1) == 0) {
            const char* id = cur + sizeof(name) - 1;
            for (Session& s : sessions) {
                if (s.active && expired(s.issued)) s.active = false;
                if (s.active && equalConstantTime(s.id, id, TOKEN_HEX_LEN)) return &s;
            }
        }
        cur = *end ? end + 1 : end;
    }
    return nullptr;
}

// Browser Origin must match the Host that was used to load the HTTPS page.
// Some clients omit Origin; the independent secret CSRF header remains mandatory.
bool sameOriginWhenPresent(httpd_req_t* r) {
    if (httpd_req_get_hdr_value_len(r, "Origin") == 0) return true;
    char origin[270] = {};
    char host[255] = {};
    if (!getHeader(r, "Origin", origin, sizeof(origin)) ||
        !getHeader(r, "Host", host, sizeof(host))) return false;
    String expected = String("https://") + host;
    return expected.length() == strlen(origin) &&
           strcmp(origin, expected.c_str()) == 0;
}

bool readBody(httpd_req_t* r, char (&body)[MAX_BODY_SIZE + 1]) {
    if (r->content_len == 0 || r->content_len > MAX_BODY_SIZE ||
        httpd_req_get_hdr_value_len(r, "Transfer-Encoding") != 0) return false;
    size_t offset = 0;
    while (offset < r->content_len) {
        const int got = httpd_req_recv(r, body + offset,
                                      static_cast<size_t>(r->content_len - offset));
        if (got <= 0) return false;
        offset += static_cast<size_t>(got);
    }
    return memchr(body, '\0', static_cast<size_t>(offset)) == nullptr;
}

int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Native HTML form. The API key is not read by JavaScript or saved in a browser store.
bool parseFormKey(const char* form, char out[65]) {
    constexpr char prefix[] = "api_key=";
    if (strncmp(form, prefix, sizeof(prefix) - 1) != 0) return false;
    const char* read = form + sizeof(prefix) - 1;
    size_t length = 0;
    for (; *read; ++read) {
        if (length >= 64 || *read == '&') return false;
        int ch = static_cast<unsigned char>(*read);
        if (*read == '+') ch = ' ';
        else if (*read == '%') {
            if (read[1] == '\0' || read[2] == '\0') return false;
            const int a = nibble(read[1]), b = nibble(read[2]);
            if (a < 0 || b < 0) return false;
            ch = (a << 4) | b;
            read += 2;
        }
        if (ch < 32 || ch > 126) return false;
        out[length++] = static_cast<char>(ch);
    }
    out[length] = '\0';
    return length == 64;
}

esp_err_t view(httpd_req_t* r) {
    httpd_resp_set_hdr(r, "Content-Security-Policy",
        "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; "
        "connect-src 'self'; img-src 'self' data:; manifest-src 'self'; "
        "form-action 'self'; base-uri 'none'; frame-ancestors 'none'");
    return respond(r, "200 OK", "text/html; charset=utf-8", WebUiPage::html());
}

esp_err_t javascript(httpd_req_t* r) {
    return respond(r, "200 OK", "text/javascript; charset=utf-8", WebUiPage::script());
}
esp_err_t manifest(httpd_req_t* r) {
    return respond(r, "200 OK", "application/manifest+json", WebUiPage::manifest());
}
esp_err_t icon(httpd_req_t* r) {
    return respond(r, "200 OK", "image/svg+xml", WebUiPage::icon());
}
esp_err_t png(httpd_req_t* r) {
    httpd_resp_set_status(r, "200 OK");
    httpd_resp_set_type(r, "image/png");
    harden(r);
    return httpd_resp_send(r, reinterpret_cast<const char*>(WebUiPage::iconPng()),
                           static_cast<int>(WebUiPage::iconPngLen()));
}

esp_err_t login(httpd_req_t* r) {
    if (!sameOriginWhenPresent(r)) return error(r, "403 Forbidden", "origin_blocked");
    if (loginBlockedUntil != 0 &&
        static_cast<int32_t>(millis() - loginBlockedUntil) < 0) {
        return error(r, "429 Too Many Requests", "try_later");
    }
    char type[64] = {};
    if (!getHeader(r, "Content-Type", type, sizeof(type)) ||
        strcmp(type, "application/x-www-form-urlencoded") != 0) {
        return error(r, "415 Unsupported Media Type", "unsupported_media_type");
    }
    char body[MAX_BODY_SIZE + 1] = {};
    char provided[65] = {};
    const bool valid = readBody(r, body) && parseFormKey(body, provided);
    const bool match = valid && equalConstantTime(provided, HUB_API_KEY, 64);
    memset(body, 0, sizeof(body));
    memset(provided, 0, sizeof(provided));
    if (!match) {
        if (++failedLogins >= 5) {
            failedLogins = 0;
            loginBlockedUntil = millis() + 30000;
        }
        return respond(r, "401 Unauthorized", "text/html; charset=utf-8",
            "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>Đăng nhập thất bại</title><p>Mã API không đúng.</p>"
            "<p><a href='/ui'>Quay lại đăng nhập</a></p>");
    }
    failedLogins = 0;
    loginBlockedUntil = 0;
    Session& session = sessions[nextSession++ % SESSION_SLOTS];
    randomToken(session.id);
    randomToken(session.csrf);
    session.issued = millis();
    session.active = true;
    char cookie[128] = {};
    snprintf(cookie, sizeof(cookie), "khairc_sid=%s; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=900", session.id);
    httpd_resp_set_hdr(r, "Set-Cookie", cookie);
    httpd_resp_set_hdr(r, "Location", "/ui");
    return respond(r, "303 See Other", "text/plain", "");
}

esp_err_t logout(httpd_req_t* r) {
    if (!sameOriginWhenPresent(r)) return error(r, "403 Forbidden", "origin_blocked");
    Session* session = currentSession(r);
    if (session) {
        char csrf[64] = {};
        if (!getHeader(r, "X-CSRF-Token", csrf, sizeof(csrf)) ||
            strlen(csrf) != TOKEN_HEX_LEN ||
            !equalConstantTime(csrf, session->csrf, TOKEN_HEX_LEN)) {
            return error(r, "403 Forbidden", "csrf_blocked");
        }
        session->active = false;
    }
    httpd_resp_set_hdr(r, "Set-Cookie", "khairc_sid=; Secure; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
    return respond(r, "200 OK", "application/json", "{\"ok\":true}");
}

esp_err_t status(httpd_req_t* r) {
    Session* s = currentSession(r);
    if (!s) return error(r, "401 Unauthorized", "login_required");
    char response[96];
    snprintf(response, sizeof(response), "{\"authenticated\":true,\"csrf\":\"%s\"}", s->csrf);
    return respond(r, "200 OK", "application/json", response);
}

esp_err_t command(httpd_req_t* r) {
    Session* s = currentSession(r);
    if (!s) return error(r, "401 Unauthorized", "login_required");
    if (!sameOriginWhenPresent(r)) return error(r, "403 Forbidden", "origin_blocked");
    char csrf[64] = {};
    if (!getHeader(r, "X-CSRF-Token", csrf, sizeof(csrf)) ||
        strlen(csrf) != TOKEN_HEX_LEN ||
        !equalConstantTime(csrf, s->csrf, TOKEN_HEX_LEN)) {
        return error(r, "403 Forbidden", "csrf_blocked");
    }
    char type[64] = {};
    if (!getHeader(r, "Content-Type", type, sizeof(type)) ||
        strcmp(type, "application/json") != 0) {
        return error(r, "415 Unsupported Media Type", "unsupported_media_type");
    }
    char body[MAX_BODY_SIZE + 1] = {};
    if (!readBody(r, body)) return error(r, "400 Bad Request", "invalid_json");
    JsonDocument doc;
    if (deserializeJson(doc, body, strlen(body), DeserializationOption::NestingLimit(3)) ||
        !doc.is<JsonObject>()) return error(r, "400 Bad Request", "invalid_json");
    JsonObject obj = doc.as<JsonObject>();
    const char* action = obj["action"].as<const char*>();
    if (!action || obj["action"].as<JsonString>().size() != strlen(action)) {
        return error(r, "422 Unprocessable Entity", "unsupported_action");
    }
    const bool state = strcmp(action,"state") == 0;
    const bool fix = strcmp(action,"fix") == 0;
    const bool swingOn = strcmp(action,"swing_on") == 0;
    const bool swingOff = strcmp(action,"swing_off") == 0;
    if (!(state || fix || swingOn || swingOff))
        return error(r, "422 Unprocessable Entity", "unsupported_action");
    if (state ? obj.size() != 4 : obj.size() != 1)
        return error(r, "422 Unprocessable Entity", "unsupported_action");
    bool power = false;
    int temp = 0;
    uint8_t fan = 0;
    if (state) {
        if (!obj["power"].is<bool>() || !obj["temperature_c"].is<int>() ||
            !obj["fan_speed"].is<const char*>())
            return error(r, "422 Unprocessable Entity", "unsupported_action");
        const char* speed = obj["fan_speed"].as<const char*>();
        if (obj["fan_speed"].as<JsonString>().size() != strlen(speed))
            return error(r, "422 Unprocessable Entity", "unsupported_action");
        if (strcmp(speed,"auto") == 0) fan = 0x80;
        else if (strcmp(speed,"1") == 0) fan = 0x82;
        else if (strcmp(speed,"2") == 0) fan = 0x86;
        else if (strcmp(speed,"3") == 0) fan = 0x81;
        else if (strcmp(speed,"4") == 0) fan = 0x85;
        else if (strcmp(speed,"5") == 0) fan = 0x83;
        else return error(r, "422 Unprocessable Entity", "unsupported_action");
        temp = obj["temperature_c"].as<int>();
        if (temp < 17 || temp > 30) return error(r, "422 Unprocessable Entity", "unsupported_action");
        power = obj["power"].as<bool>();
    }
    if (OtaService::rebootPending() || !IrDiagnostics::isReady())
        return error(r, "503 Service Unavailable", "device_not_ready");
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if (state) result = IrDiagnostics::sendToshibaState(power, temp, fan);
    else if (fix) result = IrDiagnostics::sendToshibaFix();
    else result = IrDiagnostics::sendToshibaSwing(swingOn);
    if (result == ESP_ERR_INVALID_STATE) return error(r, "503 Service Unavailable", "device_not_ready");
    if (result == ESP_ERR_INVALID_ARG) return error(r, "422 Unprocessable Entity", "unsupported_action");
    if (result != ESP_OK) return error(r, "500 Internal Server Error", "ir_send_failed");
    char response[180];
    snprintf(response,sizeof(response),
      "{\"schema_version\":1,\"device_id\":\"toshiba_ac\",\"action\":\"%s\",\"transport\":\"ir\",\"status\":\"sent\",\"state_verified\":false}", action);
    return respond(r, "200 OK", "application/json", response);
}

bool add(httpd_handle_t srv, const char* uri, httpd_method_t method,
         esp_err_t (*callback)(httpd_req_t*)) {
    httpd_uri_t route{};
    route.uri = uri;
    route.method = method;
    route.handler = callback;
    return httpd_register_uri_handler(srv, &route) == ESP_OK;
}
} // namespace

namespace WebUi {
bool registerRoutes(httpd_handle_t server) {
    return add(server, "/ui", HTTP_GET, view) &&
           add(server, "/web/app.js", HTTP_GET, javascript) &&
           add(server, "/web/manifest.webmanifest", HTTP_GET, manifest) &&
           add(server, "/web/icon.svg", HTTP_GET, icon) &&
           add(server, "/web/icon.png", HTTP_GET, png) &&
           add(server, "/web/login", HTTP_POST, login) &&
           add(server, "/web/logout", HTTP_POST, logout) &&
           add(server, "/web/session", HTTP_GET, status) &&
           add(server, "/web/command", HTTP_POST, command);
}
void resetSessions() {
    for (Session& s : sessions) {
        s.active = false;
        memset(s.id, 0, sizeof(s.id));
        memset(s.csrf, 0, sizeof(s.csrf));
    }
}
}
