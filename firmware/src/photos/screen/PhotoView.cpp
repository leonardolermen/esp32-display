#include "photos/screen/PhotoView.h"

#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

#include "board/ScreenGeometry.h"

namespace {

struct Target {
    uint16_t* pixels;
};

}

PhotoView::PhotoView(PhotoStore& store)
    : store(store)
{
}

// A foto inteira vai para a PSRAM antes de decodificar: tem no máximo 200 KB e a PSRAM é folgada, e assim o JPEGDEC
// não precisa dos callbacks de arquivo.
bool PhotoView::load(const PhotoName& name, lv_image_dsc_t& into)
{
    if (into.data == nullptr) {
        return false;
    }

    File file = store.open(name);
    if (!file) {
        return false;
    }

    size_t size = file.size();
    uint8_t* jpeg = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (jpeg == nullptr) {
        file.close();
        return false;
    }
    file.read(jpeg, size);
    file.close();

    Target target{reinterpret_cast<uint16_t*>(const_cast<uint8_t*>(into.data))};
    bool decoded = false;
    if (decoder.openRAM(jpeg, size, copyBlock)) {
        decoder.setUserPointer(&target);
        decoder.setPixelType(RGB565_LITTLE_ENDIAN);
        decoded = decoder.decode(0, 0, 0) == 1;
        decoder.close();
    }
    heap_caps_free(jpeg);

    return decoded;
}

// As fotos já chegam em 480×320 (o navegador recorta); se vier diferente, só a parte que cabe é copiada.
int PhotoView::copyBlock(JPEGDRAW* block)
{
    Target* target = static_cast<Target*>(block->pUser);

    for (int row = 0; row < block->iHeight; row++) {
        int y = block->y + row;
        if (y >= SCREEN_HEIGHT) {
            break;
        }

        int width = std::min(block->iWidth, SCREEN_WIDTH - block->x);
        if (width <= 0) {
            break;
        }

        memcpy(target->pixels + y * SCREEN_WIDTH + block->x, block->pPixels + row * block->iWidth, width * 2);
    }

    return 1;
}
