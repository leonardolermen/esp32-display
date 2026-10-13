#pragma once

#include <cstdint>

// Pinos do exemplo F1ATB para a JC3248W535C. O reset do touch (12) coincide com o SCLK do microSD nas fontes;
// enquanto o microSD estiver fora do projeto, isso não importa — confirmar no esquemático antes de usar o SD.
namespace pins {

constexpr int8_t LCD_CS = 45;
constexpr int8_t LCD_SCK = 47;
constexpr int8_t LCD_D0 = 21;
constexpr int8_t LCD_D1 = 48;
constexpr int8_t LCD_D2 = 40;
constexpr int8_t LCD_D3 = 39;
constexpr int8_t LCD_BACKLIGHT = 1;

constexpr int8_t TOUCH_SDA = 4;
constexpr int8_t TOUCH_SCL = 8;
constexpr int8_t TOUCH_RESET = 12;
constexpr uint8_t TOUCH_ADDRESS = 0x3B;

}
