#pragma once

#include <cstddef>
#include <cstdint>

namespace DominantColor {

constexpr uint32_t FALLBACK = 0x34322F;
// Com o texto #F5F1EA (luminância 0,88), 0,15 dá contraste 4,65 — acima do 4,5 da WCAG para texto comum.
constexpr double MAX_LUMINANCE = 0.15;

uint32_t of(const uint16_t* pixels, size_t count);
double luminance(uint32_t rgb);

}
