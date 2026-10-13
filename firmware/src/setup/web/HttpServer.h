#pragma once

// O WebServer do core tem que entrar antes do ESPAsyncWebServer: sem ele, o Async declara os próprios HTTP_GET,
// HTTP_POST… e eles colidem com os do core quando o WiFiManager (que usa o WebServer) aparece depois.
#include <WebServer.h>

#include <ESPAsyncWebServer.h>

#include "setup/storage/AdminSecret.h"

class HttpServer {
public:
    explicit HttpServer(AdminSecret& secret);

    void begin();
    AsyncWebServer& raw();
    bool isAuthorized(AsyncWebServerRequest* request) const;
    bool authorize(AsyncWebServerRequest* request);

private:
    void serveEmbedded(const char* path, const char* contentType, const uint8_t* start, const uint8_t* end);

    AdminSecret& secret;
    AsyncWebServer server{80};
};
