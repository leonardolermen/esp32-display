#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "spotify/DominantColor.h"

constexpr int COVER_SIZE = 200;

// Pixels RGB565 já em 200×200. O vector passa de 4 KB, então o malloc do core o coloca na PSRAM.
struct CoverArt {
    std::string albumId;
    std::vector<uint16_t> pixels;
    uint32_t color = DominantColor::FALLBACK;
};
