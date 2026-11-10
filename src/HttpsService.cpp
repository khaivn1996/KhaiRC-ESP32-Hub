#include "HttpsService.h"
#include "WifiService.h"
#include <Arduino.h>

// Stage 1: TLS read-only probe. Never exposes commands or OTA.
// Missing credentials must NOT prevent the known-working Hub from booting.
#if __has_include(<esp_https_server.h>) && __has_include("tls_credentials.h")
#define KHAIRC_HTTPS_PROBE_ENABLED 1
#include <esp_https_server.h>
#include <esp_http_server.h>
#include "tls_credentials.h"
#else
#define KHAIRC_HTTPS_PROBE_ENABLED 0
#endif

namespace {
#if KHAIRC_HTTPS_PROBE_ENABLED
    httpd_handle_t secureServer = nullptr;
    constexpr unsigned long RETRY_MS = 30000;
    bool attempted = false;
    unsigned long lastAttempt = 0;

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

    void startServer() {
        httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
        config.port_secure = 443;
        config.httpd.stack_size = 8192;
        config.httpd.max_uri_handlers = 2;
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

        httpd_uri_t health{};
        health.uri = "/api/v1/health";
        health.method = HTTP_GET;
        health.handler = getHealth;
        if (httpd_register_uri_handler(secureServer, &health) != ESP_OK) {
            httpd_ssl_stop(secureServer);
            secureServer = nullptr;
            Serial.println("[HTTPS] Unable to register health route");
            return;
        }
        Serial.println("[HTTPS] Probe on 443 (only GET /api/v1/health)");
    }
#endif
}

namespace HttpsService {
    void begin() {
#if KHAIRC_HTTPS_PROBE_ENABLED
        Serial.println("[HTTPS] Probe configured; starting when Wi-Fi is ready");
#else
        Serial.println("[HTTPS] Probe disabled: missing TLS credentials or HTTPS component");
#endif
    }

    void loop() {
#if KHAIRC_HTTPS_PROBE_ENABLED
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
