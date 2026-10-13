#include "theme/Theme.h"

#include "theme/Fonts.h"

namespace theme {

lv_color_t background()
{
    return lv_color_hex(0x1F1E1C);
}

lv_color_t surface()
{
    return lv_color_hex(0x34322F);
}

lv_color_t text()
{
    return lv_color_hex(0xF5F1EA);
}

lv_color_t textSecondary()
{
    return lv_color_hex(0xA8A29A);
}

lv_color_t accent()
{
    return lv_color_hex(0xD97757);
}

lv_color_t amber()
{
    return lv_color_hex(0xE8B04A);
}

void apply(lv_display_t* display)
{
    lv_theme_t* base = lv_theme_default_init(display, accent(), amber(), true, fonts::medium);
    lv_display_set_theme(display, base);

    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, background(), 0);
    lv_obj_set_style_text_color(screen, text(), 0);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color)
{
    lv_obj_t* created = lv_label_create(parent);
    lv_obj_set_style_text_font(created, font, 0);
    lv_obj_set_style_text_color(created, color, 0);
    return created;
}

lv_obj_t* panel(lv_obj_t* parent)
{
    lv_obj_t* created = lv_obj_create(parent);
    lv_obj_remove_style_all(created);
    lv_obj_set_style_bg_color(created, surface(), 0);
    lv_obj_set_style_bg_opa(created, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(created, 10, 0);
    lv_obj_set_style_pad_all(created, 12, 0);
    return created;
}

}
