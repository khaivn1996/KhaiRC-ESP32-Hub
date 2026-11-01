
#include <Arduino.h>

#include "WifiService.h"
#include "ApiServer.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("======================");
    Serial.println(" KhaiRC ESP32 Hub");
    Serial.println(" Firmware v0.1.0");
    Serial.println("======================");

    WifiService::begin();
    ApiServer::begin();
}

void loop() {
    WifiService::loop();
    ApiServer::loop();

    delay(5);
}
