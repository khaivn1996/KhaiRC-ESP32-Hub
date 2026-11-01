#include "DeviceRegistry.h"
#include <Arduino.h>
#include "IrDiagnostics.h"
#include "WifiService.h"
#include "ApiServer.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("======================");
    Serial.println(" KhaiRC ESP32 Hub");
    Serial.println(" Firmware v0.2.0-dev");
    Serial.println("======================");

    WifiService::begin();
    DeviceRegistry::begin();
    ApiServer::begin();
    IrDiagnostics::begin();
}

void loop() {
    WifiService::loop();
    ApiServer::loop();

    delay(5);
}
