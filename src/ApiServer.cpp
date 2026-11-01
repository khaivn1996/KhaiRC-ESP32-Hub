#include "DeviceRegistry.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "WifiService.h"
#include "ApiServer.h"

namespace {
    WebServer server(80);
    bool serverRunning = false;
}

namespace ApiServer {

void begin() {

    server.on("/api/v1/health", HTTP_GET, []() {
        String json = "{";
        json += "\"status\":\"ok\",";
        json += "\"device\":\"KhaiRC-ESP32-Hub\",";
        json += "\"firmware\":\"0.2.0-dev\",";
        json += "\"uptime_sec\":";
        json += String(millis() / 1000);
        json += ",\"free_heap\":";
        json += String(ESP.getFreeHeap());
        json += "}";

        server.send(200, "application/json", json);
    });

    server.on("/api/v1/system", HTTP_GET, []() {
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

        server.send(200, "application/json", json);
    });

    server.on("/api/v1/devices", HTTP_GET, []() {
        server.send(
            200,
            "application/json",
            DeviceRegistry::listJson()
        );
    });

    server.on("/api/v1/capabilities", HTTP_GET, []() {
        server.send(
            200,
            "application/json",
            "{\"schema_version\":1,"
            "\"capabilities\":{"
            "\"ir_send\":false,"
            "\"tuya_control\":false,"
            "\"cloud_access\":false"
            "}}"
        );
    });

    server.onNotFound([]() {
        String path = server.uri();

        const String devicePrefix = "/api/v1/devices/";

        if (path.startsWith(devicePrefix)) {

            if (server.method() != HTTP_GET) {
                server.sendHeader("Allow", "GET");
                server.send(
                    405,
                    "application/json",
                    "{\"error\":{\"code\":\"method_not_allowed\"}}"
                );
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
            server.sendHeader("Allow", "GET");
            server.send(
                405,
                "application/json",
                "{\"error\":{\"code\":\"method_not_allowed\"}}"
            );
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

    if (!WifiService::connected()) {
        if (serverRunning) {
            server.stop();
            serverRunning = false;
            Serial.println("[API] Server stopped");
        }
        return;
    }

    if (!serverRunning) {
        server.begin();
        serverRunning = true;
        Serial.println("[API] HTTP server started");
    }

    server.handleClient();
}

}
