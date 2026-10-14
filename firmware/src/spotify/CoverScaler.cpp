#include "spotify/CoverScaler.h"

#include <algorithm>

namespace CoverScaler {

void downscale(const uint16_t* source, int sourceWidth, int sourceHeight, uint16_t* target, int size)
{
    for (int y = 0; y < size; y++) {
        int top = y * sourceHeight / size;
        int bottom = std::max(top + 1, (y + 1) * sourceHeight / size);

        for (int x = 0; x < size; x++) {
            int left = x * sourceWidth / size;
            int right = std::max(left + 1, (x + 1) * sourceWidth / size);

            uint32_t red = 0;
            uint32_t green = 0;
            uint32_t blue = 0;
            uint32_t count = 0;
            for (int row = top; row < bottom; row++) {
                for (int column = left; column < right; column++) {
                    uint16_t pixel = source[row * sourceWidth + column];
                    red += pixel >> 11;
                    green += (pixel >> 5) & 0x3F;
                    blue += pixel & 0x1F;
                    count++;
                }
            }

            target[y * size + x] = static_cast<uint16_t>(((red / count) << 11) | ((green / count) << 5) | (blue / count));
        }
    }
}

}
