
#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include "WifiService.h"

namespace {
    constexpr unsigned long RETRY_MS = 30000;
    unsigned long lastRetry = 0;
    bool wasConnected = false;
}

namespace WifiService {

void begin() {
    Serial.println("[WiFi] Initializing...");

    WiFi.setHostname("khairc-hub");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    lastRetry = millis();
}

bool connected() {
    return WiFi.status() == WL_CONNECTED;
}

void loop() {
    bool online = connected();

    if (online && !wasConnected) {
        Serial.println("[WiFi] Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
    }

    if (!online && wasConnected) {
        Serial.println("[WiFi] Disconnected!");
    }

    wasConnected = online;

    if (!online &&
        millis() - lastRetry >= RETRY_MS) {

        lastRetry = millis();
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
    }
}

}
