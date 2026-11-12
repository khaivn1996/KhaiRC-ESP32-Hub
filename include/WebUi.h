#pragma once
#include <esp_http_server.h>

namespace WebUi {
// Register only on the TLS server, never on HTTP/80 or OTA/8081.
bool registerRoutes(httpd_handle_t server);
void resetSessions();
}
