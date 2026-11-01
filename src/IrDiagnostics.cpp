
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

} // namespace IrDiagnostics
