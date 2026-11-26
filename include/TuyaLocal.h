#pragma once

#include <Arduino.h>

// Rang Dong OC09 "O cam 2" over Tuya LAN protocol 3.4. Web UI (HTTPS) only.
namespace TuyaLocal {

enum class Status : uint8_t {
    Ok,
    NotConfigured,
    Busy,
    WifiUnavailable,
    NetworkError,
    Timeout,        // Failed before CONTROL_NEW was sent: plug state unchanged.
    NoAck,          // CONTROL_NEW was sent but no reply: outcome unknown.
    AuthFailed,
    DeviceRejected,
    ProtocolError,
};

struct Result {
    Status status;
    bool verified;  // Plug reported the DP value after the write.
    bool on;        // Valid only when verified.
};

bool configured();
// Exactly one TCP connection and one CONTROL_NEW per call. Never retried.
Result setPower(bool on);
const char* errorCode(Status status);
bool isTimeout(Status status);

}  // namespace TuyaLocal
