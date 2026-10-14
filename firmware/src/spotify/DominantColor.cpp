#include "spotify/DominantColor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int BINS_PER_CHANNEL = 16;
constexpr size_t SAMPLE_STEP = 4;
constexpr int DARKEST_USEFUL = 24;
constexpr int BRIGHTEST_USEFUL = 232;
constexpr double DARKEN_FACTOR = 0.9;

struct Rgb {
    int red;
    int green;
    int blue;
};

struct Bin {
    uint32_t count = 0;
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
};

Rgb expand(uint16_t pixel)
{
    int red5 = pixel >> 11;
    int green6 = (pixel >> 5) & 0x3F;
    int blue5 = pixel & 0x1F;
    return Rgb{(red5 << 3) | (red5 >> 2), (green6 << 2) | (green6 >> 4), (blue5 << 3) | (blue5 >> 2)};
}

uint32_t pack(Rgb color)
{
    return (static_cast<uint32_t>(color.red) << 16) | (static_cast<uint32_t>(color.green) << 8)
        | static_cast<uint32_t>(color.blue);
}

Rgb unpack(uint32_t rgb)
{
    return Rgb{static_cast<int>((rgb >> 16) & 0xFF), static_cast<int>((rgb >> 8) & 0xFF), static_cast<int>(rgb & 0xFF)};
}

double linear(int channel)
{
    double value = channel / 255.0;
    return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

// Quase preto e quase branco são fundo e bordas da capa; a cor do álbum está no resto.
bool isUseful(Rgb color)
{
    int brightest = std::max({color.red, color.green, color.blue});
    int darkest = std::min({color.red, color.green, color.blue});
    return brightest >= DARKEST_USEFUL && darkest <= BRIGHTEST_USEFUL;
}

size_t binOf(Rgb color)
{
    return static_cast<size_t>((color.red >> 4) * BINS_PER_CHANNEL * BINS_PER_CHANNEL
        + (color.green >> 4) * BINS_PER_CHANNEL + (color.blue >> 4));
}

}

namespace DominantColor {

double luminance(uint32_t rgb)
{
    Rgb color = unpack(rgb);
    return 0.2126 * linear(color.red) + 0.7152 * linear(color.green) + 0.0722 * linear(color.blue);
}

uint32_t of(const uint16_t* pixels, size_t count)
{
    std::vector<Bin> bins(BINS_PER_CHANNEL * BINS_PER_CHANNEL * BINS_PER_CHANNEL);
    for (size_t index = 0; index < count; index += SAMPLE_STEP) {
        Rgb color = expand(pixels[index]);
        if (!isUseful(color)) {
            continue;
        }

        Bin& bin = bins[binOf(color)];
        bin.count++;
        bin.red += color.red;
        bin.green += color.green;
        bin.blue += color.blue;
    }

    auto fullest = std::max_element(bins.begin(), bins.end(), [](const Bin& left, const Bin& right) {
        return left.count < right.count;
    });
    if (fullest->count == 0) {
        return FALLBACK;
    }

    Rgb color{static_cast<int>(fullest->red / fullest->count), static_cast<int>(fullest->green / fullest->count),
        static_cast<int>(fullest->blue / fullest->count)};
    while (luminance(pack(color)) > MAX_LUMINANCE) {
        color.red = static_cast<int>(color.red * DARKEN_FACTOR);
        color.green = static_cast<int>(color.green * DARKEN_FACTOR);
        color.blue = static_cast<int>(color.blue * DARKEN_FACTOR);
    }
    return pack(color);
}

}
