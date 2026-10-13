#include "birthday/screen/BirthdayOverlay.h"

#include <Arduino.h>

#include "birthday/BirthdayContent.h"
#include "board/ScreenGeometry.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int CONFETTI_COUNT = 24;
constexpr uint32_t FALL_MIN_MILLIS = 3000;
constexpr uint32_t FALL_MAX_MILLIS = 5000;
constexpr uint32_t FADE_OUT_MILLIS = 400;
constexpr uint32_t COLORS[] = {0xD97757, 0xE8B04A, 0x8BB8E8, 0x9FD49A};
constexpr int COLOR_COUNT = 4;

void fall(void* object, int32_t y)
{
    lv_obj_set_y(static_cast<lv_obj_t*>(object), y);
}

}

void BirthdayOverlay::show(std::function<void()> onDismiss)
{
    dismiss = std::move(onDismiss);

    layer = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(layer, theme::background(), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_COVER, 0);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(layer, onTap, LV_EVENT_CLICKED, this);

    addConfetti(layer);

    lv_obj_t* date = theme::label(layer, fonts::small, theme::textSecondary());
    lv_label_set_text_fmt(date, "%d de outubro", birthday::DAY);
    lv_obj_align(date, LV_ALIGN_CENTER, 0, -70);

    lv_obj_t* title = theme::label(layer, fonts::title, theme::text());
    lv_label_set_text(title, birthday::TITLE);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t* message = theme::label(layer, fonts::medium, theme::text());
    lv_label_set_text(message, birthday::MESSAGE);
    lv_label_set_long_mode(message, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(message, 320);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(message, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t* signature = theme::label(layer, fonts::small, theme::textSecondary());
    lv_label_set_text(signature, birthday::SIGNATURE);
    lv_obj_align(signature, LV_ALIGN_CENTER, 0, 60);

    lv_obj_t* hint = theme::label(layer, fonts::small, theme::textSecondary());
    lv_label_set_text(hint, "toque para começar");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void BirthdayOverlay::addConfetti(lv_obj_t* parent)
{
    for (int piece = 0; piece < CONFETTI_COUNT; piece++) {
        lv_obj_t* confetti = lv_obj_create(parent);
        lv_obj_remove_style_all(confetti);
        int32_t size = 5 + random(4);
        lv_obj_set_size(confetti, size, size);
        lv_obj_set_style_bg_color(confetti, lv_color_hex(COLORS[piece % COLOR_COUNT]), 0);
        lv_obj_set_style_bg_opa(confetti, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(confetti, piece % 3 == 0 ? LV_RADIUS_CIRCLE : 1, 0);
        lv_obj_set_x(confetti, random(SCREEN_WIDTH));

        lv_anim_t drop;
        lv_anim_init(&drop);
        lv_anim_set_var(&drop, confetti);
        lv_anim_set_values(&drop, -10, SCREEN_HEIGHT + 10);
        lv_anim_set_duration(&drop, FALL_MIN_MILLIS + random(FALL_MAX_MILLIS - FALL_MIN_MILLIS));
        lv_anim_set_delay(&drop, random(FALL_MAX_MILLIS));
        lv_anim_set_repeat_count(&drop, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&drop, fall);
        lv_anim_start(&drop);
    }
}

void BirthdayOverlay::onTap(lv_event_t* event)
{
    BirthdayOverlay* self = static_cast<BirthdayOverlay*>(lv_event_get_user_data(event));

    lv_obj_fade_out(self->layer, FADE_OUT_MILLIS, 0);
    lv_obj_delete_delayed(self->layer, FADE_OUT_MILLIS);
    self->layer = nullptr;

    if (self->dismiss) {
        self->dismiss();
    }
}
