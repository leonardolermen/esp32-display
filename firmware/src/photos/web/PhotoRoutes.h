#pragma once

#include "photos/storage/PhotoStore.h"
#include "setup/web/HttpServer.h"

class PhotoRoutes {
public:
    PhotoRoutes(HttpServer& http, PhotoStore& store);

    void begin();

private:
    // Estado de um envio, preso ao request (o servidor libera com free() ao fim), nunca compartilhado entre dois.
    struct Upload {
        bool authorized;
        bool owner;
        size_t received;
        const char* refusal;
    };

    void list(AsyncWebServerRequest* request);
    void serve(AsyncWebServerRequest* request);
    void remove(AsyncWebServerRequest* request);
    void receive(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index);
    void finish(AsyncWebServerRequest* request);
    Upload* startUpload(AsyncWebServerRequest* request);
    void releaseUpload(AsyncWebServerRequest* request, Upload* upload);

    HttpServer& http;
    PhotoStore& store;
    AsyncWebServerRequest* uploading = nullptr;
};
