#pragma once

#include "setup/device/WifiLink.h"
#include "setup/web/HttpServer.h"
#include "spotify/device/SpotifyAuth.h"
#include "spotify/storage/SpotifyVault.h"

class SpotifyRoutes {
public:
    SpotifyRoutes(HttpServer& http, SpotifyAuth& auth, SpotifyVault& vault, const WifiLink& wifi);

    void begin();

private:
    void go(AsyncWebServerRequest* request);
    void callback(AsyncWebServerRequest* request);
    void status(AsyncWebServerRequest* request);
    void connect(AsyncWebServerRequest* request, const uint8_t* data, size_t length);
    void paste(AsyncWebServerRequest* request, const uint8_t* data, size_t length);
    void disconnect(AsyncWebServerRequest* request);

    HttpServer& http;
    SpotifyAuth& auth;
    SpotifyVault& vault;
    const WifiLink& wifi;
};
