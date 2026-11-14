"""Run WifiService.cpp with synthetic credentials and a simulated busy driver."""
import argparse
import os
import pathlib
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

ARDUINO = r'''
#pragma once
#include <cstdint>
extern unsigned long nowMs;
inline unsigned long millis() { return nowMs; }
struct SerialMock {
    template<class T> void print(T) {}
    template<class T> void println(T) {}
    template<class... T> void printf(const char*, T...) {}
};
extern SerialMock Serial;
'''

WIFI = r'''
#pragma once
#include <cassert>
#include <functional>
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_STA_START, ARDUINO_EVENT_WIFI_STA_CONNECTED,
    ARDUINO_EVENT_WIFI_STA_DISCONNECTED, ARDUINO_EVENT_WIFI_STA_GOT_IP };
struct WiFiEventInfo_t { struct { unsigned reason; } wifi_sta_disconnected; };
constexpr int WIFI_STA = 1, WL_CONNECTED = 3, WL_DISCONNECTED = 6;
struct WiFiMock {
    bool automatic = true, busy = false, stopOK = true, retryOK = true;
    int stops = 0, retries = 0, state = WL_DISCONNECTED;
    std::function<void(WiFiEvent_t, WiFiEventInfo_t)> callback;
    void onEvent(decltype(callback) fn) { callback = fn; }
    void setHostname(const char*) {}
    void mode(int) {}
    void setAutoReconnect(bool value) { automatic = value; }
    void begin(const char*, const char*) { busy = true; }
    int status() { return state; }
    const char* localIP() { return "test-ip"; }
    bool disconnect(bool off, bool erase) {
        assert(!off && !erase);
        ++stops;
        if (stopOK) busy = false;
        return stopOK;
    }
    bool reconnect() {
        assert(!busy); // Reproduce the original driver's "sta is connecting".
        ++retries;
        busy = retryOK;
        return retryOK;
    }
};
extern WiFiMock WiFi;
'''

TEST = r'''
#include "Arduino.h"
#include "WiFi.h"
#include "WifiService.h"
#include <iostream>
unsigned long nowMs = 0;
SerialMock Serial;
WiFiMock WiFi;
int main() {
    WifiService::begin();
    assert(!WiFi.automatic && WiFi.busy);
    nowMs = 29999; WifiService::loop(); assert(WiFi.retries == 0);
    nowMs = 30000; WifiService::loop(); assert(WiFi.retries == 1 && WiFi.stops == 1);
    // Repeated disconnect events must not trigger immediate app retries.
    for (int i = 0; i < 10; ++i)
        WiFi.callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, {{15}});
    nowMs = 59999; WifiService::loop(); assert(WiFi.retries == 1);
    nowMs = 60000; WifiService::loop(); assert(WiFi.retries == 2);
    WiFi.state = WL_CONNECTED;
    nowMs = 90000; WifiService::loop(); assert(WiFi.stops == 2);
    WiFi.state = WL_DISCONNECTED; WiFi.stopOK = false;
    nowMs = 90001; WifiService::loop(); assert(WiFi.retries == 2);
    WiFi.stopOK = true; WiFi.retryOK = false;
    nowMs = 120001; WifiService::loop(); assert(WiFi.retries == 3);
    WiFi.retryOK = true;
    nowMs = 150001; WifiService::loop(); assert(WiFi.retries == 4);
    nowMs = 0xfffffff0UL; WifiService::loop(); assert(WiFi.retries == 5);
    nowMs = 29983; WifiService::loop(); assert(WiFi.retries == 5);
    nowMs = 29984; WifiService::loop(); assert(WiFi.retries == 6);
    std::cout << "PASS Wi-Fi: busy handshake, retry interval, disconnect events, online, errors, timer rollover\n";
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error("C++ compiler not found")
    env = os.environ.copy()
    env["PATH"] = str(pathlib.Path(compiler).parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="khairc-wifi-test-") as directory:
        temp = pathlib.Path(directory)
        # Never put include/ on the test include path or copy real credentials.
        shutil.copyfile(ROOT / "src/WifiService.cpp", temp / "WifiService.cpp")
        shutil.copyfile(ROOT / "include/WifiService.h", temp / "WifiService.h")
        files = {"Arduino.h": ARDUINO, "WiFi.h": WIFI, "test.cpp": TEST,
                 "secrets.h": '#define WIFI_SSID "test"\n#define WIFI_PASSWORD "synthetic"\n',
                 "OtaSafety.h": '#pragma once\n#include <cstdint>\nnamespace OtaSafety { inline void wifiDisconnectReason(std::uint8_t) {} }\n'}
        for name, content in files.items():
            (temp / name).write_text(content, encoding="utf-8")
        executable = temp / "test-wifi.exe"
        subprocess.run([compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror", "-I", str(temp),
                        str(temp / "test.cpp"), str(temp / "WifiService.cpp"), "-o", str(executable)],
                       check=True, env=env)
        subprocess.run([str(executable)], check=True, env=env)


if __name__ == "__main__":
    main()
