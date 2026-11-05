#pragma once
#include <cstdint>
#include <string>

#define PROGMEM
class String : public std::string {
public:
    String() = default;
    using std::string::string;
    String(const std::string& value) : std::string(value) {}
    explicit String(size_t value) : std::string(std::to_string(value)) {}
    bool isEmpty() const { return empty(); }
};

extern unsigned long fakeMillis;
inline unsigned long millis() { return fakeMillis; }
struct FakeSerial { void println(const char*) {} };
struct FakeESP { int restarts = 0; void restart() { ++restarts; } };
extern FakeSerial Serial;
extern FakeESP ESP;
