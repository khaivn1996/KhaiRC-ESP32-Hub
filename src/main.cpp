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

    Serial.println("[IR TEST] Toshiba FIX ready");
}

void loop() {
    WifiService::loop();
    ApiServer::loop();

    // Chi phat FIX 1 lan sau moi lan khoi dong.
    static bool testSent = false;

    if (!testSent && millis() >= 10000) {
        testSent = true;

        Serial.println("[IR TEST] Sending Toshiba FIX");

        IrDiagnostics::testToshibaFix();
    }

    // Thu cong bang 'f' neu ket noi PC.
    if (Serial.available()) {
        char command = Serial.read();

        if (command == 'f' || command == 'F') {
            IrDiagnostics::testToshibaFix();
        }
    }

    delay(5);
}
