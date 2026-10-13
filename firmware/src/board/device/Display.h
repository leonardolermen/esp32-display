#pragma once

#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include <memory>

#include "board/ScreenRotation.h"

class Display {
public:
    explicit Display(const ScreenRotation& rotation);

    lv_display_t* begin();

private:
    static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);

    const ScreenRotation& rotation;
    std::unique_ptr<Arduino_ESP32QSPI> bus;
    std::unique_ptr<Arduino_AXS15231B> panel;
    uint16_t* screenFrame = nullptr;
    uint16_t* panelFrame = nullptr;
};
