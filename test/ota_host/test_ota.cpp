#include "Arduino.h"
#include "WebServer.h"
#include "esp_ota_ops.h"
#include "OtaService.h"
#include "OtaSafety.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

unsigned long fakeMillis = 0;
FakeSerial Serial;
FakeESP ESP;
FakeOta fakeOta;
bool OtaSafety::safeEnabled = true;

std::vector<uint8_t> firmware(size_t size = 4096) {
    std::vector<uint8_t> bytes(size, 0xa5);
    esp_image_header_t image{};
    image.magic = ESP_IMAGE_HEADER_MAGIC;
    image.segment_count = 3;
    image.chip_id = ESP_CHIP_ID_ESP32S3;
    image.hash_appended = 1;
    memcpy(bytes.data(), &image, sizeof(image));
    esp_app_desc_t app{};
    app.magic_word = ESP_APP_DESC_MAGIC_WORD;
    memcpy(bytes.data() + sizeof(image) + sizeof(esp_image_segment_header_t), &app, sizeof(app));
    return bytes;
}

void stream(WebServer& server, HTTPRawStatus status) {
    server.body.status = status;
    server.routes.at({"/update", HTTP_POST}).stream();
}
void finish(WebServer& server) { server.routes.at({"/update", HTTP_POST}).finish(); }
void start(WebServer& server, size_t size) {
    server.connection.stopped = false;
    server.requestHeaders["Content-Length"] = String(size);
    stream(server, RAW_START);
}
void write(WebServer& server, const std::vector<uint8_t>& data, size_t chunkSize = 1436) {
    for (size_t offset = 0; offset < data.size() && !server.connection.stopped;) {
        const size_t count = std::min(chunkSize, data.size() - offset);
        memcpy(server.body.buf, data.data() + offset, count);
        server.body.currentSize = count;
        stream(server, RAW_WRITE);
        offset += count;
    }
}
void error(WebServer& server, int status, const char* code) {
    assert(server.status == status);
    assert(server.response.find(code) != std::string::npos);
    assert(fakeOta.boots == 0);
    assert(!OtaService::rebootPending());
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string name = argv[1];
    WebServer server;
    server.requestHeaders["X-OTA-Password"] = "test-only-ota-password";
    server.requestHeaders["Content-Type"] = "application/octet-stream";
    OtaService::registerRoutes(server);
    auto data = firmware();

    if (name == "disabled") {
        assert(!OtaService::available());
        start(server, data.size());
        error(server, 503, "ota_not_configured");
        assert(fakeOta.begins == 0);
    } else if (name == "wrong_password" || name == "missing_password") {
        server.requestHeaders["X-OTA-Password"] = name == "wrong_password" ? "test-only-ota-passwore" : "";
        start(server, data.size());
        write(server, data);
        error(server, 401, "unauthorized");
        assert(fakeOta.begins == 0 && fakeOta.writes == 0);
    } else if (name == "status" || name == "page") {
        server.routes.at({name == "status" ? "/api/v1/ota" : "/update", HTTP_GET}).finish();
        assert(server.status == 200);
        assert(server.response.find(name == "status" ? "4194304" : "<html lang=\"vi\">") != std::string::npos);
        assert(server.response.find("test-only-ota-password") == std::string::npos);
    } else if (name == "oversize" || name == "overflow" || name == "invalid_length" || name == "missing_length" || name == "empty") {
        server.requestHeaders["Content-Length"] = name == "oversize" ? "4194305" :
            name == "overflow" ? "999999999999999999999" :
            name == "invalid_length" ? "4096junk" : name == "empty" ? "0" : "";
        stream(server, RAW_START);
        const bool large = name == "oversize" || name == "overflow";
        error(server, large ? 413 : name == "missing_length" ? 411 : 400,
            large ? "firmware_too_large" : name == "missing_length" ? "content_length_required" :
            name == "empty" ? "invalid_firmware_size" : "invalid_content_length");
        assert(fakeOta.begins == 0);
    } else if (name == "no_partition" || name == "active_partition" || name == "small_partition") {
        if (name == "no_partition") fakeOta.targetMissing = true;
        if (name == "active_partition") fakeOta.target.address = fakeOta.running.address;
        if (name == "small_partition") fakeOta.target.size = 2048;
        start(server, data.size());
        error(server, name == "small_partition" ? 413 : 503,
            name == "small_partition" ? "firmware_too_large" : "ota_partition_unavailable");
        assert(fakeOta.begins == 0);
    } else if (name == "safety_disabled") {
        OtaSafety::safeEnabled = false;
        assert(!OtaService::available());
        start(server, data.size());
        error(server, 503, "ota_safety_not_ready");
        assert(fakeOta.begins == 0);
    } else if (name == "wrong_type" || name == "chunked" || name == "multipart") {
        if (name == "wrong_type") server.requestHeaders["Content-Type"] = "application/json";
        if (name == "chunked") server.requestHeaders["Transfer-Encoding"] = "chunked";
        if (name == "multipart") server.rawPresent = false;
        start(server, data.size());
        error(server, name == "chunked" ? 400 : 415,
            name == "chunked" ? "unsupported_transfer_encoding" : "unsupported_media_type");
        assert(fakeOta.begins == 0);
    } else {
        if (name == "wrong_magic") data[0] = 0;
        if (name == "wrong_chip") data[12] = 0;
        if (name == "no_hash") data[23] = 0;
        if (name == "bootloader") data[32] = 0;
        if (name == "begin_failure") fakeOta.beginResult = -1;
        if (name == "write_failure") fakeOta.writeResult = -1;
        if (name == "invalid_checksum") fakeOta.endResult = -1;
        if (name == "boot_failure") fakeOta.bootResult = -1;
        if (name == "max_size") data = firmware(4194304);
        start(server, data.size());
        if (name == "timeout") fakeMillis = 120001;
        if (name == "truncated" || name == "disconnect" || name == "parser_failure") data.resize(1024);
        if (name == "size_mismatch") data.resize(data.size() + 1);
        write(server, data, name == "split_header" ? 1 : 1436);

        if (name == "wrong_magic" || name == "wrong_chip" || name == "no_hash" || name == "bootloader") {
            error(server, 400, "invalid_firmware_image");
            assert(fakeOta.begins == 0);
        } else if (name == "begin_failure" || name == "write_failure") {
            error(server, 500, name == "begin_failure" ? "ota_begin_failed" : "ota_write_failed");
            assert(fakeOta.aborts == (name == "write_failure" ? 1 : 0));
        } else if (name == "timeout" || name == "size_mismatch") {
            error(server, name == "timeout" ? 408 : 400, name == "timeout" ? "upload_timeout" : "firmware_size_mismatch");
        } else if (name == "disconnect" || name == "parser_failure") {
            if (name == "disconnect") stream(server, RAW_ABORTED);
            OtaService::loop();
            assert(fakeOta.aborts == 1 && fakeOta.boots == 0 && fakeOta.ends == 0);
            // A later valid request must not inherit the abandoned transfer.
            fakeOta.bytes.clear();
            data = firmware();
            start(server, data.size());
            write(server, data);
            stream(server, RAW_END);
            finish(server);
            assert(server.status == 200 && fakeOta.bytes == data);
        } else {
            stream(server, RAW_END);
            assert(fakeOta.boots == 0); // End-of-body alone cannot activate firmware.
            finish(server);
            if (name == "truncated") {
                error(server, 400, "incomplete_upload");
                assert(fakeOta.ends == 0 && fakeOta.aborts == 1);
            } else if (name == "invalid_checksum") {
                error(server, 400, "firmware_verification_failed");
                assert(fakeOta.ends == 1 && fakeOta.aborts == 0);
            } else if (name == "boot_failure") {
                assert(server.status == 500 && !OtaService::rebootPending());
                assert(fakeOta.ends == 1 && fakeOta.boots == 1);
            } else {
                assert(name == "success" || name == "split_header" || name == "max_size");
                assert(server.status == 200 && server.responses == 1);
                assert(fakeOta.begins == 1 && fakeOta.ends == 1 && fakeOta.boots == 1);
                assert(fakeOta.bytes == data && fakeOta.aborts == 0);
                assert(OtaService::rebootPending());
                fakeMillis = 1499;
                OtaService::loop();
                assert(ESP.restarts == 0);
                fakeMillis = 1500;
                OtaService::loop();
                assert(ESP.restarts == 1);
            }
        }
    }
    std::cout << "PASS " << name << '\n';
}
