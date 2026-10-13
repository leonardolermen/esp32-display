#pragma once

#include <optional>
#include <string>

#include "photos/storage/PhotoStore.h"
#include "setup/web/HttpServer.h"

class PhotoRoutes {
public:
    PhotoRoutes(HttpServer& http, PhotoStore& store);

    void begin();

private:
    void list(AsyncWebServerRequest* request);
    void serve(AsyncWebServerRequest* request);
    void remove(AsyncWebServerRequest* request);
    void receive(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index);
    void finish(AsyncWebServerRequest* request);

    HttpServer& http;
    PhotoStore& store;
    std::optional<std::string> refusal;
};
