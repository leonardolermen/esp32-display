#include "board/device/Backlight.h"

#include <Arduino.h>

#include <algorithm>

#include "board/BoardPins.h"

namespace {

constexpr uint32_t PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint32_t PWM_MAX_DUTY = 255;

}

void Backlight::begin()
{
    ledcAttach(pins::LCD_BACKLIGHT, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
    setBrightness(100);
}

void Backlight::setBrightness(uint8_t percent)
{
    uint32_t clamped = std::min<uint32_t>(percent, 100);

    ledcWrite(pins::LCD_BACKLIGHT, clamped * PWM_MAX_DUTY / 100);
}
