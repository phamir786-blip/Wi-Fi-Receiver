/**
 * @file web_server.h
 * @brief Embedded asynchronous Web Server and JSON REST API for WiFi-HiFi.
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include "config.h"

class WebServerManager {
public:
    WebServerManager();
    ~WebServerManager();

    bool begin(uint16_t port = NETWORK_HTTP_PORT);
    void stop();

private:
    AsyncWebServer server_;
    bool running_;

    void registerRoutes();
    void registerApiRoutes();
};

extern WebServerManager g_web_server;
