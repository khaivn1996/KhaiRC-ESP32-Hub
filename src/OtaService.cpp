#include <Arduino.h>
#include <WebServer.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <algorithm>
#include <cstring>

#include "OtaPage.h"
#include "OtaService.h"

#if __has_include("ota_password.h")
#include "ota_password.h"
#endif
#ifndef HUB_OTA_PASSWORD
#define HUB_OTA_PASSWORD ""
#endif

namespace {

constexpr size_t MAX_FIRMWARE_SIZE = 4U * 1024U * 1024U;
constexpr unsigned long MAX_UPLOAD_MS = 120000;
constexpr unsigned long REBOOT_DELAY_MS = 1500;
constexpr size_t PREFIX_SIZE = sizeof(esp_image_header_t) +
    sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t);
constexpr char password[] = HUB_OTA_PASSWORD;

// WebServer processes a raw body synchronously using its small fixed buffer.
// Keep only the image prefix here; never allocate the entire firmware in RAM.
uint8_t prefix[PREFIX_SIZE];
size_t prefixBytes = 0;
size_t expectedBytes = 0;
size_t receivedBytes = 0;
esp_ota_handle_t updateHandle = 0;
const esp_partition_t* updatePartition = nullptr;
bool handleOpen = false;
bool receiving = false;
bool bodyComplete = false;
bool responseSent = false;
bool restartPending = false;
unsigned long uploadStartedAt = 0;
unsigned long restartScheduledAt = 0;

bool configured() {
    constexpr size_t length = sizeof(password) - 1;
    if (length == 0 || length > 128) {
        return false;
    }
    for (size_t i = 0; i != length; ++i) {
        if (password[i] < 33 || password[i] > 126) {
            return false;
        }
    }
    return true;
}

bool authenticate(WebServer& server) {
    const String provided = server.header("X-OTA-Password");
    constexpr size_t length = sizeof(password) - 1;
    if (!configured() || provided.length() != length) {
        return false;
    }
    // Compare every byte of equal-length passwords without early exit.
    uint8_t diff = 0;
    for (size_t i = 0; i != length; ++i) {
        diff |= static_cast<uint8_t>(provided[i] ^ password[i]);
    }
    return diff == 0;
}

const esp_partition_t* targetPartition() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (!running || !target || running->address == target->address ||
        target->type != ESP_PARTITION_TYPE_APP ||
        target->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        target->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MAX ||
        target->size < PREFIX_SIZE) {
        return nullptr;
    }
    return target;
}

size_t uploadLimit(const esp_partition_t* partition) {
    return partition ? std::min(MAX_FIRMWARE_SIZE, static_cast<size_t>(partition->size)) : 0;
}

void abortUpdate() {
    if (handleOpen) {
        esp_ota_abort(updateHandle);
        handleOpen = false;
    }
    receiving = false;
    bodyComplete = false;
}

void sendError(WebServer& server, int status, const char* code) {
    String json = "{\"error\":{\"code\":\"";
    json += code;
    json += "\"}}";
    server.sendHeader("Cache-Control", "no-store");
    // Upload callbacks run inside request parsing, before WebServer resets its
    // response length. Set it explicitly so even the very first rejected POST
    // has a correctly framed JSON response.
    server.setContentLength(json.length());
    server.send(status, "application/json", json);
}

void rejectUpload(WebServer& server, int status, const char* code) {
    abortUpdate();
    if (!responseSent) {
        responseSent = true;
        server.sendHeader("Connection", "close");
        sendError(server, status, code);
    }
    // Stop receiving rejected/oversized bodies instead of draining megabytes.
    server.client().stop();
}

bool authorize(WebServer& server) {
    if (!configured()) {
        sendError(server, 503, "ota_not_configured");
        return false;
    }
    if (!authenticate(server)) {
        sendError(server, 401, "unauthorized");
        return false;
    }
    if (restartPending) {
        sendError(server, 503, "reboot_pending");
        return false;
    }
    return true;
}

void startUpload(WebServer& server) {
    abortUpdate();
    responseSent = false;
    prefixBytes = expectedBytes = receivedBytes = 0;
    updatePartition = nullptr;

    if (!configured() || !authenticate(server) || restartPending) {
        rejectUpload(server, !configured() || restartPending ? 503 : 401,
                     !configured() ? "ota_not_configured" :
                     (restartPending ? "reboot_pending" : "unauthorized"));
        return;
    }
    if (server.header("Content-Type") != "application/octet-stream") {
        rejectUpload(server, 415, "unsupported_media_type");
        return;
    }
    if (!server.header("Transfer-Encoding").isEmpty()) {
        rejectUpload(server, 400, "unsupported_transfer_encoding");
        return;
    }
    updatePartition = targetPartition();
    if (!updatePartition) {
        rejectUpload(server, 503, "ota_partition_unavailable");
        return;
    }

    const String length = server.header("Content-Length");
    if (length.isEmpty()) {
        rejectUpload(server, 411, "content_length_required");
        return;
    }
    // Strict decimal parsing, bounded before multiplication to avoid overflow.
    const size_t limit = uploadLimit(updatePartition);
    for (size_t i = 0; i < length.length(); ++i) {
        const char c = length[i];
        if (c < '0' || c > '9') {
            rejectUpload(server, 400, "invalid_content_length");
            return;
        }
        const size_t digit = static_cast<size_t>(c - '0');
        if (expectedBytes > (limit - digit) / 10) {
            rejectUpload(server, 413, "firmware_too_large");
            return;
        }
        expectedBytes = expectedBytes * 10 + digit;
    }
    if (expectedBytes < PREFIX_SIZE ||
        server.clientContentLength() != static_cast<int>(expectedBytes)) {
        rejectUpload(server, 400, "invalid_firmware_size");
        return;
    }
    receiving = true;
    uploadStartedAt = millis();
    server.client().setTimeout(5000);
}

bool beginImage(WebServer& server) {
    esp_image_header_t imageHeader;
    esp_app_desc_t appDescription;
    memcpy(&imageHeader, prefix, sizeof(imageHeader));
    memcpy(&appDescription,
           prefix + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t),
           sizeof(appDescription));
    if (imageHeader.magic != ESP_IMAGE_HEADER_MAGIC ||
        imageHeader.chip_id != ESP_CHIP_ID_ESP32S3 ||
        imageHeader.segment_count == 0 || imageHeader.segment_count > 16 ||
        imageHeader.hash_appended != 1 ||
        appDescription.magic_word != ESP_APP_DESC_MAGIC_WORD) {
        rejectUpload(server, 400, "invalid_firmware_image");
        return false;
    }
    // Authentication, size and image prefix checks precede any flash erase.
    if (esp_ota_begin(updatePartition, expectedBytes, &updateHandle) != ESP_OK) {
        rejectUpload(server, 500, "ota_begin_failed");
        return false;
    }
    handleOpen = true;
    if (esp_ota_write(updateHandle, prefix, PREFIX_SIZE) != ESP_OK) {
        rejectUpload(server, 500, "ota_write_failed");
        return false;
    }
    return true;
}

void writeChunk(WebServer& server, const uint8_t* data, size_t size) {
    if (!receiving || responseSent) {
        return;
    }
    if (millis() - uploadStartedAt > MAX_UPLOAD_MS) {
        rejectUpload(server, 408, "upload_timeout");
        return;
    }
    if (size > expectedBytes - receivedBytes) {
        rejectUpload(server, 400, "firmware_size_mismatch");
        return;
    }
    receivedBytes += size;
    if (prefixBytes < PREFIX_SIZE) {
        const size_t count = std::min(size, PREFIX_SIZE - prefixBytes);
        memcpy(prefix + prefixBytes, data, count);
        prefixBytes += count;
        data += count;
        size -= count;
        if (prefixBytes < PREFIX_SIZE || !beginImage(server)) {
            return;
        }
    }
    if (size && esp_ota_write(updateHandle, data, size) != ESP_OK) {
        rejectUpload(server, 500, "ota_write_failed");
    }
}

void receiveUpload(WebServer& server) {
    if (!server.hasRaw()) {
        // This endpoint only accepts a raw .bin body, never multipart files.
        rejectUpload(server, 415, "unsupported_media_type");
        return;
    }
    HTTPRaw& raw = server.raw();
    switch (raw.status) {
    case RAW_START:
        startUpload(server);
        break;
    case RAW_WRITE:
        writeChunk(server, raw.buf, raw.currentSize);
        break;
    case RAW_END:
        bodyComplete = receiving && !responseSent && handleOpen &&
            receivedBytes == expectedBytes;
        break;
    case RAW_ABORTED:
        abortUpdate();
        break;
    }
}

void finishUpload(WebServer& server) {
    if (responseSent) {
        return;
    }
    if (!authorize(server)) {
        abortUpdate();
        return;
    }
    if (server.header("Content-Type") != "application/octet-stream") {
        rejectUpload(server, 415, "unsupported_media_type");
        return;
    }
    if (!bodyComplete || !handleOpen) {
        rejectUpload(server, 400, "incomplete_upload");
        return;
    }
    // esp_ota_end verifies the ESP image/checksum/appended SHA-256, and frees
    // the OTA handle even on failure. Boot selection changes only afterwards.
    const esp_err_t result = esp_ota_end(updateHandle);
    handleOpen = false;
    receiving = bodyComplete = false;
    if (result != ESP_OK) {
        rejectUpload(server, 400, "firmware_verification_failed");
        return;
    }
    if (esp_ota_set_boot_partition(updatePartition) != ESP_OK) {
        rejectUpload(server, 500, "ota_boot_selection_failed");
        return;
    }
    restartPending = true;
    restartScheduledAt = millis();
    responseSent = true;
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Connection", "close");
    server.send(200, "application/json", "{\"status\":\"updated\",\"rebooting\":true}");
    Serial.println("[OTA] Image verified; restarting shortly");
}

}

namespace OtaService {

bool available() {
    return configured() && !restartPending && targetPartition() != nullptr;
}

bool rebootPending() {
    return restartPending;
}

void registerRoutes(WebServer& server) {
    WebServer* serverPtr = &server;
    server.on("/", HTTP_GET, [serverPtr]() {
        serverPtr->sendHeader("Location", "/update");
        serverPtr->sendHeader("Cache-Control", "no-store");
        serverPtr->send(302, "text/plain", "");
    });
    server.on("/update", HTTP_GET, [serverPtr]() {
        serverPtr->sendHeader("Cache-Control", "no-store");
        serverPtr->sendHeader("X-Content-Type-Options", "nosniff");
        serverPtr->sendHeader("X-Frame-Options", "DENY");
        serverPtr->sendHeader("Content-Security-Policy",
            "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
            "connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
        serverPtr->send_P(200, "text/html; charset=utf-8", OtaPage::html());
    });
    server.on("/api/v1/ota", HTTP_GET, [serverPtr]() {
        if (!authorize(*serverPtr)) {
            return;
        }
        const esp_partition_t* target = targetPartition();
        if (!target) {
            sendError(*serverPtr, 503, "ota_partition_unavailable");
            return;
        }
        String json = "{\"max_firmware_bytes\":";
        json += String(uploadLimit(target));
        json += "}";
        serverPtr->sendHeader("Cache-Control", "no-store");
        serverPtr->send(200, "application/json", json);
    });
    server.on("/update", HTTP_POST,
        [serverPtr]() { finishUpload(*serverPtr); },
        [serverPtr]() { receiveUpload(*serverPtr); });
    Serial.println(configured() ? "[OTA] Web upload: /update" :
        "[OTA] Disabled: configure HUB_OTA_PASSWORD in include/ota_password.h and rebuild");
}

void loop() {
    // Raw callbacks and final handler all run within one handleClient() call.
    // If parsing/disconnection skipped the final handler, release the OTA handle
    // before the next request. A partial image never becomes the boot target.
    abortUpdate();
    responseSent = false;
    if (restartPending && millis() - restartScheduledAt >= REBOOT_DELAY_MS) {
        ESP.restart();
    }
}

}
