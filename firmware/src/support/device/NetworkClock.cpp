#include "support/device/NetworkClock.h"

#include <Arduino.h>

namespace {

// Brasília sem horário de verão.
constexpr const char* TIME_ZONE = "<-03>3";

}

// O relógio do ESP32 sobrevive a um reset por software; sem o fuso aplicado logo no boot, a tela mostraria a hora
// em UTC (3 h adiantada) até a tarefa de rede subir.
void NetworkClock::applyTimeZone()
{
    setenv("TZ", TIME_ZONE, 1);
    tzset();
}

void NetworkClock::begin()
{
    // O NTP corre em segundo plano e LocalTime::now() só passa a responder depois.
    configTzTime(TIME_ZONE, "pool.ntp.org", "time.google.com");
}
