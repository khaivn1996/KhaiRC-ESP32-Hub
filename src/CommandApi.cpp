
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <cstring>

#include "ApiAuth.h"
#include "CommandApi.h"
#include "DeviceRegistry.h"
#include "IrDiagnostics.h"

namespace {

constexpr size_t MAX_BODY_SIZE = 512;

// The route has a raw callback, so WebServer streams the body here in fixed
// chunks instead of growing a heap buffer to the client's Content-Length
// before authentication. Only the first MAX_BODY_SIZE bytes are kept; the
// final handler still rejects anything larger with 413.
char rawBody[MAX_BODY_SIZE + 1] = {};
size_t rawLength = 0;
bool rawComplete = false;

void resetBody() {
    rawLength = 0;
    rawComplete = false;
    rawBody[0] = '\0';
}

// Clears the captured body on every exit path of the final handler.
struct BodyReset {
    ~BodyReset() { resetBody(); }
};

void receiveBody(WebServer& server) {
    // Multipart file uploads also reach this callback; they carry no raw body.
    if (!server.hasRaw()) {
        return;
    }
    HTTPRaw& raw = server.raw();
    switch (raw.status) {
    case RAW_START:
        resetBody();
        break;
    case RAW_WRITE: {
        const size_t room = MAX_BODY_SIZE - rawLength;
        const size_t count = raw.currentSize < room ? raw.currentSize : room;
        memcpy(rawBody + rawLength, raw.buf, count);
        rawLength += count;
        rawBody[rawLength] = '\0';
        break;
    }
    case RAW_END:
        rawComplete = true;
        break;
    case RAW_ABORTED:
        resetBody();
        break;
    }
}

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
    return ApiAuth::hubKeyMatches(provided.c_str(), provided.length());
}

void handleCommand(WebServer& server) {
    const BodyReset bodyReset;

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

    // Built from a C string: like WebServer's former "plain" argument, the
    // body ends at the first NUL byte.
    const String body = rawComplete ? String(rawBody) : String();

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

    // Reject embedded NULs instead of silently accepting a truncated command.
    if (doc["device_id"].as<JsonString>().size() !=
            strlen(doc["device_id"].as<const char*>()) ||
        doc["action"].as<JsonString>().size() !=
            strlen(doc["action"].as<const char*>())) {
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

    // Milife is READ-ONLY in this rollout; reject every POST action.
    if (deviceId == "milife_plug_10a") {
        sendError(server, 422, "unsupported_action");
        return;
    }

    // 6. Validate vocabulary for this device before touching hardware.
    const bool fan = deviceId == "fan_ir";
    const bool toshiba = deviceId == "toshiba_ac";
    const bool supported = fan
        ? (action == "power_on" || action == "swing" || action == "power_off")
        : (toshiba ? action == "fix"
                   : (action == "power_on" || action == "power_off"));

    if (!supported) {
        sendError(server, 422, "unsupported_action");
        return;
    }

    // 7. Daikin and Tuya still have no executable driver.
    if ((!fan && !toshiba) || !IrDiagnostics::isReady()) {
        sendError(server, 503, "device_not_ready");
        return;
    }

    // 8. Execute exactly once; only success after the RMT wait completes.
    esp_err_t result;
    if (toshiba) {
        result = IrDiagnostics::sendToshibaFix();
    } else if (action == "power_on") {
        result = IrDiagnostics::sendFanHigh();
    } else if (action == "power_off") {
        result = IrDiagnostics::sendFanOff();
    } else {
        result = IrDiagnostics::sendFanSwing();
    }

    if (result != ESP_OK) {
        sendError(server, 500, "ir_send_failed");
        return;
    }

    // Both identifiers were matched against the fixed command vocabulary.
    // "sent" describes the transmitter, not the actual appliance state.
    String json = "{\"schema_version\":1,\"device_id\":\"";
    json += deviceId;
    json += "\",\"action\":\"";
    json += action;
    json += "\",\"transport\":\"ir\",\"status\":\"sent\",\"state_verified\":false}";
    server.send(200, "application/json", json);
}

}

namespace CommandApi {

void registerRoutes(WebServer& server) {

    // ApiServer collects the headers shared by commands and OTA.
    WebServer* serverPtr = &server;

    server.on(
        "/api/v1/commands",
        HTTP_POST,
        [serverPtr]() {
            handleCommand(*serverPtr);
        },
        [serverPtr]() {
            receiveBody(*serverPtr);
        }
    );

    Serial.println("[CommandAPI] Route registered");
}

}
