#include "TuyaLocal.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_random.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "WifiService.h"

#if __has_include("tuya_config.h")
#include "tuya_config.h"  // Optional untracked local device credentials.
#endif
#ifndef TUYA_OC09_IP
#define TUYA_OC09_IP ""
#endif
#ifndef TUYA_OC09_DEVICE_ID
#define TUYA_OC09_DEVICE_ID ""
#endif
#ifndef TUYA_OC09_LOCAL_KEY
#define TUYA_OC09_LOCAL_KEY ""
#endif
#ifndef TUYA_OC09_DP
#define TUYA_OC09_DP 4
#endif

namespace {

using TuyaLocal::Status;

constexpr uint16_t kPort = 6668;
constexpr int32_t kConnectTimeoutMs = 1500;
constexpr uint32_t kNegotiateTimeoutMs = 2000;
constexpr uint32_t kReplyTimeoutMs = 3000;
constexpr uint32_t kAfterAckMs = 1200;
constexpr size_t kBufferSize = 512;
constexpr size_t kHeaderSize = 16;
constexpr size_t kHmacSize = 32;
constexpr size_t kTrailerSize = kHmacSize + 4;
constexpr uint32_t kPrefix = 0x000055AA;
constexpr uint32_t kSuffix = 0x0000AA55;
constexpr uint32_t kCmdNegStart = 3;
constexpr uint32_t kCmdNegResp = 4;
constexpr uint32_t kCmdNegFinish = 5;
constexpr uint32_t kCmdStatus = 8;
constexpr uint32_t kCmdControlNew = 13;
constexpr char kVersionHeader[] = "3.4";
constexpr size_t kVersionHeaderSize = 15;  // "3.4" + 12 zero bytes.
constexpr int kDp = TUYA_OC09_DP;
static_assert(kDp >= 1 && kDp <= 255, "TUYA_OC09_DP must be 1..255");

// Static buffers are touched only while Gate is held (keeps httpd stack small).
uint8_t txFrame[kBufferSize];
uint8_t rxFrame[kBufferSize];
uint8_t work[kBufferSize];

std::atomic_flag busy = ATOMIC_FLAG_INIT;
class Gate {
public:
    Gate() : acquired(!busy.test_and_set(std::memory_order_acquire)) {}
    ~Gate() { if (acquired) busy.clear(std::memory_order_release); }
    bool ok() const { return acquired; }
    Gate(const Gate&) = delete;
    Gate& operator=(const Gate&) = delete;
private:
    bool acquired;
};

uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
void setBe32(uint8_t* p, uint32_t n) {
    p[0] = static_cast<uint8_t>(n >> 24);
    p[1] = static_cast<uint8_t>(n >> 16);
    p[2] = static_cast<uint8_t>(n >> 8);
    p[3] = static_cast<uint8_t>(n);
}

bool getConfig(IPAddress& ip, uint8_t key[16]) {
    const char* localKey = TUYA_OC09_LOCAL_KEY;
    if (strlen(localKey) != 16 || strlen(TUYA_OC09_DEVICE_ID) == 0 ||
        !ip.fromString(TUYA_OC09_IP) || ip == IPAddress(0, 0, 0, 0)) return false;
    memcpy(key, localKey, 16);
    return true;
}

bool hmacSha256(const uint8_t key[16], const uint8_t* data, size_t n, uint8_t out[kHmacSize]) {
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return info != nullptr && mbedtls_md_hmac(info, key, 16, data, n, out) == 0;
}

bool sameBytes(const uint8_t* a, const uint8_t* b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

// In-place AES-128-ECB over whole blocks.
bool aesEcb(bool encrypt, const uint8_t key[16], uint8_t* data, size_t n) {
    if (n == 0 || n % 16 != 0) return false;
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int result = encrypt ? mbedtls_aes_setkey_enc(&ctx, key, 128)
                         : mbedtls_aes_setkey_dec(&ctx, key, 128);
    for (size_t i = 0; result == 0 && i < n; i += 16) {
        result = mbedtls_aes_crypt_ecb(&ctx, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
                                       data + i, data + i);
    }
    mbedtls_aes_free(&ctx);
    return result == 0;
}

// data holds n plaintext bytes and has room for PKCS7 padding; encrypts in place.
bool encryptPadded(const uint8_t key[16], uint8_t* data, size_t n, size_t capacity, size_t& outLength) {
    const size_t pad = 16 - (n % 16);
    if (n + pad > capacity) return false;
    memset(data + n, static_cast<int>(pad), pad);
    outLength = n + pad;
    return aesEcb(true, key, data, outLength);
}

// Decrypts in place and strips PKCS7 padding.
bool decryptPadded(const uint8_t key[16], uint8_t* data, size_t n, size_t& outLength) {
    if (!aesEcb(false, key, data, n)) return false;
    const uint8_t pad = data[n - 1];
    if (pad == 0 || pad > 16 || pad > n) return false;
    for (size_t i = n - pad; i < n; ++i) {
        if (data[i] != pad) return false;
    }
    outLength = n - pad;
    return true;
}

bool sendFrame(WiFiClient& client, uint32_t seq, uint32_t cmd,
               const uint8_t* payload, size_t n, const uint8_t key[16]) {
    const size_t total = kHeaderSize + n + kTrailerSize;
    if (total > sizeof(txFrame)) return false;
    setBe32(txFrame, kPrefix);
    setBe32(txFrame + 4, seq);
    setBe32(txFrame + 8, cmd);
    setBe32(txFrame + 12, static_cast<uint32_t>(n + kTrailerSize));
    memcpy(txFrame + kHeaderSize, payload, n);
    if (!hmacSha256(key, txFrame, kHeaderSize + n, txFrame + kHeaderSize + n)) return false;
    setBe32(txFrame + kHeaderSize + n + kHmacSize, kSuffix);
    return client.write(txFrame, total) == total;
}

enum class ReadResult : uint8_t { Ok, Timeout, Closed };

ReadResult readExact(WiFiClient& client, uint8_t* out, size_t n, uint32_t deadline) {
    size_t got = 0;
    while (got < n) {
        if (static_cast<int32_t>(millis() - deadline) >= 0) return ReadResult::Timeout;
        const int available = client.available();
        if (available > 0) {
            const size_t want = n - got;
            const int read = client.read(out + got, want < static_cast<size_t>(available)
                                                        ? want : static_cast<size_t>(available));
            if (read > 0) got += static_cast<size_t>(read);
            continue;
        }
        if (!client.connected()) return ReadResult::Closed;
        delay(5);
    }
    return ReadResult::Ok;
}

struct Frame {
    uint32_t cmd;
    uint32_t retcode;
    uint8_t* payload;
    size_t length;
};

Status readFrame(WiFiClient& client, uint32_t deadline, const uint8_t key[16], Frame& frame) {
    ReadResult read = readExact(client, rxFrame, kHeaderSize, deadline);
    if (read == ReadResult::Timeout) return Status::Timeout;
    if (read == ReadResult::Closed) return Status::NetworkError;
    const uint32_t length = be32(rxFrame + 12);
    if (be32(rxFrame) != kPrefix || length < kTrailerSize ||
        length > sizeof(rxFrame) - kHeaderSize) return Status::ProtocolError;
    read = readExact(client, rxFrame + kHeaderSize, length, deadline);
    if (read == ReadResult::Timeout) return Status::Timeout;
    if (read == ReadResult::Closed) return Status::NetworkError;
    const size_t payloadLength = length - kTrailerSize;
    const size_t signedLength = kHeaderSize + payloadLength;
    if (be32(rxFrame + signedLength + kHmacSize) != kSuffix) return Status::ProtocolError;
    uint8_t expected[kHmacSize];
    if (!hmacSha256(key, rxFrame, signedLength, expected)) return Status::ProtocolError;
    if (!sameBytes(expected, rxFrame + signedLength, kHmacSize)) return Status::AuthFailed;
    frame.cmd = be32(rxFrame + 8);
    frame.retcode = 0;
    frame.payload = rxFrame + kHeaderSize;
    frame.length = payloadLength;
    // Device replies may prefix a clear 4-byte return code before encrypted blocks.
    if (frame.length % 16 == 4) {
        frame.retcode = be32(frame.payload);
        frame.payload += 4;
        frame.length -= 4;
    }
    return Status::Ok;
}

// Reads the DP value from a decrypted STATUS/CONTROL_NEW payload, if present.
bool parseDp(uint8_t* plain, size_t length, bool& on) {
    if (length >= kVersionHeaderSize && memcmp(plain, kVersionHeader, 3) == 0) {
        plain += kVersionHeaderSize;
        length -= kVersionHeaderSize;
    }
    JsonDocument doc;
    if (deserializeJson(doc, reinterpret_cast<const char*>(plain), length,
                        DeserializationOption::NestingLimit(4)) ||
        !doc.is<JsonObject>()) return false;
    char key[4];
    snprintf(key, sizeof(key), "%d", kDp);
    JsonVariant value = doc["data"]["dps"][key];
    if (!value.is<bool>()) value = doc["dps"][key];
    if (!value.is<bool>()) return false;
    on = value.as<bool>();
    return true;
}

// Session key negotiation (protocol 3.4). Fills sessionKey on success.
Status negotiate(WiFiClient& client, const uint8_t localKey[16], uint8_t sessionKey[16]) {
    uint8_t localNonce[16];
    esp_fill_random(localNonce, sizeof(localNonce));
    size_t length = 0;
    memcpy(work, localNonce, sizeof(localNonce));
    if (!encryptPadded(localKey, work, sizeof(localNonce), sizeof(work), length)) return Status::ProtocolError;
    if (!sendFrame(client, 1, kCmdNegStart, work, length, localKey)) return Status::NetworkError;

    const uint32_t deadline = millis() + kNegotiateTimeoutMs;
    Frame frame{};
    for (;;) {
        const Status status = readFrame(client, deadline, localKey, frame);
        if (status != Status::Ok) return status;
        if (frame.cmd == kCmdNegResp) break;
    }
    if (frame.retcode != 0) return Status::DeviceRejected;
    if (frame.length == 0 || frame.length % 16 != 0 ||
        !decryptPadded(localKey, frame.payload, frame.length, length) || length < 48) {
        return Status::AuthFailed;
    }
    uint8_t remoteNonce[16];
    memcpy(remoteNonce, frame.payload, sizeof(remoteNonce));
    uint8_t check[kHmacSize];
    if (!hmacSha256(localKey, localNonce, sizeof(localNonce), check)) return Status::ProtocolError;
    if (!sameBytes(check, frame.payload + 16, kHmacSize)) return Status::AuthFailed;

    if (!hmacSha256(localKey, remoteNonce, sizeof(remoteNonce), work) ||
        !encryptPadded(localKey, work, kHmacSize, sizeof(work), length)) return Status::ProtocolError;
    if (!sendFrame(client, 2, kCmdNegFinish, work, length, localKey)) return Status::NetworkError;

    for (size_t i = 0; i < 16; ++i) sessionKey[i] = localNonce[i] ^ remoteNonce[i];
    return aesEcb(true, localKey, sessionKey, 16) ? Status::Ok : Status::ProtocolError;
}

// After CONTROL_NEW is on the wire, only Ok, NoAck or DeviceRejected are reported.
TuyaLocal::Result control(WiFiClient& client, const uint8_t sessionKey[16], bool on) {
    memset(work, 0, kVersionHeaderSize);
    memcpy(work, kVersionHeader, 3);
    const int written = snprintf(reinterpret_cast<char*>(work + kVersionHeaderSize),
        sizeof(work) - kVersionHeaderSize,
        "{\"protocol\":5,\"t\":%lu,\"data\":{\"dps\":{\"%d\":%s}}}",
        static_cast<unsigned long>(time(nullptr)), kDp, on ? "true" : "false");
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(work) - kVersionHeaderSize - 16) {
        return {Status::ProtocolError, false, false};
    }
    size_t length = 0;
    if (!encryptPadded(sessionKey, work, kVersionHeaderSize + static_cast<size_t>(written),
                       sizeof(work), length)) return {Status::ProtocolError, false, false};
    if (!sendFrame(client, 3, kCmdControlNew, work, length, sessionKey)) {
        // A partial write may still reach the plug.
        return {Status::NoAck, false, false};
    }

    uint32_t deadline = millis() + kReplyTimeoutMs;
    bool acked = false;
    for (;;) {
        Frame frame{};
        if (readFrame(client, deadline, sessionKey, frame) != Status::Ok) break;
        if (frame.cmd != kCmdControlNew && frame.cmd != kCmdStatus) continue;
        if (frame.cmd == kCmdControlNew) {
            if (frame.retcode != 0) return {Status::DeviceRejected, false, false};
            if (!acked) {
                acked = true;
                const uint32_t shorter = millis() + kAfterAckMs;
                if (static_cast<int32_t>(shorter - deadline) < 0) deadline = shorter;
            }
        }
        size_t plainLength = 0;
        bool value = false;
        if (frame.length != 0 && frame.length % 16 == 0 &&
            decryptPadded(sessionKey, frame.payload, frame.length, plainLength) &&
            parseDp(frame.payload, plainLength, value)) {
            return {Status::Ok, true, value};
        }
    }
    return acked ? TuyaLocal::Result{Status::Ok, false, false}
                 : TuyaLocal::Result{Status::NoAck, false, false};
}

}  // namespace

namespace TuyaLocal {

bool configured() {
    IPAddress ip;
    uint8_t key[16] = {};
    const bool ok = getConfig(ip, key);
    memset(key, 0, sizeof(key));
    return ok;
}

const char* errorCode(Status status) {
    switch (status) {
        case Status::NotConfigured: return "tuya_not_configured";
        case Status::Busy: return "device_busy";
        case Status::WifiUnavailable: return "wifi_unavailable";
        case Status::NetworkError: return "tuya_network_error";
        case Status::Timeout: return "tuya_timeout";
        case Status::NoAck: return "tuya_no_ack";
        case Status::AuthFailed: return "tuya_auth_failed";
        case Status::DeviceRejected: return "tuya_device_rejected";
        default: return "tuya_protocol_error";
    }
}

bool isTimeout(Status status) {
    return status == Status::Timeout || status == Status::NoAck;
}

Result setPower(bool on) {
    Gate gate;
    if (!gate.ok()) return {Status::Busy, false, false};
    IPAddress ip;
    uint8_t localKey[16] = {};
    if (!getConfig(ip, localKey)) return {Status::NotConfigured, false, false};
    if (!WifiService::connected()) {
        memset(localKey, 0, sizeof(localKey));
        return {Status::WifiUnavailable, false, false};
    }

    WiFiClient client;
    Result result{Status::NetworkError, false, false};
    if (client.connect(ip, kPort, kConnectTimeoutMs)) {
        client.setNoDelay(true);
        uint8_t sessionKey[16] = {};
        const Status negotiated = negotiate(client, localKey, sessionKey);
        result = negotiated == Status::Ok ? control(client, sessionKey, on)
                                          : Result{negotiated, false, false};
        memset(sessionKey, 0, sizeof(sessionKey));
    }
    client.stop();
    memset(localKey, 0, sizeof(localKey));
    memset(work, 0, sizeof(work));
    Serial.printf("[Tuya] OC09 DP%d %s -> %s%s\n", kDp, on ? "ON" : "OFF",
                  result.status == Status::Ok ? "ok" : errorCode(result.status),
                  result.status == Status::Ok ? (result.verified ? " (verified)" : " (ack only)") : "");
    return result;
}

}  // namespace TuyaLocal
