
#include "IrDiagnostics.h"

#include <Arduino.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_err.h"

namespace {

constexpr int IR_PIN = 4;

rmt_channel_handle_t txChannel = nullptr;
rmt_encoder_handle_t txEncoder = nullptr;

bool initialized = false;

void printError(const char* step, esp_err_t err) {
    Serial.printf(
        "[IR] %s: %s\n",
        step,
        esp_err_to_name(err)
    );
}

} // namespace

namespace IrDiagnostics {

void testDc() {
    Serial.println("[IR] DC TEST: PREPARING");

    pinMode(IR_PIN, OUTPUT);
    digitalWrite(IR_PIN, LOW);

    delay(3000);

    Serial.println("[IR] DC TEST: ON - 15 seconds");
    digitalWrite(IR_PIN, HIGH);

    delay(15000);

    digitalWrite(IR_PIN, LOW);
    Serial.println("[IR] DC TEST: OFF");
}

void begin() {
    initialized = false;

    rmt_tx_channel_config_t txConfig = {};

    txConfig.gpio_num = static_cast<gpio_num_t>(IR_PIN);
    txConfig.clk_src = RMT_CLK_SRC_DEFAULT;
    txConfig.resolution_hz = 1000000;
    txConfig.mem_block_symbols = 64;
    txConfig.trans_queue_depth = 4;

    esp_err_t err = rmt_new_tx_channel(
        &txConfig,
        &txChannel
    );

    if (err != ESP_OK) {
        printError("new_tx_channel", err);
        return;
    }

    rmt_carrier_config_t carrier = {};
    carrier.frequency_hz = 38000;
    // carrier.frequency_hz = 38028;
    carrier.duty_cycle = 0.33f;
    carrier.flags.polarity_active_low = false;
    carrier.flags.always_on = false;

    err = rmt_apply_carrier(txChannel, &carrier);

    if (err != ESP_OK) {
        printError("apply_carrier", err);
        return;
    }

    rmt_copy_encoder_config_t encoderConfig = {};

    err = rmt_new_copy_encoder(
        &encoderConfig,
        &txEncoder
    );

    if (err != ESP_OK) {
        printError("new_copy_encoder", err);
        return;
    }

    err = rmt_enable(txChannel);

    if (err != ESP_OK) {
        printError("enable", err);
        return;
    }

    initialized = true;

    Serial.println("[IR] init: ESP_OK");
    Serial.println("[IR] Carrier: 38000 Hz");
}

void testBurst() {
    if (!initialized) {
        Serial.println("[IR] ERROR: Not initialized");
        return;
    }

    Serial.println("[IR] Starting 3-second test...");

    rmt_symbol_word_t symbol = {};

    symbol.level0 = 1;
    symbol.duration0 = 10000;

    symbol.level1 = 0;
    symbol.duration1 = 10000;

    rmt_transmit_config_t transmitConfig = {};
    transmitConfig.loop_count = 0;
    transmitConfig.flags.eot_level = 0;

    for (int i = 0; i < 30; i++) {

        esp_err_t err = rmt_transmit(
            txChannel,
            txEncoder,
            &symbol,
            sizeof(symbol),
            &transmitConfig
        );

        if (err != ESP_OK) {
            printError("transmit", err);
            return;
        }

        err = rmt_tx_wait_all_done(
            txChannel,
            1000
        );

        if (err != ESP_OK) {
            printError("wait_all_done", err);
            return;
        }

        delay(80);
    }

    Serial.println("[IR] TEST COMPLETE");
}


void testFanHigh() {
    if (!initialized || !txChannel || !txEncoder) {
        Serial.println("[IR] Fan HIGH: not ready");
        return;
    }

    // Ma POWER_HIGH tu FanIrCodes.kt cua KhaiRC Android.
    // Don vi: microsecond. Chan le la MARK, chan chan la SPACE.
    static constexpr uint16_t pattern[] = {
        1274, 362, 1274, 358, 362, 1238,
        1270, 362, 1270, 362, 362, 1238,
        362, 1238, 1270, 362, 362, 1238,
        362, 1234, 366, 1234, 362, 7766,

        1246, 390, 1242, 390, 362, 1234,
        1242, 394, 1242, 390, 362, 1238,
        362, 1234, 1242, 394, 390, 1206,
        366, 1234, 390, 1210, 390, 7738,

        1238, 394, 1238, 398, 390, 1206,
        1214, 422, 1190, 442, 414, 1182,
        394, 1206, 1194, 438, 418, 1182,
        418, 1182, 418, 1182, 418, 1000
    };

    constexpr size_t count =
        sizeof(pattern) / sizeof(pattern[0]);

    static_assert(count % 2 == 0, "Invalid IR pattern");

    static rmt_symbol_word_t symbols[count / 2];

    for (size_t i = 0; i < count / 2; ++i) {
        symbols[i].level0 = 1;
        symbols[i].duration0 = pattern[i * 2];

        symbols[i].level1 = 0;
        symbols[i].duration1 = pattern[i * 2 + 1];
    }

    rmt_transmit_config_t config = {};
    config.loop_count = 0;
    config.flags.eot_level = 0;

    esp_err_t err = rmt_transmit(
        txChannel,
        txEncoder,
        symbols,
        sizeof(symbols),
        &config
    );

    if (err == ESP_OK) {
        err = rmt_tx_wait_all_done(txChannel, 1000);
    }

    Serial.printf(
        "[IR] Fan HIGH: %s\n",
        esp_err_to_name(err)
    );
}



void testFanSwing() {
    if (!initialized || !txChannel || !txEncoder) {
        Serial.println("[IR] Fan TAIZ SWING: not ready");
        return;
    }

    // KhaiRC Android: FanIrCodes.SWING @ 323c201.
    // Raw microseconds, starting with MARK; includes all 3 captured groups.
    // Send this entire pattern once, with no extra command repeats.
    // Keep the Hub's existing 38000 Hz carrier and 0.33 duty cycle.
    // Android requests 38028 Hz; the raw durations below are unchanged.
    static constexpr uint16_t pattern[] = {
        1198, 438, 1194, 438, 418, 1158,
        1190, 442, 1194, 462, 422, 1154,
        422, 1174, 426, 1174, 450, 1150,
        450, 1150, 446, 1154, 1190, 6946,

        1194, 466, 1190, 442, 422, 1174,
        1194, 466, 1194, 438, 446, 1154,
        422, 1174, 450, 1150, 422, 1178,
        446, 1154, 446, 1154, 1190, 6978,

        1194, 466, 1194, 438, 422, 1150,
        1194, 442, 1166, 466, 422, 1174,
        426, 1174, 398, 1178, 422, 1174,
        398, 1178, 422, 1154, 1190, 1000
    };

    constexpr size_t count = sizeof(pattern) / sizeof(pattern[0]);
    static_assert(count == 72, "TAIZ SWING must have 72 raw durations");

    static rmt_symbol_word_t symbols[count / 2] = {};

    for (size_t i = 0; i < count / 2; ++i) {
        symbols[i].level0 = 1;
        symbols[i].duration0 = pattern[i * 2];
        symbols[i].level1 = 0;
        symbols[i].duration1 = pattern[i * 2 + 1];
    }

    rmt_transmit_config_t config = {};
    config.loop_count = 0;
    config.flags.eot_level = 0;

    esp_err_t err = rmt_transmit(
        txChannel,
        txEncoder,
        symbols,
        sizeof(symbols),
        &config
    );

    if (err == ESP_OK) {
        err = rmt_tx_wait_all_done(txChannel, 1000);
    }

    Serial.printf("[IR] Fan TAIZ SWING: %s\n", esp_err_to_name(err));
}


void testToshibaFix() {
    if (!initialized || !txChannel || !txEncoder) {
        Serial.println("[IR] Toshiba FIX: not ready");
        return;
    }

    // KhaiRC Android - ToshibaAcEncoder.encodeFix()
    // 7 bytes, 56 bits, LSB first, repeatCount = 1.
    // Checksum XOR = 0x84.
    static constexpr uint8_t frame[] = {
        0x4F, 0xB0, 0x80, 0x7F,
        0x84, 0x00, 0x84
    };

    // 1 header + 56 data bits + 1 stop symbol.
    constexpr size_t SYMBOL_COUNT =
        1 + sizeof(frame) * 8 + 1;

    rmt_symbol_word_t symbols[SYMBOL_COUNT] = {};

    size_t index = 0;

    // Mot RMT symbol gom MARK + SPACE.
    auto appendSymbol = [&](uint16_t markUs, uint16_t spaceUs) {
        symbols[index].level0 = 1;
        symbols[index].duration0 = markUs;

        symbols[index].level1 = 0;
        symbols[index].duration1 = spaceUs;

        ++index;
    };

    // Toshiba header.
    appendSymbol(4350, 4300);

    // 56 bits, moi byte gui LSB first.
    for (uint8_t value : frame) {
        for (uint8_t bit = 0; bit < 8; ++bit) {
            bool one = (value & (1U << bit)) != 0;

            appendSymbol(
                550,
                one ? 1600 : 550
            );
        }
    }

    // Stop MARK + terminal SPACE.
    appendSymbol(550, 550);

    rmt_transmit_config_t config = {};
    config.loop_count = 0;
    config.flags.eot_level = 0;

    esp_err_t err = rmt_transmit(
        txChannel,
        txEncoder,
        symbols,
        sizeof(symbols),
        &config
    );

    if (err == ESP_OK) {
        err = rmt_tx_wait_all_done(
            txChannel,
            1000
        );
    }

    Serial.printf(
        "[IR] Toshiba FIX: %s\n",
        esp_err_to_name(err)
    );
}


} // namespace IrDiagnostics
