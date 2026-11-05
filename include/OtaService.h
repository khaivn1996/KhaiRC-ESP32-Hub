#pragma once

class WebServer;

namespace OtaService {
    void registerRoutes(WebServer& server);
    // Call between WebServer::handleClient() calls, never from an upload callback.
    void loop();
    bool available();
    bool rebootPending();
}
