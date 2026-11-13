#pragma once
#include <cstdint>

namespace OtaSafety {
    // Call once, before WifiService::begin(); never auto-send IR.
    void begin();
    // Call after ApiServer::loop() and HttpsService::loop().
    void loop();
    void wifiBegin();
    // Called in the Wi-Fi callback: RAM only, no NVS writes.
    void wifiDisconnectReason(std::uint8_t reason);
    // OTA is blocked if rollback support cannot be verified.
    bool canAcceptUpdate();
    bool pendingVerification();
    const char* stateName();
    const char* slotName();
    const char* buildId();
}
