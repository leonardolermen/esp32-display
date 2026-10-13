#pragma once

#include <WiFiManager.h>

#include <string>

class WifiLink {
public:
    void begin();
    void process();
    bool connected() const;
    std::string ip() const;

private:
    WiFiManager manager;
    bool mdnsStarted = false;
};
