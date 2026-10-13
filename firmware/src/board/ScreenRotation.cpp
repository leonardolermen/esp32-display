#include "board/ScreenRotation.h"

namespace {

// O painel lê cada pixel com o byte alto primeiro. Inverter aqui sai de graça, já que a rotação toca todo pixel, e
// poupa o envio de inverter de novo.
inline uint16_t bigEndian(uint16_t pixel)
{
    return static_cast<uint16_t>((pixel << 8) | (pixel >> 8));
}

}

// Uma linha do painel é uma coluna da tela. Para cada linha da tela, os pixels da faixa estão lado a lado na PSRAM e
// são lidos de uma vez pelo cache; o destino espalhado é a RAM interna, onde escrever fora de ordem não custa nada.
void Rotation90::rotateRows(
    const uint16_t* landscape, uint16_t firstRow, uint16_t rowCount, uint16_t* portraitRows) const
{
    for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
        const uint16_t* source = landscape + y * SCREEN_WIDTH + firstRow;
        uint16_t* target = portraitRows + (PANEL_WIDTH - 1 - y);

        for (uint16_t row = 0; row < rowCount; row++) {
            target[row * PANEL_WIDTH] = bigEndian(source[row]);
        }
    }
}

ScreenPoint Rotation90::toScreen(PanelPoint touched) const
{
    return ScreenPoint{touched.y, static_cast<uint16_t>(PANEL_WIDTH - 1 - touched.x)};
}

void Rotation270::rotateRows(
    const uint16_t* landscape, uint16_t firstRow, uint16_t rowCount, uint16_t* portraitRows) const
{
    for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
        const uint16_t* source = landscape + y * SCREEN_WIDTH + (SCREEN_WIDTH - 1 - firstRow);
        uint16_t* target = portraitRows + y;

        for (uint16_t row = 0; row < rowCount; row++) {
            target[row * PANEL_WIDTH] = bigEndian(*(source - row));
        }
    }
}

ScreenPoint Rotation270::toScreen(PanelPoint touched) const
{
    return ScreenPoint{static_cast<uint16_t>(PANEL_HEIGHT - 1 - touched.y), touched.x};
}
