#pragma once
#include <cstdint>
#include <cstring>

// Pure C++ Toshiba encoder mirrored from ToshibaAcEncoder.kt (Stage 2C).
// Network input is restricted to legal temperatures and captured fan bytes.
namespace ToshibaCodec {
inline bool validFan(std::uint8_t fan) {
    return fan == 0x80 || fan == 0x82 || fan == 0x86 ||
           fan == 0x81 || fan == 0x85 || fan == 0x83;
}
inline std::uint8_t checksum(const std::uint8_t* frame, std::size_t n) {
    std::uint8_t xorValue = 0;
    for (std::size_t i = 0; i < n; ++i) xorValue ^= frame[i];
    return xorValue;
}
inline bool stateFrame(bool power, int temperatureC, std::uint8_t fan,
                       std::uint8_t (&out)[9]) {
    if (temperatureC < 17 || temperatureC > 30 || !validFan(fan)) return false;
    const std::uint8_t offset = static_cast<std::uint8_t>(temperatureC - 17);
    std::uint8_t reverseNibble = 0;
    for (int bit = 0; bit < 4; ++bit) {
        if ((offset & (1U << bit)) != 0) {
            reverseNibble |= static_cast<std::uint8_t>(1U << (3 - bit));
        }
    }
    const std::uint8_t prefix[8] = {0x4F, 0xB0, 0xC0, 0x3F, 0x80,
                                    reverseNibble,
                                    static_cast<std::uint8_t>(power ? fan : 0xE0), 0x00};
    std::memcpy(out, prefix, sizeof(prefix));
    out[8] = checksum(out, 8);
    return true;
}
inline void swingFrame(bool enabled, std::uint8_t (&out)[7]) {
    const std::uint8_t prefix[6] = {0x4F, 0xB0, 0x80, 0x7F, 0x84,
                                    static_cast<std::uint8_t>(enabled ? 0x80 : 0x40)};
    std::memcpy(out, prefix, sizeof(prefix));
    out[6] = checksum(out, 6);
}
} // namespace ToshibaCodec
