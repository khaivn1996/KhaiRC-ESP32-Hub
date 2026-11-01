
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <cstring>

#include "CommandApi.h"
#include "DeviceRegistry.h"
#include "secrets.h"

namespace {

constexpr size_t MAX_BODY_SIZE = 512;

static_assert(
    sizeof(HUB_API_KEY) == 65,
    "HUB_API_KEY must contain 64 characters"
);

void sendError(
    WebServer& server,
    int status,
    const char* code
) {
    String json = "{\"error\":{\"code\":\"";
    json += code;
    json += "\"}}";

    server.send(status, "application/json", json);
}

bool authenticate(WebServer& server) {
    const String provided = server.header("X-API-Key");

    const size_t expectedLength = strlen(HUB_API_KEY);

    if (provided.length() != expectedLength) {
        return false;
    }

    // Compare all bytes without early exit.
    uint8_t diff = 0;

    for (size_t i = 0; i < expectedLength; ++i) {
        diff |= static_cast<uint8_t>(
            provided[i] ^ HUB_API_KEY[i]
        );
    }

    return diff == 0;
}

void handleCommand(WebServer& server) {

    // 1. Authenticate before processing command data.
    if (!authenticate(server)) {
        sendError(server, 401, "unauthorized");
        return;
    }

    // 2. Require JSON.
    const String contentType = server.header("Content-Type");

    if (contentType != "application/json" &&
        !contentType.startsWith("application/json;")) {
        sendError(server, 415, "unsupported_media_type");
        return;
    }

    // 3. Apply application-level payload limit.
    if (server.clientContentLength() > MAX_BODY_SIZE) {
        sendError(server, 413, "payload_too_large");
        return;
    }

    const String body = server.arg("plain");

    if (body.length() > MAX_BODY_SIZE) {
        sendError(server, 413, "payload_too_large");
        return;
    }

    if (body.isEmpty()) {
        sendError(server, 400, "invalid_json");
        return;
    }

    // 4. Parse and validate JSON.
    JsonDocument doc;

    DeserializationError error = deserializeJson(
        doc,
        body,
        DeserializationOption::NestingLimit(3)
    );

    if (error || !doc.is<JsonObject>()) {
        sendError(server, 400, "invalid_json");
        return;
    }

    if (!doc["device_id"].is<const char*>() ||
        !doc["action"].is<const char*>()) {
        sendError(server, 400, "invalid_command");
        return;
    }

    String deviceId =
        doc["device_id"].as<const char*>();

    String action =
        doc["action"].as<const char*>();

    if (deviceId.isEmpty() ||
        deviceId.length() > 48 ||
        action.isEmpty() ||
        action.length() > 32) {
        sendError(server, 400, "invalid_command");
        return;
    }

    // 5. Validate registered device.
    if (DeviceRegistry::deviceJson(deviceId).isEmpty()) {
        sendError(server, 404, "device_not_found");
        return;
    }

    // 6. Validate initial command vocabulary.
    if (action != "power_on" &&
        action != "power_off") {
        sendError(server, 422, "unsupported_action");
        return;
    }

    // 7. No hardware driver is available yet.
    // Do not modify device state or claim success.
    sendError(server, 503, "device_not_ready");
}

}

namespace CommandApi {

void registerRoutes(WebServer& server) {

    // Collect request headers required by the API.
    static const char* headerKeys[] = {
        "X-API-Key",
        "Content-Type"
    };

    server.collectHeaders(headerKeys, 2);

    WebServer* serverPtr = &server;

    server.on(
        "/api/v1/commands",
        HTTP_POST,
        [serverPtr]() {
            handleCommand(*serverPtr);
        }
    );

    Serial.println("[CommandAPI] Route registered");
}

}
