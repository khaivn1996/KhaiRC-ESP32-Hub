#pragma once
#include "esp_app_format.h"
#include <cstddef>
#include <vector>

using esp_err_t = int;
using esp_ota_handle_t = uint32_t;
constexpr int ESP_OK = 0;
constexpr int ESP_PARTITION_TYPE_APP = 0;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_0 = 16;
constexpr int ESP_PARTITION_SUBTYPE_APP_OTA_MAX = 32;
struct esp_partition_t { int type, subtype; uint32_t address, size; };
struct FakeOta {
    esp_partition_t running{0, 16, 0x10000, 0x640000};
    esp_partition_t target{0, 17, 0x650000, 0x640000};
    bool targetMissing = false;
    int begins = 0, writes = 0, ends = 0, boots = 0, aborts = 0;
    int beginResult = 0, writeResult = 0, endResult = 0, bootResult = 0;
    std::vector<uint8_t> bytes;
};
extern FakeOta fakeOta;
inline const esp_partition_t* esp_ota_get_running_partition() { return &fakeOta.running; }
inline const esp_partition_t* esp_ota_get_next_update_partition(const void*) {
    return fakeOta.targetMissing ? nullptr : &fakeOta.target;
}
inline int esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t* handle) {
    ++fakeOta.begins; *handle = 1; return fakeOta.beginResult;
}
inline int esp_ota_write(esp_ota_handle_t, const void* data, size_t size) {
    ++fakeOta.writes;
    const auto* bytes = static_cast<const uint8_t*>(data);
    fakeOta.bytes.insert(fakeOta.bytes.end(), bytes, bytes + size);
    return fakeOta.writeResult;
}
inline int esp_ota_end(esp_ota_handle_t) { ++fakeOta.ends; return fakeOta.endResult; }
inline int esp_ota_set_boot_partition(const esp_partition_t*) { ++fakeOta.boots; return fakeOta.bootResult; }
inline int esp_ota_abort(esp_ota_handle_t) { ++fakeOta.aborts; return 0; }
