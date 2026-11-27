#include "WebUi.h"
#include "WebUiPage.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "IrDiagnostics.h"
#include "OtaService.h"
#include "ToshibaCodec.h"
#include "TuyaLocal.h"

namespace {
constexpr size_t MAX_BODY_SIZE = 512;

void harden(httpd_req_t* r) {
    httpd_resp_set_hdr(r, "Cache-Control", "no-store, max-age=0");
    httpd_resp_set_hdr(r, "Pragma", "no-cache");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
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

bool getHeader(httpd_req_t* r, const char* name, char* out, size_t capacity) {
    const size_t len = httpd_req_get_hdr_value_len(r, name);
    return len > 0 && len < capacity &&
           httpd_req_get_hdr_value_str(r, name, out, capacity) == ESP_OK;
}

// Browser Origin must match the Host that was used to load the HTTPS page.
bool sameOrigin(httpd_req_t* r) {
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

esp_err_t view(httpd_req_t* r) {
    httpd_resp_set_hdr(r, "Content-Security-Policy",
        "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; "
        "connect-src 'self'; img-src 'self' data:; manifest-src 'self'; "
        "form-action 'none'; base-uri 'none'; frame-ancestors 'none'");
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

// Rang Dong OC09 "O cam 4" (Tuya LAN 3.4). Explicit set ON/OFF, one attempt, no retry.
esp_err_t plugCommand(httpd_req_t* r, JsonObject obj) {
    constexpr char kPlugId[] = "rangdong_oc09_s4";
    const char* device = obj["device_id"].as<const char*>();
    const char* action = obj["action"].as<const char*>();
    if (obj.size() != 2 || !device || !action ||
        obj["device_id"].as<JsonString>().size() != strlen(device) ||
        obj["action"].as<JsonString>().size() != strlen(action) ||
        strcmp(device, kPlugId) != 0) {
        return error(r, "422 Unprocessable Entity", "unsupported_action");
    }
    const bool on = strcmp(action, "power_on") == 0;
    if (!on && strcmp(action, "power_off") != 0)
        return error(r, "422 Unprocessable Entity", "unsupported_action");
    if (OtaService::rebootPending()) return error(r, "503 Service Unavailable", "device_not_ready");
    if (!TuyaLocal::configured()) return error(r, "503 Service Unavailable", "tuya_not_configured");
    const TuyaLocal::Result result = TuyaLocal::setPower(on);
    if (result.status == TuyaLocal::Status::Busy)
        return error(r, "503 Service Unavailable", "device_busy");
    if (result.status != TuyaLocal::Status::Ok) {
        return error(r, TuyaLocal::isTimeout(result.status) ? "504 Gateway Timeout" : "502 Bad Gateway",
                     TuyaLocal::errorCode(result.status));
    }
    char response[200];
    snprintf(response, sizeof(response),
      "{\"schema_version\":1,\"device_id\":\"%s\",\"action\":\"%s\",\"transport\":\"tuya_lan\","
      "\"status\":\"sent\",\"state_verified\":%s%s}",
      kPlugId, on ? "power_on" : "power_off", result.verified ? "true" : "false",
      result.verified ? (result.on ? ",\"power\":true" : ",\"power\":false") : "");
    return respond(r, "200 OK", "application/json", response);
}

esp_err_t command(httpd_req_t* r) {
    if (!sameOrigin(r)) return error(r, "403 Forbidden", "origin_blocked");
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
    // Toshiba bodies never carry device_id; only the OC09 plug card sends it.
    if (!obj["device_id"].isNull()) return plugCommand(r, obj);
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
        if (!ToshibaCodec::fanFromName(speed, fan))
            return error(r, "422 Unprocessable Entity", "unsupported_action");
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
           add(server, "/web/command", HTTP_POST, command);
}
}
