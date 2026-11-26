
#pragma once

#include <Arduino.h>

namespace ApiServer {
    // Shown in the Serial banner and HTTP/80 + OTA/8081 health.
    constexpr char FIRMWARE_VERSION[] = "1.0.0";

    void begin();
    void loop();
    bool ready();
    // Shared by HTTP/80 and HTTPS/443 so both serve identical JSON.
    String systemJson();
    String capabilitiesJson();
}
