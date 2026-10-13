#pragma once

#include <lvgl.h>

namespace theme {

lv_color_t background();
lv_color_t surface();
lv_color_t text();
lv_color_t textSecondary();
lv_color_t accent();
lv_color_t amber();

void apply(lv_display_t* display);
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color);
lv_obj_t* panel(lv_obj_t* parent);

}
