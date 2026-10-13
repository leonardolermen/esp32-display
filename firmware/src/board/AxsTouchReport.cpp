#include "board/AxsTouchReport.h"

std::optional<PanelPoint> AxsTouchReport::parse(const uint8_t (&report)[LENGTH])
{
    // A leitura pede um único ponto; qualquer quantidade diferente de 1 é "sem dedo" ou ruído do barramento.
    uint8_t touchCount = report[1];
    if (touchCount != 1) {
        return std::nullopt;
    }

    uint16_t x = static_cast<uint16_t>(((report[2] & 0x0F) << 8) | report[3]);
    uint16_t y = static_cast<uint16_t>(((report[4] & 0x0F) << 8) | report[5]);
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) {
        return std::nullopt;
    }

    return PanelPoint{x, y};
}
