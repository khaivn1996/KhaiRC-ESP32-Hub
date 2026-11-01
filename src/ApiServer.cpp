
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
        json += "\"firmware\":\"0.1.0\",";
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

    server.onNotFound([]() {
        server.send(404, "application/json",
                    "{\"error\":\"not_found\"}");
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
