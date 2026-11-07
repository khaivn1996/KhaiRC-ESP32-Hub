#pragma once

#include <Arduino.h>

class WebServer;

namespace MiotLocal {

enum class Status : uint8_t {
    Ok,
    NotConfigured,
    WifiUnavailable,
    NetworkError,
    HelloTimeout,
    DeviceMismatch,
    ReadTimeout,
    InvalidResponse,
    AuthFailed,
    DeviceRejected,
};

struct PowerResult {
    Status status;
    bool on;
};

bool configured();
PowerResult readPower();
void registerRoutes(WebServer& server);

}  // namespace MiotLocal
