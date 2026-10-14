#pragma once

#include <cstdint>

// Reduz uma vez, ao baixar a capa: em full refresh, escalar no desenho custaria em todo quadro.
namespace CoverScaler {

void downscale(const uint16_t* source, int sourceWidth, int sourceHeight, uint16_t* target, int size);

}
