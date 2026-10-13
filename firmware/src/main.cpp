#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>

#include "board/ScreenRotation.h"
#include "board/device/Backlight.h"
#include "board/device/Display.h"
#include "board/device/Touch.h"

namespace {

constexpr uint32_t MAX_LOOP_SLEEP_MILLIS = 10;

Rotation90 rotation;
Display display(rotation);
Touch touch(rotation);
Backlight backlight;
uint32_t taps = 0;

void logToSerial(lv_log_level_t, const char* text)
{
    Serial.print(text);
}

lv_obj_t* addCornerMarker(lv_obj_t* screen, lv_align_t corner, uint32_t color)
{
    lv_obj_t* marker = lv_obj_create(screen);
    lv_obj_set_size(marker, 40, 40);
    lv_obj_set_style_bg_color(marker, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(marker, 0, 0);
    lv_obj_set_style_radius(marker, 0, 0);
    lv_obj_align(marker, corner, 0, 0);
    return marker;
}

void countTap(lv_event_t* event)
{
    lv_obj_t* label = static_cast<lv_obj_t*>(lv_event_get_user_data(event));

    taps++;
    lv_label_set_text_fmt(label, "Toques: %lu", static_cast<unsigned long>(taps));
}

void printTouchedPoint(lv_event_t*)
{
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);

    Serial.printf("toque em x=%d y=%d\n", static_cast<int>(point.x), static_cast<int>(point.y));
}

void showBringUpScreen()
{
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1F1E1C), 0);
    lv_obj_add_event_cb(screen, printTouchedPoint, LV_EVENT_PRESSED, nullptr);

    addCornerMarker(screen, LV_ALIGN_TOP_LEFT, 0xFF0000);
    addCornerMarker(screen, LV_ALIGN_TOP_RIGHT, 0x00FF00);
    addCornerMarker(screen, LV_ALIGN_BOTTOM_LEFT, 0x0000FF);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Claudinho - bring-up");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF5F1EA), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t* button = lv_button_create(screen);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, -20);
    lv_obj_t* buttonLabel = lv_label_create(button);
    lv_label_set_text(buttonLabel, "Toques: 0");
    lv_obj_add_event_cb(button, countTap, LV_EVENT_CLICKED, buttonLabel);

    lv_obj_t* spinner = lv_spinner_create(screen);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 60);
}

}

void setup()
{
    Serial.begin(115200);
    // Com a USB ligada e ninguém lendo a serial, cada print esperava até 2 s pelo buffer cheio e congelava a tela.
    // Sem espera, o log é descartado quando não há leitor e o loop do LVGL nunca para por causa dele.
    Serial.setTxTimeoutMs(0);

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });
    lv_log_register_print_cb(logToSerial);

    display.begin();
    touch.begin();
    backlight.begin();

    showBringUpScreen();
}

void loop()
{
    uint32_t untilNextRun = lv_timer_handler();

    delay(std::min(untilNextRun, MAX_LOOP_SLEEP_MILLIS));
}
