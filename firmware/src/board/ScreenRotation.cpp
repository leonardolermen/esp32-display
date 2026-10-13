#include "board/ScreenRotation.h"

void Rotation90::rotate(const uint16_t* landscape, uint16_t* portrait) const
{
    for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
        const uint16_t* row = landscape + y * SCREEN_WIDTH;
        uint16_t panelX = PANEL_WIDTH - 1 - y;

        for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
            portrait[x * PANEL_WIDTH + panelX] = row[x];
        }
    }
}

ScreenPoint Rotation90::toScreen(PanelPoint touched) const
{
    return ScreenPoint{touched.y, static_cast<uint16_t>(PANEL_WIDTH - 1 - touched.x)};
}

void Rotation270::rotate(const uint16_t* landscape, uint16_t* portrait) const
{
    for (uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
        const uint16_t* row = landscape + y * SCREEN_WIDTH;

        for (uint16_t x = 0; x < SCREEN_WIDTH; x++) {
            portrait[(PANEL_HEIGHT - 1 - x) * PANEL_WIDTH + y] = row[x];
        }
    }
}

ScreenPoint Rotation270::toScreen(PanelPoint touched) const
{
    return ScreenPoint{static_cast<uint16_t>(PANEL_HEIGHT - 1 - touched.y), touched.x};
}
