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
    WriteTimeout,
    WriteUnconfirmed,
};

struct PowerResult {
    Status status;
    bool on;
};

bool configured();
PowerResult readPower();
Status setPower(bool on);
// Shared by HTTP/80 and HTTPS/443 so both report identical errors and JSON.
const char* errorCode(Status status);
bool isTimeout(Status status);
String stateJson(bool on);
void registerRoutes(WebServer& server);

}  // namespace MiotLocal
