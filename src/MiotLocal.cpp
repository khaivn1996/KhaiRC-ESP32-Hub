#include "MiotLocal.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_system.h>
#include <mbedtls/aes.h>
#include <mbedtls/md5.h>
#include <cstring>
#include <cstdio>

#include "WifiService.h"
#include "secrets.h"  // HUB_API_KEY only; never print it.

#if __has_include("miot_config.h")
#include "miot_config.h"  // Optional untracked local device credentials.
#endif
#ifndef MILIFE_IP
#define MILIFE_IP ""
#endif
#ifndef MILIFE_DID
#define MILIFE_DID 0UL
#endif
#ifndef MILIFE_TOKEN
#define MILIFE_TOKEN ""
#endif

namespace {

constexpr uint16_t kMiioPort = 54321;
constexpr uint32_t kHelloTimeoutMs = 1200;
constexpr uint32_t kReadTimeoutMs = 3200;
constexpr size_t kPacketLimit = 1024;
constexpr size_t kPlainLimit = 256;
constexpr size_t kHeaderSize = 32;
constexpr uint8_t kHello[32] = {
    0x21, 0x31, 0x00, 0x20,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
};

uint16_t be16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}
uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
void setBe16(uint8_t* p, uint16_t n) {
    p[0] = static_cast<uint8_t>(n >> 8);
    p[1] = static_cast<uint8_t>(n);
}
void setBe32(uint8_t* p, uint32_t n) {
    p[0] = static_cast<uint8_t>(n >> 24);
    p[1] = static_cast<uint8_t>(n >> 16);
    p[2] = static_cast<uint8_t>(n >> 8);
    p[3] = static_cast<uint8_t>(n);
}

bool parseToken(uint8_t out[16]) {
    const char* hex = MILIFE_TOKEN;
    if (strlen(hex) != 32) return false;
    for (size_t i = 0; i < 16; ++i) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int hi = nibble(hex[2 * i]);
        const int lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool getConfig(IPAddress& ip, uint32_t& did, uint8_t token[16]) {
    did = static_cast<uint32_t>(MILIFE_DID);
    return did != 0 && ip.fromString(MILIFE_IP) &&
           ip != IPAddress(0, 0, 0, 0) && parseToken(token);
}

bool sameSender(WiFiUDP& udp, const IPAddress& expected) {
    return udp.remoteIP() == expected && udp.remotePort() == kMiioPort;
}

// This firmware sends exactly one read-only request per HTTP GET.
MiotLocal::Status hello(WiFiUDP& udp, const IPAddress& ip, uint32_t did,
                        uint32_t& stamp, uint32_t& tickMs) {
    if (!udp.beginPacket(ip, kMiioPort) ||
        udp.write(kHello, sizeof(kHello)) != sizeof(kHello) ||
        !udp.endPacket()) return MiotLocal::Status::NetworkError;

    const uint32_t began = millis();
    while (millis() - began < kHelloTimeoutMs) {
        const int n = udp.parsePacket();
        if (n == 0) { delay(5); continue; }
        if (!sameSender(udp, ip)) { udp.clear(); continue; }
        uint8_t reply[kHeaderSize];
        if (n != static_cast<int>(kHeaderSize) ||
            udp.read(reply, sizeof(reply)) != static_cast<int>(sizeof(reply)) ||
            reply[0] != 0x21 || reply[1] != 0x31 ||
            be16(reply + 2) != kHeaderSize) {
            return MiotLocal::Status::InvalidResponse;
        }
        if (be32(reply + 8) != did) return MiotLocal::Status::DeviceMismatch;
        stamp = be32(reply + 12);
        tickMs = millis();
        return MiotLocal::Status::Ok;
    }
    return MiotLocal::Status::HelloTimeout;
}

bool md5(const uint8_t* data, size_t n, uint8_t out[16]) {
    return mbedtls_md5(data, n, out) == 0;
}

bool encrypt(const uint8_t key[16], const uint8_t iv[16],
             const uint8_t* plain, size_t n, uint8_t* encrypted) {
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, sizeof(ivCopy));
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    const int setup = mbedtls_aes_setkey_enc(&ctx, key, 128);
    const int result = setup == 0
        ? mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, n, ivCopy,
                                plain, encrypted)
        : setup;
    mbedtls_aes_free(&ctx);
    return result == 0;
}

bool decrypt(const uint8_t key[16], const uint8_t iv[16],
             const uint8_t* cipher, size_t n, uint8_t* plain) {
    uint8_t ivCopy[16];
    memcpy(ivCopy, iv, sizeof(ivCopy));
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    const int setup = mbedtls_aes_setkey_dec(&ctx, key, 128);
    const int result = setup == 0
        ? mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, n, ivCopy,
                                cipher, plain)
        : setup;
    mbedtls_aes_free(&ctx);
    return result == 0;
}

bool deriveKey(const uint8_t token[16], uint8_t key[16], uint8_t iv[16]) {
    uint8_t material[32];
    if (!md5(token, 16, key)) return false;
    memcpy(material, key, 16);
    memcpy(material + 16, token, 16);
    return md5(material, sizeof(material), iv);
}

bool buildReadPacket(uint32_t did, uint32_t stamp, uint32_t id,
                     const uint8_t token[16], uint8_t* out, size_t& length) {
    char payload[kPlainLimit];
    const int written = snprintf(payload, sizeof(payload),
        "{\"id\":%lu,\"method\":\"get_properties\",\"params\":[{\"did\":\"%lu\",\"siid\":2,\"piid\":1}]}",
        static_cast<unsigned long>(id), static_cast<unsigned long>(did));
    if (written <= 0 || written >= static_cast<int>(sizeof(payload))) return false;
    const size_t plainLength = static_cast<size_t>(written);
    const size_t padding = 16 - (plainLength % 16);
    const size_t paddedLength = plainLength + padding;
    if (kHeaderSize + paddedLength > kPacketLimit) return false;

    uint8_t clear[kPlainLimit];
    memcpy(clear, payload, plainLength);
    memset(clear + plainLength, static_cast<int>(padding), padding);
    uint8_t key[16], iv[16];
    if (!deriveKey(token, key, iv)) return false;
    if (!encrypt(key, iv, clear, paddedLength, out + kHeaderSize)) return false;

    length = kHeaderSize + paddedLength;
    out[0] = 0x21; out[1] = 0x31;
    setBe16(out + 2, static_cast<uint16_t>(length));
    memset(out + 4, 0, 4);
    setBe32(out + 8, did);
    setBe32(out + 12, stamp);
    memcpy(out + 16, token, 16);
    uint8_t checksum[16];
    if (!md5(out, length, checksum)) return false;
    memcpy(out + 16, checksum, sizeof(checksum));
    return true;
}

MiotLocal::PowerResult parseResponse(uint8_t* packet, size_t n, uint32_t did,
                                    uint32_t expectedId, const uint8_t token[16]) {
    using MiotLocal::Status;
    if (n < kHeaderSize + 16 || packet[0] != 0x21 || packet[1] != 0x31 ||
        be16(packet + 2) != n || (n - kHeaderSize) % 16 != 0 ||
        be32(packet + 8) != did) return {Status::InvalidResponse, false};

    uint8_t digest[16];
    memcpy(digest, packet + 16, 16);
    memcpy(packet + 16, token, 16);
    uint8_t expected[16];
    if (!md5(packet, n, expected)) return {Status::InvalidResponse, false};
    uint8_t diff = 0;
    for (size_t i = 0; i < 16; ++i) diff |= digest[i] ^ expected[i];
    if (diff) return {Status::AuthFailed, false};

    uint8_t key[16], iv[16];
    if (!deriveKey(token, key, iv)) return {Status::InvalidResponse, false};
    const size_t cipherLength = n - kHeaderSize;
    uint8_t plain[kPacketLimit - kHeaderSize];
    if (!decrypt(key, iv, packet + kHeaderSize, cipherLength, plain))
        return {Status::InvalidResponse, false};
    const uint8_t pad = plain[cipherLength - 1];
    if (pad == 0 || pad > 16 || pad > cipherLength)
        return {Status::InvalidResponse, false};
    for (size_t i = cipherLength - pad; i < cipherLength; ++i) {
        if (plain[i] != pad) return {Status::InvalidResponse, false};
    }
    const size_t length = cipherLength - pad;
    JsonDocument doc;
    if (deserializeJson(doc, reinterpret_cast<const char*>(plain), length) ||
        !doc.is<JsonObject>()) return {Status::InvalidResponse, false};
    if (!doc["id"].is<uint32_t>() || doc["id"].as<uint32_t>() != expectedId)
        return {Status::InvalidResponse, false};
    if (doc["error"].is<JsonObject>()) return {Status::DeviceRejected, false};
    if (!doc["result"].is<JsonArray>()) return {Status::InvalidResponse, false};
    JsonArray results = doc["result"].as<JsonArray>();
    for (JsonVariant item : results) {
        if (!item.is<JsonObject>()) continue;
        const char* responseDid = item["did"].as<const char*>();
        // Some firmware omits did in property items; the UDP header DID
        // has already been authenticated and checked above.
        if ((responseDid != nullptr && String(responseDid) != String(did)) ||
            item["siid"].as<int>() != 2 || item["piid"].as<int>() != 1) continue;
        if (!item["code"].is<int>()) return {Status::InvalidResponse, false};
        if (item["code"].as<int>() != 0) return {Status::DeviceRejected, false};
        if (!item["value"].is<bool>()) return {Status::InvalidResponse, false};
        return {Status::Ok, item["value"].as<bool>()};
    }
    return {Status::InvalidResponse, false};
}

bool authorized(WebServer& server) {
    const String provided = server.header("X-API-Key");
    constexpr size_t requiredLength = sizeof(HUB_API_KEY) - 1;
    static_assert(sizeof(HUB_API_KEY) == 65, "Expected a 64-character HUB_API_KEY");
    if (provided.length() != requiredLength) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < requiredLength; ++i) {
        diff |= static_cast<uint8_t>(provided[i] ^ HUB_API_KEY[i]);
    }
    return diff == 0;
}

void sendError(WebServer& server, int code, const char* reason) {
    String json = "{\"error\":{\"code\":\"";
    json += reason;
    json += "\"}}";
    server.send(code, "application/json", json);
}

}  // namespace

namespace MiotLocal {

bool configured() {
    IPAddress ip;
    uint32_t did = 0;
    uint8_t token[16] = {};
    return getConfig(ip, did, token);
}

PowerResult readPower() {
    IPAddress ip;
    uint32_t did = 0;
    uint8_t token[16] = {};
    if (!getConfig(ip, did, token)) return {Status::NotConfigured, false};
    if (!WifiService::connected()) return {Status::WifiUnavailable, false};

    WiFiUDP udp;
    if (!udp.begin(0)) return {Status::NetworkError, false};
    uint32_t stamp = 0, tickMs = 0;
    const Status handshake = hello(udp, ip, did, stamp, tickMs);
    if (handshake != Status::Ok) {
        udp.stop();
        return {handshake, false};
    }

    const uint32_t id = (esp_random() % 999999UL) + 1;
    uint8_t packet[kPacketLimit];
    size_t length = 0;
    if (!buildReadPacket(did, stamp + (millis() - tickMs) / 1000,
                         id, token, packet, length)) {
        udp.stop();
        return {Status::InvalidResponse, false};
    }
    if (!udp.beginPacket(ip, kMiioPort) || udp.write(packet, length) != length ||
        !udp.endPacket()) {
        udp.stop();
        return {Status::NetworkError, false};
    }

    const uint32_t start = millis();
    while (millis() - start < kReadTimeoutMs) {
        const int received = udp.parsePacket();
        if (received == 0) { delay(5); continue; }
        if (!sameSender(udp, ip)) { udp.clear(); continue; }
        if (received > static_cast<int>(sizeof(packet))) {
            udp.stop();
            return {Status::InvalidResponse, false};
        }
        if (udp.read(packet, received) != received) {
            udp.stop();
            return {Status::InvalidResponse, false};
        }
        const PowerResult result = parseResponse(packet, static_cast<size_t>(received),
                                                 did, id, token);
        udp.stop();
        return result;
    }
    udp.stop();
    return {Status::ReadTimeout, false};
}

void registerRoutes(WebServer& server) {
    WebServer* handle = &server;
    server.on("/api/v1/devices/milife_plug_10a/state", HTTP_GET, [handle]() {
        if (!authorized(*handle)) {
            sendError(*handle, 401, "unauthorized");
            return;
        }
        if (!configured()) {
            sendError(*handle, 503, "miot_not_configured");
            return;
        }
        const PowerResult value = readPower();
        if (value.status != Status::Ok) {
            const int http = (value.status == Status::HelloTimeout ||
                              value.status == Status::ReadTimeout) ? 504 : 502;
            const char* code = "miot_protocol_error";
            switch (value.status) {
                case Status::WifiUnavailable: code = "wifi_unavailable"; break;
                case Status::NetworkError: code = "miot_network_error"; break;
                case Status::HelloTimeout: code = "miot_hello_timeout"; break;
                case Status::DeviceMismatch: code = "miot_device_mismatch"; break;
                case Status::ReadTimeout: code = "miot_read_timeout"; break;
                case Status::AuthFailed: code = "miot_auth_failed"; break;
                case Status::DeviceRejected: code = "miot_device_rejected"; break;
                default: break;
            }
            sendError(*handle, http, code);
            return;
        }
        const String json = String("{\"schema_version\":1,\"device_id\":\"milife_plug_10a\","
                                   "\"transport\":\"miio_lan\",\"power\":") +
                                   (value.on ? "true" : "false") +
                                   ",\"state_verified\":true}";
        handle->send(200, "application/json", json);
    });
}

}  // namespace MiotLocal
