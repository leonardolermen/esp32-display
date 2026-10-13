#include "setup/device/WifiLink.h"

#include <ESPmDNS.h>
#include <WiFi.h>

namespace {

constexpr const char* HOSTNAME = "claudinho";
constexpr const char* HOTSPOT_NAME = "Claudinho";

}

bool WifiLink::hasSavedNetwork()
{
    // A rede salva vem da config da estação, que só existe com o driver iniciado; begin() repete o mode sem custo.
    WiFi.mode(WIFI_STA);
    return manager.getWiFiIsSaved();
}

void WifiLink::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);

    // Sem bloquear: a UI continua viva mostrando o QR enquanto o portal espera alguém escolher a rede.
    manager.setConfigPortalBlocking(false);
    manager.setHostname(HOSTNAME);
    manager.autoConnect(HOTSPOT_NAME);
}

void WifiLink::process()
{
    manager.process();

    if (connected() && !mdnsStarted) {
        mdnsStarted = MDNS.begin(HOSTNAME);
        MDNS.addService("http", "tcp", 80);
    }
}

bool WifiLink::connected() const
{
    return WiFi.status() == WL_CONNECTED;
}

std::string WifiLink::ip() const
{
    return WiFi.localIP().toString().c_str();
}
