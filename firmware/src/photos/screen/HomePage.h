#pragma once

#include <lvgl.h>

#include <optional>
#include <vector>

#include "navigation/Page.h"
#include "photos/ShuffleOrder.h"
#include "photos/screen/PhotoView.h"
#include "photos/storage/PhotoStore.h"

class HomePage : public Page {
public:
    explicit HomePage(PhotoStore& store);

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onTick(uint32_t nowMillis) override;

private:
    void showNextPhoto(uint32_t nowMillis);
    void refreshClock();
    static lv_image_dsc_t makeFrame();

    PhotoStore& store;
    PhotoView view;
    std::optional<ShuffleOrder> order;
    std::vector<PhotoName> names;

    lv_image_dsc_t frames[2] = {};
    lv_obj_t* images[2] = {};
    size_t visible = 0;
    uint32_t lastSwapMillis = 0;
    uint32_t lastClockMillis = 0;

    lv_obj_t* invite = nullptr;
    lv_obj_t* band = nullptr;
    lv_obj_t* clockLabel = nullptr;
    lv_obj_t* dateLabel = nullptr;
};
