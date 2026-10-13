#pragma once

#include <cstdint>

class Backlight {
public:
    void begin();
    void setBrightness(uint8_t percent);
};
