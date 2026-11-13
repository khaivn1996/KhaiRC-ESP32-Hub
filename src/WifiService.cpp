
#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include "WifiService.h"
#include "OtaSafety.h"

namespace {
    constexpr unsigned long RETRY_MS = 30000;
    unsigned long lastRetry = 0;
    bool wasConnected = false;
}

namespace WifiService {

void begin() {
    Serial.println("[WiFi] Initializing...");
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("[WiFi EVENT] STA_START");
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("[WiFi EVENT] STA_CONNECTED");
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            OtaSafety::wifiDisconnectReason(static_cast<std::uint8_t>(info.wifi_sta_disconnected.reason));
            Serial.printf(
                "[WiFi EVENT] DISCONNECTED reason=%u\n",
                static_cast<unsigned>(info.wifi_sta_disconnected.reason)
            );
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("[WiFi EVENT] GOT_IP");
            break;

        default:
            break;
    }});
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
