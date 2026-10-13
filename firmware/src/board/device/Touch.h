#pragma once

#include <lvgl.h>

#include <optional>

#include "board/ScreenRotation.h"

class Touch {
public:
    explicit Touch(const ScreenRotation& rotation);

    lv_indev_t* begin();

private:
    static void read(lv_indev_t* device, lv_indev_data_t* data);
    std::optional<PanelPoint> readPanel();

    const ScreenRotation& rotation;
};
