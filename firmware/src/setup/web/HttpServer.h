#pragma once

#include <ESPAsyncWebServer.h>

#include "setup/storage/AdminSecret.h"

class HttpServer {
public:
    explicit HttpServer(AdminSecret& secret);

    void begin();
    AsyncWebServer& raw();
    bool authorize(AsyncWebServerRequest* request);

private:
    void serveEmbedded(const char* path, const char* contentType, const uint8_t* start, const uint8_t* end);

    AdminSecret& secret;
    AsyncWebServer server{80};
};
