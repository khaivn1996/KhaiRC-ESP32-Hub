
#pragma once

#include <Arduino.h>

namespace DeviceRegistry {

void begin();

String listJson();

String deviceJson(const String& id);

}
