#include "HttpsService.h"
#include "WifiService.h"
#include <Arduino.h>

// Stage 2A: pinned HTTPS read APIs. No HTTPS commands, power writes or OTA.
// Keep the proven HTTP/80 and LAN OTA/8081 available for recovery.
#if __has_include(<esp_https_server.h>) && __has_include("tls_credentials.h")
#define KHAIRC_HTTPS_ENABLED 1
#include <esp_https_server.h>
#include <esp_http_server.h>
#include <WiFi.h>
#include <cstring>
#include "DeviceRegistry.h"
#include "IrDiagnostics.h"
#include "MiotLocal.h"
#include "OtaService.h"
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
            "\"phase\":\"https_probe\",\"uptime_sec\":%lu,\"free_heap\":%u}",
            static_cast<unsigned long>(millis() / 1000UL),
            static_cast<unsigned int>(ESP.getFreeHeap()));
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
        config.httpd.max_uri_handlers = 5;
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
            !registerGet("/api/v1/devices/milife_plug_10a/state", getMilifeState)) {
            httpd_ssl_stop(secureServer);
            secureServer = nullptr;
            Serial.println("[HTTPS] Stage 2A registration failed");
            return;
        }
        Serial.println("[HTTPS] Stage 2A read-only API on 443 (Milife state requires key)");
    }
#endif
}

namespace HttpsService {
    void begin() {
#if KHAIRC_HTTPS_ENABLED
        Serial.println("[HTTPS] Stage 2A configured; starting when Wi-Fi is ready");
#else
        Serial.println("[HTTPS] Stage 2A disabled: missing TLS credentials or HTTPS component");
#endif
    }

    void loop() {
#if KHAIRC_HTTPS_ENABLED
        const bool online = WifiService::connected();
        if (!online && secureServer != nullptr) {
            httpd_ssl_stop(secureServer);
            secureServer = nullptr;
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
