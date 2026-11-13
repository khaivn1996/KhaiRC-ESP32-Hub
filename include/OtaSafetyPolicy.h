#pragma once
#include <cstdint>

// Pure policy for host tests; all elapsed time comparisons are rollover-safe.
namespace OtaSafetyPolicy {
constexpr uint32_t STABLE_MS = 15000U;
constexpr uint32_t DEADLINE_MS = 90000U;
class HealthGate {
public:
    void reset() { stable_ = false; }
    bool ready(uint32_t now, bool wifi, bool ip, bool http, bool https, bool ir) {
        if (!(wifi && ip && http && https && ir)) {
            stable_ = false;
            return false;
        }
        if (!stable_) {
            since_ = now;
            stable_ = true;
            return false;
        }
        return static_cast<uint32_t>(now - since_) >= STABLE_MS;
    }
private:
    bool stable_ = false;
    uint32_t since_ = 0;
};
inline bool expired(uint32_t started, uint32_t now) {
    return static_cast<uint32_t>(now - started) >= DEADLINE_MS;
}
}
