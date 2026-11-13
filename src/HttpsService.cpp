#include "HttpsService.h"
#include "WifiService.h"
#include "OtaSafety.h"
#include <Arduino.h>

// Stage 2C: HTTPS read APIs plus Toshiba-only state/FIX/SWING. No MiLife writes or OTA.
// Keep the proven HTTP/80 and LAN OTA/8081 available for recovery.
#if __has_include(<esp_https_server.h>) && __has_include("tls_credentials.h")
#define KHAIRC_HTTPS_ENABLED 1
#include <esp_https_server.h>
#include <esp_http_server.h>
#include <WiFi.h>
#include <cstring>
#include <ArduinoJson.h>
#include "DeviceRegistry.h"
#include "IrDiagnostics.h"
#include "MiotLocal.h"
#include "OtaService.h"
#include "WebUi.h"
#include "secrets.h"
#include "tls_credentials.h"
#else
#define KHAIRC_HTTPS_ENABLED 0
#endif

namespace {
#if KHAIRC_HTTPS_ENABLED
    httpd_handle_t secureServer = nullptr;
    constexpr unsigned long RETRY_MS = 30000;
    bool attempted = false;
    unsigned long lastAttempt = 0;
    static_assert(sizeof(HUB_API_KEY) == 65, "HUB_API_KEY must have exactly 64 characters");

    esp_err_t sendJson(httpd_req_t* request, const char* status, const String& body) {
        httpd_resp_set_status(request, status);
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.length()));
    }

    esp_err_t sendError(httpd_req_t* request, const char* status, const char* code) {
        String body = "{\"error\":{\"code\":\"";
        body += code;
        body += "\"}}";
        return sendJson(request, status, body);
    }

    // Exactly 64 bytes must match; no early return for a correctly sized key.
    bool authorized(httpd_req_t* request) {
        constexpr size_t KEY_SIZE = sizeof(HUB_API_KEY) - 1;
        if (httpd_req_get_hdr_value_len(request, "X-API-Key") != KEY_SIZE) return false;
        char provided[KEY_SIZE + 1] = {};
        if (httpd_req_get_hdr_value_str(request, "X-API-Key", provided,
                                        sizeof(provided)) != ESP_OK) return false;
        uint8_t diff = 0;
        for (size_t i = 0; i < KEY_SIZE; ++i) {
            diff |= static_cast<uint8_t>(provided[i] ^ HUB_API_KEY[i]);
        }
        return diff == 0;
    }

    esp_err_t getHealth(httpd_req_t* request) {
        char response[240];
        const int count = snprintf(
            response, sizeof(response),
            "{\"status\":\"ok\",\"device\":\"KhaiRC-ESP32-Hub\","
            "\"phase\":\"https_probe\",\"uptime_sec\":%lu,\"free_heap\":%u,"
            "\"build_id\":\"%s\",\"ota_state\":\"%s\"}",
            static_cast<unsigned long>(millis() / 1000UL),
            static_cast<unsigned int>(ESP.getFreeHeap()),
            OtaSafety::buildId(), OtaSafety::stateName());
        if (count < 0 || static_cast<size_t>(count) >= sizeof(response)) return ESP_FAIL;
        httpd_resp_set_type(request, "application/json");
        httpd_resp_set_hdr(request, "Cache-Control", "no-store");
        return httpd_resp_send(request, response, count);
    }

    esp_err_t getSystem(httpd_req_t* request) {
        String json = "{\"chip\":\"";
        json += ESP.getChipModel();
        json += "\",\"flash_mb\":";
        json += String(ESP.getFlashChipSize() / 1048576);
        json += ",\"psram_mb\":";
        json += String(ESP.getPsramSize() / 1048576);
        json += ",\"ip\":\"";
        json += WiFi.localIP().toString();
        json += "\",\"rssi_dbm\":";
        json += String(WiFi.RSSI());
        json += "}";
        return sendJson(request, "200 OK", json);
    }

    esp_err_t getDevices(httpd_req_t* request) {
        return sendJson(request, "200 OK", DeviceRegistry::listJson());
    }

    esp_err_t getCapabilities(httpd_req_t* request) {
        String json = "{\"schema_version\":1,\"capabilities\":{\"ir_send\":";
        json += IrDiagnostics::isReady() ? "true" : "false";
        json += ",\"tuya_control\":false,\"cloud_access\":false,\"ota_update\":";
        json += OtaService::available() ? "true" : "false";
        json += "}}";
        return sendJson(request, "200 OK", json);
    }

    esp_err_t getMilifeState(httpd_req_t* request) {
        if (!authorized(request)) return sendError(request, "401 Unauthorized", "unauthorized");
        if (!MiotLocal::configured()) {
            return sendError(request, "503 Service Unavailable", "miot_not_configured");
        }
        // Exactly ONE read-only miIO request. No toggle, power write or auto-retry.
        const MiotLocal::PowerResult result = MiotLocal::readPower();
        if (result.status != MiotLocal::Status::Ok) {
            const bool timeout = result.status == MiotLocal::Status::HelloTimeout ||
                                 result.status == MiotLocal::Status::ReadTimeout;
            const char* code = "miot_protocol_error";
            switch (result.status) {
                case MiotLocal::Status::WifiUnavailable: code = "wifi_unavailable"; break;
                case MiotLocal::Status::NetworkError: code = "miot_network_error"; break;
                case MiotLocal::Status::HelloTimeout: code = "miot_hello_timeout"; break;
                case MiotLocal::Status::DeviceMismatch: code = "miot_device_mismatch"; break;
                case MiotLocal::Status::ReadTimeout: code = "miot_read_timeout"; break;
                case MiotLocal::Status::AuthFailed: code = "miot_auth_failed"; break;
                case MiotLocal::Status::DeviceRejected: code = "miot_device_rejected"; break;
                default: break;
            }
            return sendError(request, timeout ? "504 Gateway Timeout" : "502 Bad Gateway", code);
        }
        const String json = String("{\"schema_version\":1,\"device_id\":\"milife_plug_10a\","
                                   "\"transport\":\"miio_lan\",\"power\":") +
                            (result.on ? "true" : "false") + ",\"state_verified\":true}";
        return sendJson(request, "200 OK", json);
    }


    // Only Toshiba is permitted on HTTPS writes; no raw IR, MiLife, TAIZ, Daikin or OTA.
    // No command is queued or retried. API key checked before parsing a body.
    esp_err_t postToshibaAction(httpd_req_t* request) {
        constexpr size_t MAX_BODY_SIZE = 512;
        if (!authorized(request)) {
            return sendError(request, "401 Unauthorized", "unauthorized");
        }
        if (httpd_req_get_hdr_value_len(request, "Transfer-Encoding") != 0) {
            return sendError(request, "400 Bad Request", "invalid_command");
        }
        const size_t typeLength = httpd_req_get_hdr_value_len(request, "Content-Type");
        if (typeLength == 0 || typeLength >= 64) {
            return sendError(request, "415 Unsupported Media Type", "unsupported_media_type");
        }
        char contentType[64] = {};
        if (httpd_req_get_hdr_value_str(request, "Content-Type",
                                        contentType, sizeof(contentType)) != ESP_OK ||
            (strcmp(contentType, "application/json") != 0 &&
             strncmp(contentType, "application/json;", 17) != 0)) {
            return sendError(request, "415 Unsupported Media Type", "unsupported_media_type");
        }
        if (request->content_len <= 0) {
            return sendError(request, "400 Bad Request", "invalid_json");
        }
        if (request->content_len > static_cast<int>(MAX_BODY_SIZE)) {
            return sendError(request, "413 Payload Too Large", "payload_too_large");
        }
        char body[MAX_BODY_SIZE + 1] = {};
        int offset = 0;
        while (offset < request->content_len) {
            const int read = httpd_req_recv(request, body + offset,
                                   static_cast<size_t>(request->content_len - offset));
            if (read <= 0) return ESP_FAIL; // close uncertain socket; no replay
            offset += read;
        }
        if (memchr(body, '\0', static_cast<size_t>(offset)) != nullptr) {
            return sendError(request, "400 Bad Request", "invalid_json");
        }
        JsonDocument doc;
        if (deserializeJson(doc, body, static_cast<size_t>(offset),
                            DeserializationOption::NestingLimit(3)) ||
            !doc.is<JsonObject>()) {
            return sendError(request, "400 Bad Request", "invalid_json");
        }
        if (!doc["device_id"].is<const char*>() || !doc["action"].is<const char*>()) {
            return sendError(request, "400 Bad Request", "invalid_command");
        }
        const char* device = doc["device_id"].as<const char*>();
        const char* action = doc["action"].as<const char*>();
        if (doc["device_id"].as<JsonString>().size() != strlen(device) ||
            doc["action"].as<JsonString>().size() != strlen(action)) {
            return sendError(request, "400 Bad Request", "invalid_command");
        }
        if (strcmp(device, "toshiba_ac") != 0) {
            return sendError(request, "422 Unprocessable Entity", "unsupported_action");
        }
        const bool stateAction = strcmp(action, "state") == 0;
        const bool fix = strcmp(action, "fix") == 0;
        const bool swingOn = strcmp(action, "swing_on") == 0;
        const bool swingOff = strcmp(action, "swing_off") == 0;
        if (!(stateAction || fix || swingOn || swingOff)) {
            return sendError(request, "422 Unprocessable Entity", "unsupported_action");
        }
        bool power = false;
        int temperatureC = 0;
        uint8_t fanByte = 0;
        if (stateAction) {
            if (doc.as<JsonObject>().size() != 5 ||
                !doc["power"].is<bool>() ||
                !doc["temperature_c"].is<int>() ||
                !doc["fan_speed"].is<const char*>()) {
                return sendError(request, "422 Unprocessable Entity", "unsupported_action");
            }
            const char* fan = doc["fan_speed"].as<const char*>();
            if (doc["fan_speed"].as<JsonString>().size() != strlen(fan)) {
                return sendError(request, "422 Unprocessable Entity", "unsupported_action");
            }
            if (strcmp(fan,"auto") == 0) fanByte = 0x80;
            else if (strcmp(fan,"1") == 0) fanByte = 0x82;
            else if (strcmp(fan,"2") == 0) fanByte = 0x86;
            else if (strcmp(fan,"3") == 0) fanByte = 0x81;
            else if (strcmp(fan,"4") == 0) fanByte = 0x85;
            else if (strcmp(fan,"5") == 0) fanByte = 0x83;
            else return sendError(request, "422 Unprocessable Entity", "unsupported_action");
            temperatureC = doc["temperature_c"].as<int>();
            if (temperatureC < 17 || temperatureC > 30) {
                return sendError(request, "422 Unprocessable Entity", "unsupported_action");
            }
            power = doc["power"].as<bool>();
        } else if (doc.as<JsonObject>().size() != 2) {
            return sendError(request, "422 Unprocessable Entity", "unsupported_action");
        }
        if (OtaService::rebootPending() || !IrDiagnostics::isReady()) {
            return sendError(request, "503 Service Unavailable", "device_not_ready");
        }
        esp_err_t sent = ESP_ERR_INVALID_ARG;
        if (fix) sent = IrDiagnostics::sendToshibaFix();
        else if (swingOn || swingOff) sent = IrDiagnostics::sendToshibaSwing(swingOn);
        else if (stateAction) sent = IrDiagnostics::sendToshibaState(power, temperatureC, fanByte);
        if (sent == ESP_ERR_INVALID_STATE) {
            return sendError(request, "503 Service Unavailable", "device_not_ready");
        }
        if (sent == ESP_ERR_INVALID_ARG) {
            return sendError(request, "422 Unprocessable Entity", "unsupported_action");
        }
        if (sent != ESP_OK) {
            return sendError(request, "500 Internal Server Error", "ir_send_failed");
        }
        // Ack only means ESP32 completed one requested RMT send, not AC state.
        String reply = "{\"schema_version\":1,\"device_id\":\"toshiba_ac\",\"action\":\"";
        reply += action; // previously matched fixed vocabulary only
        reply += "\",\"transport\":\"ir\",\"status\":\"sent\",\"state_verified\":false}";
        return sendJson(request, "200 OK", reply);
    }

    bool registerPost(const char* uri, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t route{};
        route.uri = uri;
        route.method = HTTP_POST;
        route.handler = handler;
        return httpd_register_uri_handler(secureServer, &route) == ESP_OK;
    }

    bool registerGet(const char* uri, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t route{};
        route.uri = uri;
        route.method = HTTP_GET;
        route.handler = handler;
        return httpd_register_uri_handler(secureServer, &route) == ESP_OK;
    }

    void startServer() {
        httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
        config.port_secure = 443;
        config.httpd.stack_size = 8192;
        config.httpd.max_uri_handlers = 15;
        config.httpd.max_req_hdr_len = 1536;
        config.httpd.max_open_sockets = 2;
        config.httpd.lru_purge_enable = true;
        config.servercert = reinterpret_cast<const uint8_t*>(HUB_TLS_CERT_PEM);
        config.servercert_len = sizeof(HUB_TLS_CERT_PEM);
        config.prvtkey_pem = reinterpret_cast<const uint8_t*>(HUB_TLS_KEY_PEM);
        config.prvtkey_len = sizeof(HUB_TLS_KEY_PEM);

        const esp_err_t started = httpd_ssl_start(&secureServer, &config);
        if (started != ESP_OK) {
            secureServer = nullptr;
            Serial.printf("[HTTPS] TLS server start failed: %d\n", static_cast<int>(started));
            return;
        }
        // Fail closed: never keep a partially registered HTTPS API online.
        if (!registerGet("/api/v1/health", getHealth) ||
            !registerGet("/api/v1/system", getSystem) ||
            !registerGet("/api/v1/capabilities", getCapabilities) ||
            !registerGet("/api/v1/devices", getDevices) ||
            !registerGet("/api/v1/devices/milife_plug_10a/state", getMilifeState) ||
            !registerPost("/api/v1/commands", postToshibaAction) ||
            !WebUi::registerRoutes(secureServer)) {
            httpd_ssl_stop(secureServer);
            secureServer = nullptr;
            Serial.println("[HTTPS] Stage 2C registration failed");
            return;
        }
        Serial.println("[HTTPS] Stage 3A on 443: Toshiba Web UI + Stage 2C APIs");
    }
#endif
}

namespace HttpsService {
    bool ready() {
#if KHAIRC_HTTPS_ENABLED
        return secureServer != nullptr;
#else
        return false;
#endif
    }
    void begin() {
#if KHAIRC_HTTPS_ENABLED
        Serial.println("[HTTPS] Stage 2C configured; starting when Wi-Fi is ready");
#else
        Serial.println("[HTTPS] Stage 2C disabled: missing TLS credentials or HTTPS component");
#endif
    }

    void loop() {
#if KHAIRC_HTTPS_ENABLED
        const bool online = WifiService::connected();
        if (!online && secureServer != nullptr) {
            httpd_ssl_stop(secureServer);
            secureServer = nullptr;
            WebUi::resetSessions();
            attempted = false;
            Serial.println("[HTTPS] Stopped after Wi-Fi disconnect");
        }
        if (!online || secureServer != nullptr) return;
        if (attempted && millis() - lastAttempt < RETRY_MS) return;
        attempted = true;
        lastAttempt = millis();
        startServer();
#endif
    }
}
