#pragma once
#include <cstddef>
#include <cstdint>

// Shared secret comparison for HTTP/80, HTTPS/443 and the Web UI.
namespace ApiAuth {

// Compares exactly n bytes without an early exit.
inline bool constantTimeEquals(const char* a, const char* b, std::size_t n) {
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < n; ++i) {
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

// True only when provided is exactly the 64-character HUB_API_KEY.
bool hubKeyMatches(const char* provided, std::size_t length);

}  // namespace ApiAuth
