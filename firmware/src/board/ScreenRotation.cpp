#include "board/ScreenRotation.h"

namespace {

// Os dois quadros moram na PSRAM, que só é rápida através do cache. Girar linha a linha escreve um pixel a cada
// 640 bytes e cada escrita custa uma linha de cache inteira: era o que segurava a tela em 4 FPS. Em blocos de
// 32×32 os 2 KB de origem do bloco ficam no cache enquanto o destino é escrito em sequência.
constexpr uint16_t TILE = 32;

static_assert(SCREEN_WIDTH % TILE == 0 && SCREEN_HEIGHT % TILE == 0, "the rotation tiles must cover the screen");

}

void Rotation90::rotate(const uint16_t* landscape, uint16_t* portrait) const
{
    for (uint16_t tileY = 0; tileY < SCREEN_HEIGHT; tileY += TILE) {
        for (uint16_t tileX = 0; tileX < SCREEN_WIDTH; tileX += TILE) {
            for (uint16_t x = tileX; x < tileX + TILE; x++) {
                const uint16_t* source = landscape + tileY * SCREEN_WIDTH + x;
                uint16_t* target = portrait + x * PANEL_WIDTH + (PANEL_WIDTH - 1 - tileY);

                for (uint16_t step = 0; step < TILE; step++) {
                    *target = *source;
                    source += SCREEN_WIDTH;
                    target--;
                }
            }
        }
    }
}

ScreenPoint Rotation90::toScreen(PanelPoint touched) const
{
    return ScreenPoint{touched.y, static_cast<uint16_t>(PANEL_WIDTH - 1 - touched.x)};
}

void Rotation270::rotate(const uint16_t* landscape, uint16_t* portrait) const
{
    for (uint16_t tileY = 0; tileY < SCREEN_HEIGHT; tileY += TILE) {
        for (uint16_t tileX = 0; tileX < SCREEN_WIDTH; tileX += TILE) {
            for (uint16_t x = tileX; x < tileX + TILE; x++) {
                const uint16_t* source = landscape + tileY * SCREEN_WIDTH + x;
                uint16_t* target = portrait + (PANEL_HEIGHT - 1 - x) * PANEL_WIDTH + tileY;

                for (uint16_t step = 0; step < TILE; step++) {
                    *target = *source;
                    source += SCREEN_WIDTH;
                    target++;
                }
            }
        }
    }
}

ScreenPoint Rotation270::toScreen(PanelPoint touched) const
{
    return ScreenPoint{static_cast<uint16_t>(PANEL_HEIGHT - 1 - touched.y), touched.x};
}
