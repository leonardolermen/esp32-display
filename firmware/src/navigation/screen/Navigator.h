#pragma once

#include <lvgl.h>

#include <memory>
#include <vector>

#include "navigation/Page.h"
#include "navigation/PageIndex.h"

class Navigator {
public:
    void add(std::unique_ptr<Page> page);
    void begin();
    void tick(uint32_t nowMillis);
    void goHome();

private:
    struct Slot {
        std::unique_ptr<Page> page;
        lv_obj_t* container = nullptr;
        lv_obj_t* dot = nullptr;
    };

    static void onGesture(lv_event_t* event);
    void show(size_t target, lv_dir_t direction);
    void refreshDots();

    std::vector<Slot> slots;
    std::unique_ptr<PageIndex> index;
    lv_obj_t* dots = nullptr;
};
