#pragma once
#include "Arduino.h"
#include <functional>
#include <map>

enum HTTPMethod { HTTP_GET, HTTP_POST };
enum HTTPRawStatus { RAW_START, RAW_WRITE, RAW_END, RAW_ABORTED };
struct HTTPRaw {
    HTTPRawStatus status = RAW_START;
    size_t currentSize = 0;
    uint8_t buf[1436] = {};
};
struct FakeClient {
    bool stopped = false;
    void stop() { stopped = true; }
    void setTimeout(unsigned long) {}
};
class WebServer {
public:
    struct Route { std::function<void()> finish, stream; };
    std::map<std::pair<std::string, HTTPMethod>, Route> routes;
    std::map<std::string, String> requestHeaders, responseHeaders;
    FakeClient connection;
    HTTPRaw body;
    bool rawPresent = true;
    int status = 0;
    int responses = 0;
    String response;
    String header(const char* key) { return requestHeaders[key]; }
    int clientContentLength() { return std::atoi(header("Content-Length").c_str()); }
    bool hasRaw() { return rawPresent; }
    HTTPRaw& raw() { return body; }
    FakeClient& client() { return connection; }
    void setContentLength(size_t) {}
    void sendHeader(const char* key, const char* value) { responseHeaders[key] = value; }
    void send(int code, const char*, const String& text) { status = code; response = text; ++responses; }
    void send_P(int code, const char* type, const char* text) { send(code, type, text); }
    void on(const char* path, HTTPMethod method, std::function<void()> finish,
            std::function<void()> stream = {}) {
        routes[{path, method}] = {finish, stream};
    }
};
