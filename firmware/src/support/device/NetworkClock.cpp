#include "support/device/NetworkClock.h"

#include <Arduino.h>

void NetworkClock::begin()
{
    // Brasília sem horário de verão. O NTP corre em segundo plano e LocalTime::now() só passa a responder depois.
    configTzTime("<-03>3", "pool.ntp.org", "time.google.com");
}
