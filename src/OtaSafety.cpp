#include "OtaSafety.h"
#include "OtaSafetyPolicy.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_app_desc.h>
#include <esp_system.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cstring>
#include <cstddef>

#include "ApiServer.h"
#include "HttpsService.h"
#include "IrDiagnostics.h"

// Arduino-ESP32 3.3.12 declares this as a weak *C* symbol. A normal C++
// function of the same spelling would NOT override it.
namespace {
    volatile bool hookInvoked = false;
}
extern "C" bool verifyRollbackLater() {
    hookInvoked = true;
    return true; // Do NOT mark PENDING_VERIFY as VALID during initArduino().
}

namespace {
constexpr std::uint32_t RECORD_MAGIC = 0x4B52434FU; // "KRCO"
constexpr std::uint16_t RECORD_VERSION = 1;
constexpr char NVS_NAMESPACE[] = "ota_safe";

// No secrets, no raw password/IP credentials. Prior boot survives a USB app
// flash as long as NVS is not erased.
struct Journal {
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t stage;
    std::uint32_t seq;
    std::uint32_t partition;
    std::uint32_t resetReason;
    std::uint32_t otaState;
    std::uint32_t disconnectReason;
    char build[17];
    std::uint32_t checksum;
};
static_assert(sizeof(Journal) <= 64, "Keep NVS record small");

std::uint32_t checksum(const Journal& record) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(&record);
    std::uint32_t hash = 2166136261U;
    for (std::size_t index = 0; index < offsetof(Journal, checksum); ++index) {
        hash = (hash ^ data[index]) * 16777619U;
    }
    return hash;
}
bool validJournal(const Journal& record) {
    return record.magic == RECORD_MAGIC && record.version == RECORD_VERSION &&
           record.checksum == checksum(record) && record.build[16] == '\0';
}

Preferences prefs;
bool prefsOpen = false;
Journal current{};
std::uint16_t highestStage = 0;
std::atomic<unsigned> disconnectCode{0};
unsigned lastSavedDisconnect = 0;
OtaSafetyPolicy::HealthGate healthGate;
bool pending = false;
bool validationAttempted = false;
SemaphoreHandle_t verifyMutex = nullptr;
bool confirmed = false;
bool otaStateKnown = false;
esp_ota_img_states_t otaState = ESP_OTA_IMG_UNDEFINED;
std::uint32_t bootAtMs = 0;
char buildIdBuffer[17] = {};
char slotBuffer[20] = {};

void save() {
    if (!prefsOpen) return;
    current.checksum = checksum(current);
    if (prefs.putBytes("now", &current, sizeof(current)) != sizeof(current)) {
        Serial.println("[OTA SAFE] NVS write failed");
    }
}
void setStage(std::uint16_t stage) {
    if (stage <= highestStage) return;
    highestStage = stage;
    current.stage = stage;
    save();
}
void printJournal(const char* label, const Journal& record) {
    Serial.printf("[OTA SAFE] %s seq=%lu slot=0x%lx stage=%u reset=%lu ota=%lu reason=%lu build=%s\n",
        label, static_cast<unsigned long>(record.seq),
        static_cast<unsigned long>(record.partition), record.stage,
        static_cast<unsigned long>(record.resetReason),
        static_cast<unsigned long>(record.otaState),
        static_cast<unsigned long>(record.disconnectReason), record.build);
}

// Runs in an independent FreeRTOS task so a blocked Arduino loop cannot
// silently allow a PENDING_VERIFY image to remain unverified forever.
void verifyDeadlineTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(OtaSafetyPolicy::DEADLINE_MS));
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (verifyMutex && xSemaphoreTake(verifyMutex, portMAX_DELAY) == pdTRUE) {
        if (running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
            state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.println("[OTA SAFE] deadline exceeded, attempting rollback");
            const esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
            // On success this reboots. On failure do NOT erase or reboot
            // blindly; the previous OTA partition may not be usable.
            Serial.printf("[OTA SAFE] rollback FAILED: %s; USB may be required\n",
                          esp_err_to_name(err));
        }
        xSemaphoreGive(verifyMutex);
    }
    vTaskDelete(nullptr);
}
}

namespace OtaSafety {
void begin() {
    bootAtMs = millis();
    const esp_partition_t* part = esp_ota_get_running_partition();
    const esp_err_t result = part ? esp_ota_get_state_partition(part, &otaState) : ESP_FAIL;
    otaStateKnown = result == ESP_OK;
    pending = otaStateKnown && otaState == ESP_OTA_IMG_PENDING_VERIFY;
    confirmed = !pending;
    esp_app_get_elf_sha256(buildIdBuffer, sizeof(buildIdBuffer));
    buildIdBuffer[16] = '\0';
    if (part) {
        snprintf(slotBuffer, sizeof(slotBuffer), "%s", part->label);
    } else {
        snprintf(slotBuffer, sizeof(slotBuffer), "unknown");
    }
    prefsOpen = prefs.begin(NVS_NAMESPACE, false);
    Journal previous{};
    if (prefsOpen && prefs.getBytesLength("now") == sizeof(previous) &&
        prefs.getBytes("now", &previous, sizeof(previous)) == sizeof(previous) &&
        validJournal(previous)) {
        printJournal("previous", previous);
        // Keep last prior record after the current boot replaces "now".
        previous.checksum = checksum(previous);
        (void)prefs.putBytes("prev", &previous, sizeof(previous));
        current.seq = previous.seq + 1;
    } else {
        current.seq = 1;
    }
    current.magic = RECORD_MAGIC;
    current.version = RECORD_VERSION;
    current.stage = 1;  // BOOT
    current.partition = part ? part->address : 0;
    current.resetReason = static_cast<std::uint32_t>(esp_reset_reason());
    current.otaState = otaStateKnown ? static_cast<std::uint32_t>(otaState) : 0xffffffffU;
    current.disconnectReason = 0;
    std::strncpy(current.build, buildIdBuffer, sizeof(current.build) - 1);
    current.build[16] = '\0';
    highestStage = 1;
    save();
    printJournal("current", current);
#if defined(CONFIG_APP_ROLLBACK_ENABLE)
    Serial.printf("[OTA SAFE] rollback compile=enabled hook=%u pending=%u state=%s\n",
        hookInvoked ? 1U : 0U, pending ? 1U : 0U, stateName());
#else
    Serial.println("[OTA SAFE] rollback compile=DISABLED; future OTA blocked");
#endif
    if (pending) {
        verifyMutex = xSemaphoreCreateMutex();
        if (!verifyMutex) {
            Serial.println("[OTA SAFE] cannot create guard mutex; rebooting pending image");
            ESP.restart(); // Bootloader may rollback on next boot.
        }
        const BaseType_t created = xTaskCreate(verifyDeadlineTask, "ota_guard", 4096,
                                               nullptr, 2, nullptr);
        if (created != pdPASS) {
            Serial.println("[OTA SAFE] watchdog task failed; rebooting pending image");
            // Leave pending state unconfirmed: bootloader will attempt rollback.
            ESP.restart();
        }
    }
}

void wifiBegin() { setStage(2); }

void wifiDisconnectReason(std::uint8_t reason) {
    disconnectCode.store(reason, std::memory_order_relaxed);
}

bool pendingVerification() { return pending; }
const char* buildId() { return buildIdBuffer; }
const char* slotName() { return slotBuffer; }
const char* stateName() {
    if (!otaStateKnown) return "unknown";
    if (pending) return "pending_verify";
    switch (otaState) {
        case ESP_OTA_IMG_NEW: return "new";
        case ESP_OTA_IMG_PENDING_VERIFY: return "pending_verify";
        case ESP_OTA_IMG_VALID: return "valid";
        case ESP_OTA_IMG_INVALID: return "invalid";
        case ESP_OTA_IMG_ABORTED: return "aborted";
        case ESP_OTA_IMG_UNDEFINED: return "undefined";
        default: return "unknown";
    }
}

bool canAcceptUpdate() {
#if defined(CONFIG_APP_ROLLBACK_ENABLE)
    // Fail closed when framework never called our hook, pending confirmation
    // is unresolved, or OTA partition metadata cannot be read.
    return hookInvoked && !pending && confirmed && otaStateKnown &&
           (otaState == ESP_OTA_IMG_VALID || otaState == ESP_OTA_IMG_UNDEFINED) &&
           esp_ota_get_app_partition_count() >= 2;
#else
    return false;
#endif
}

void loop() {
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0) setStage(3); // GOT_IP
    if (ApiServer::ready()) setStage(4); // HTTP/OTA started
    if (HttpsService::ready()) setStage(5); // TLS server started
    const unsigned reason = disconnectCode.load(std::memory_order_relaxed);
    if (reason != lastSavedDisconnect) {
        lastSavedDisconnect = reason;
        current.disconnectReason = reason;
        save();
    }
    if (!pending) return;
    const std::uint32_t now = millis();
    const bool isHealthy = healthGate.ready(now,
        WiFi.status() == WL_CONNECTED,
        WiFi.localIP()[0] != 0,
        ApiServer::ready(), HttpsService::ready(), IrDiagnostics::isReady());
    if (!isHealthy || OtaSafetyPolicy::expired(bootAtMs, now) || validationAttempted) return;
    validationAttempted = true;
    if (!verifyMutex || xSemaphoreTake(verifyMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("[OTA SAFE] verification mutex unavailable, keeping PENDING");
        return; // Deadline task will attempt rollback.
    }
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t actual = ESP_OTA_IMG_UNDEFINED;
    if (!running || esp_ota_get_state_partition(running, &actual) != ESP_OK ||
        actual != ESP_OTA_IMG_PENDING_VERIFY) {
        xSemaphoreGive(verifyMutex);
        Serial.println("[OTA SAFE] unexpected OTA state; NOT marking valid");
        return;
    }
    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        pending = false;
        confirmed = true;
        otaState = ESP_OTA_IMG_VALID;
        setStage(6);
        Serial.printf("[OTA SAFE] confirmed VALID after %lu ms, build=%s\n",
                      static_cast<unsigned long>(now - bootAtMs), buildIdBuffer);
    } else {
        Serial.printf("[OTA SAFE] mark VALID failed: %s\n", esp_err_to_name(err));
    }
    xSemaphoreGive(verifyMutex);
}
}
