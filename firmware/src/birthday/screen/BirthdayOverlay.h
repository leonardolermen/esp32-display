#pragma once

#include <lvgl.h>

#include <functional>

class BirthdayOverlay {
public:
    void show(std::function<void()> onDismiss);

private:
    static void onTap(lv_event_t* event);
    void addConfetti(lv_obj_t* parent);

    lv_obj_t* layer = nullptr;
    std::function<void()> dismiss;
};
