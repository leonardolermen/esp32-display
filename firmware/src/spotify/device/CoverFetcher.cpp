#include "spotify/device/CoverFetcher.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "spotify/CoverScaler.h"
#include "spotify/DominantColor.h"
#include "support/device/Https.h"

namespace {

constexpr size_t MAX_COVER_BYTES = 256 * 1024;
constexpr int HTTP_OK = 200;
// Na ordem do deslocamento: capas grandes decodificam já reduzidas no JPEGDEC, com menos PSRAM e o mesmo resultado.
constexpr int SCALE_FLAGS[] = {0, JPEG_SCALE_HALF, JPEG_SCALE_QUARTER, JPEG_SCALE_EIGHTH};

struct Target {
    uint16_t* pixels;
    int width;
    int height;
};

}

std::optional<CoverArt> CoverFetcher::fetch(const std::string& url)
{
    HttpsRequest request;
    request.url = url;
    request.maxBodyBytes = MAX_COVER_BYTES;

    HttpsResponse response = Https::send(request);
    if (response.status != HTTP_OK || !response.body) {
        return std::nullopt;
    }

    // O JPEGDEC tem buffers internos de ~17 KB; alocado sob demanda, vai para a PSRAM.
    if (!decoder) {
        decoder = std::make_unique<JPEGDEC>();
    }

    std::string_view jpeg = response.body->view();
    uint8_t* data = reinterpret_cast<uint8_t*>(const_cast<char*>(jpeg.data()));
    if (!decoder->openRAM(data, static_cast<int>(jpeg.size()), copyBlock)) {
        return std::nullopt;
    }

    int shift = CoverScaler::jpegScaleShift(decoder->getWidth());
    int scale = SCALE_FLAGS[shift];
    int width = decoder->getWidth() >> shift;
    int height = decoder->getHeight() >> shift;
    std::vector<uint16_t> decoded(static_cast<size_t>(width) * height);
    Target target{decoded.data(), width, height};

    decoder->setUserPointer(&target);
    decoder->setPixelType(RGB565_LITTLE_ENDIAN);
    bool decodedOk = decoder->decode(0, 0, scale) == 1;
    decoder->close();
    if (!decodedOk) {
        return std::nullopt;
    }

    CoverArt art;
    art.pixels.resize(COVER_SIZE * COVER_SIZE);
    CoverScaler::downscale(decoded.data(), width, height, art.pixels.data(), COVER_SIZE);
    art.color = DominantColor::of(art.pixels.data(), art.pixels.size());
    return art;
}

int CoverFetcher::copyBlock(JPEGDRAW* block)
{
    Target* target = static_cast<Target*>(block->pUser);

    for (int row = 0; row < block->iHeight; row++) {
        int y = block->y + row;
        int width = std::min(block->iWidth, target->width - block->x);
        if (y >= target->height || width <= 0) {
            break;
        }

        memcpy(target->pixels + y * target->width + block->x, block->pPixels + row * block->iWidth, width * 2);
    }

    return 1;
}
