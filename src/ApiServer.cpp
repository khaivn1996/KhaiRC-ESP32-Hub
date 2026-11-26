#include "CommandApi.h"
#include "DeviceRegistry.h"
#include "IrDiagnostics.h"
#include "LandingPage.h"
#include "OtaService.h"
#include "OtaSafety.h"
#include "MiotLocal.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_random.h>

#include "WifiService.h"
#include "ApiServer.h"

namespace {
    WebServer server(80);
    // The router must not forward this separate OTA port from the WAN.
    WebServer otaServer(8081);
    bool serverRunning = false;
    uint32_t bootId = 0;

    void sendHealth(WebServer& target) {
        String json = "{";
        json += "\"status\":\"ok\",";
        json += "\"device\":\"KhaiRC-ESP32-Hub\",";
        json += "\"firmware\":\"";
        json += ApiServer::FIRMWARE_VERSION;
        json += "\",";
        json += "\"boot_id\":\"";
        json += String(bootId, HEX);
        json += "\",";
        json += "\"uptime_sec\":";
        json += String(millis() / 1000);
        json += ",\"free_heap\":";
        json += String(ESP.getFreeHeap());
        json += ",\"build_id\":\"";
        json += OtaSafety::buildId();
        json += "\",\"ota_slot\":\"";
        json += OtaSafety::slotName();
        json += "\",\"ota_state\":\"";
        json += OtaSafety::stateName();
        json += "\"}";

        target.send(200, "application/json", json);
    }

    void sendMethodNotAllowed(const char* allow) {
        server.sendHeader("Allow", allow);
        server.send(405, "application/json", "{\"error\":{\"code\":\"method_not_allowed\"}}");
    }
}

namespace ApiServer {

bool ready() { return serverRunning && WifiService::connected(); }

String systemJson() {
    String json = "{";
    json += "\"chip\":\"";
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
    return json;
}

String capabilitiesJson() {
    String json = "{\"schema_version\":1,\"capabilities\":{\"ir_send\":";
    json += IrDiagnostics::isReady() ? "true" : "false";
    json += ",\"tuya_control\":false,\"cloud_access\":false,\"ota_update\":";
    json += OtaService::available() ? "true" : "false";
    json += "}}";
    return json;
}

void begin() {

    bootId = esp_random();

    static const char* headerKeys[] = {
        "X-API-Key", "Content-Type", "X-OTA-Password",
        "Content-Length", "Transfer-Encoding"
    };
    server.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));
    otaServer.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "no-store");
        server.sendHeader("X-Content-Type-Options", "nosniff");
        server.sendHeader("X-Frame-Options", "DENY");
        server.sendHeader("Content-Security-Policy",
            "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
            "img-src data:; connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
        server.send_P(200, "text/html; charset=utf-8", LandingPage::html());
    });
    server.on("/api/v1/health", HTTP_GET, []() {
        sendHealth(server);
    });
    // The OTA UI needs health on its own same-origin port to verify reboot.
    otaServer.on("/api/v1/health", HTTP_GET, []() {
        sendHealth(otaServer);
    });

    server.on("/api/v1/system", HTTP_GET, []() {
        server.send(200, "application/json", systemJson());
    });

    server.on("/api/v1/devices", HTTP_GET, []() {
        server.send(
            200,
            "application/json",
            DeviceRegistry::listJson()
        );
    });

    server.on("/api/v1/capabilities", HTTP_GET, []() {
        server.send(200, "application/json", capabilitiesJson());
    });

    CommandApi::registerRoutes(server);
    MiotLocal::registerRoutes(server);
    OtaService::registerRoutes(otaServer);

    server.onNotFound([]() {
        String path = server.uri();

        if (path == "/") {
            sendMethodNotAllowed("GET");
            return;
        }

        if (path == "/api/v1/commands") {
            sendMethodNotAllowed("POST");
            return;
        }

        const String devicePrefix = "/api/v1/devices/";

        if (path.startsWith(devicePrefix)) {

            if (server.method() != HTTP_GET) {
                sendMethodNotAllowed("GET");
                return;
            }

            String id = path.substring(devicePrefix.length());
            String json = DeviceRegistry::deviceJson(id);

            if (json.length() == 0) {
                server.send(
                    404,
                    "application/json",
                    "{\"error\":{\"code\":\"device_not_found\"}}"
                );
                return;
            }

            server.send(200, "application/json", json);
            return;
        }

        bool knownPath =
            path == "/api/v1/health" ||
            path == "/api/v1/system" ||
            path == "/api/v1/devices" ||
            path == "/api/v1/capabilities";

        if (knownPath && server.method() != HTTP_GET) {
            sendMethodNotAllowed("GET");
            return;
        }

        server.send(
            404,
            "application/json",
            "{\"error\":{\"code\":\"not_found\"}}"
        );
    });

    Serial.println("[API] Routes registered");
}

void loop() {

    // Also services a scheduled reboot if Wi-Fi drops after a successful OTA.
    OtaService::loop();
    if (OtaService::rebootPending()) {
        return;
    }

    if (!WifiService::connected()) {
        if (serverRunning) {
            server.stop();
            otaServer.stop();
            serverRunning = false;
            Serial.println("[API] Server stopped");
        }
        return;
    }

    if (!serverRunning) {
        server.begin();
        otaServer.begin();
        serverRunning = true;
        Serial.println("[API] HTTP server started on port 80");
        Serial.println("[OTA] HTTP web update started on LAN port 8081");
    }

    server.handleClient();
    otaServer.handleClient();
}

}
