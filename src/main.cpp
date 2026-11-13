#include "DeviceRegistry.h"
#include <Arduino.h>
#include "IrDiagnostics.h"
#include "WifiService.h"
#include "ApiServer.h"
#include "HttpsService.h"
#include "OtaSafety.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("======================");
    Serial.println(" KhaiRC ESP32 Hub");
    Serial.println(" Firmware v0.4.0-ota1");
    Serial.println("======================");

    OtaSafety::begin();
    WifiService::begin();
    OtaSafety::wifiBegin();
    DeviceRegistry::begin();
    ApiServer::begin();
    HttpsService::begin();
    IrDiagnostics::begin();

    Serial.println("[IR TEST] Manual mode: S = TAIZ SWING, F = Toshiba FIX");
    Serial.println("[IR TEST] No automatic IR transmission at boot");
    Serial.println("[CommandAPI] Fan HIGH/SWING/OFF and Toshiba FIX available over HTTP when IR is ready");
}

void loop() {
    WifiService::loop();
    ApiServer::loop();
    HttpsService::loop();
    OtaSafety::loop();

    // Manual tests only. CR/LF and unrelated characters are ignored.
    if (Serial.available()) {
        char command = Serial.read();

        if (command == 's' || command == 'S') {
            IrDiagnostics::testFanSwing();
        } else if (command == 'f' || command == 'F') {
            IrDiagnostics::testToshibaFix();
        }
    }

    delay(5);
}
