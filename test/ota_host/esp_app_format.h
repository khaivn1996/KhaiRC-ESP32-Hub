#pragma once
#include <cstdint>

// Layouts of the image prefix fields used by the production module.
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC = 0xe9;
constexpr uint16_t ESP_CHIP_ID_ESP32S3 = 9;
constexpr uint32_t ESP_APP_DESC_MAGIC_WORD = 0xabcd5432;
#pragma pack(push, 1)
struct esp_image_header_t {
    uint8_t magic, segment_count, spi_mode, spi_speed_size;
    uint32_t entry_addr;
    uint8_t wp_pin, spi_pin_drv[3];
    uint16_t chip_id;
    uint8_t min_chip_rev;
    uint16_t min_chip_rev_full, max_chip_rev_full;
    uint8_t reserved[4], hash_appended;
};
struct esp_image_segment_header_t { uint32_t load_addr, data_len; };
struct esp_app_desc_t { uint32_t magic_word; uint8_t rest[252]; };
#pragma pack(pop)
static_assert(sizeof(esp_image_header_t) == 24, "Image header layout");
