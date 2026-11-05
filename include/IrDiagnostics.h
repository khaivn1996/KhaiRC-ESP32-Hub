
#pragma once

#include "esp_err.h"

namespace IrDiagnostics {
    void begin();
    bool isReady();
    esp_err_t sendFanHigh();
    esp_err_t sendFanSwing();
    esp_err_t sendFanOff();
    esp_err_t sendToshibaFix();
    void testBurst();
    void testDc();
    void testFanHigh();
    void testFanSwing();
    void testToshibaFix();
}
