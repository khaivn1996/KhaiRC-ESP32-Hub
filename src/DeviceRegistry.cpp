
#include "DeviceRegistry.h"
#include "IrDiagnostics.h"
#include "MiotLocal.h"
#include <cstring>

namespace {

struct Device {
    const char* id;
    const char* name;
    const char* type;
    const char* transport;
};

const Device devices[] = {
    {"daikin_ac", "Daikin AC", "ir_ac", "ir"},
    {"toshiba_ac", "Toshiba AC", "ir_ac", "ir"},
    {"fan_ir", "IR Fan", "ir_fan", "ir"},
    {"rangdong_plug_01", "Rang Dong Plug",
     "tuya_plug", "tuya_cloud"},
    {"milife_plug_10a", "Milife 10A", "smart_plug", "miio_lan"}
};

constexpr size_t DEVICE_COUNT =
    sizeof(devices) / sizeof(devices[0]);

void appendDeviceJson(String& json, const Device& d) {
    json += "{\"id\":\"";
    json += d.id;

    json += "\",\"name\":\"";
    json += d.name;

    json += "\",\"type\":\"";
    json += d.type;

    json += "\",\"transport\":\"";
    json += d.transport;

    const bool fan = strcmp(d.id, "fan_ir") == 0;
    const bool toshiba = strcmp(d.id, "toshiba_ac") == 0;
    const bool milife = strcmp(d.id, "milife_plug_10a") == 0;

    // Availability reports the local driver, not appliance power/reachability.
    json += "\",\"availability\":\"";
    if (milife) {
        // Configured means a token/IP is present, NOT that the plug is online.
        json += MiotLocal::configured() ? "configured" : "not_configured";
    } else {
        json += (fan || toshiba)
            ? (IrDiagnostics::isReady() ? "ready" : "not_ready")
            : "not_configured";
    }
    json += "\",\"supported_actions\":";
    if (fan) {
        json += "[\"power_on\",\"swing\",\"power_off\"]";
    } else if (toshiba) {
        json += "[\"fix\"]";
    } else {
        json += "[]";
    }
    json += "}";
}

}

namespace DeviceRegistry {

void begin() {
    Serial.printf(
        "[Devices] Registered: %u\n",
        (unsigned int)DEVICE_COUNT
    );
}

String listJson() {
    String json;
    json.reserve(768);

    json = "{\"schema_version\":1,\"devices\":[";

    for (size_t i = 0; i < DEVICE_COUNT; ++i) {
        if (i > 0) json += ",";
        appendDeviceJson(json, devices[i]);
    }

    json += "]}";
    return json;
}

String deviceJson(const String& id) {
    for (size_t i = 0; i < DEVICE_COUNT; ++i) {
        if (id == devices[i].id) {
            String json;
            json.reserve(256);

            json = "{\"schema_version\":1,\"device\":";
            appendDeviceJson(json, devices[i]);
            json += "}";

            return json;
        }
    }

    return String();
}

}
