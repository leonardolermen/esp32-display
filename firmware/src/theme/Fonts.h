#pragma once

#include <lvgl.h>

LV_FONT_DECLARE(inter_12)
LV_FONT_DECLARE(inter_14)
LV_FONT_DECLARE(inter_18)
LV_FONT_DECLARE(inter_24)
LV_FONT_DECLARE(inter_64_digits)

namespace fonts {

inline const lv_font_t* small = &inter_12;
inline const lv_font_t* medium = &inter_14;
inline const lv_font_t* large = &inter_18;
inline const lv_font_t* title = &inter_24;
inline const lv_font_t* clock_ = &inter_64_digits;

}
