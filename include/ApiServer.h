
#pragma once

#include <Arduino.h>

namespace ApiServer {
    // Shown in the Serial banner and HTTP/80 + OTA/8081 health.
    constexpr char FIRMWARE_VERSION[] = "0.4.0-ota1";

    void begin();
    void loop();
    bool ready();
    // Shared by HTTP/80 and HTTPS/443 so both serve identical JSON.
    String systemJson();
    String capabilitiesJson();
}
