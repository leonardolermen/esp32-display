#pragma once

#include "setup/device/WifiLink.h"
#include "setup/storage/AdminSecret.h"
#include "setup/web/HttpServer.h"

class AdminRoutes {
public:
    AdminRoutes(HttpServer& http, AdminSecret& secret, const WifiLink& wifi);

    void begin();

private:
    HttpServer& http;
    AdminSecret& secret;
    const WifiLink& wifi;
};
