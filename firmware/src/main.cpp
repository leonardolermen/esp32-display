#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>
#include <memory>

#include "board/ScreenRotation.h"
#include "board/device/Backlight.h"
#include "board/device/Display.h"
#include "board/device/Touch.h"
#include "navigation/screen/Navigator.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr uint32_t MAX_LOOP_SLEEP_MILLIS = 10;

Rotation90 rotation;
Display display(rotation);
Touch touch(rotation);
Backlight backlight;
Navigator navigator;

void logToSerial(lv_log_level_t, const char* text)
{
    Serial.print(text);
}

// Página de exemplo até as páginas de verdade existirem: um fundo de cor e um título.
class ExamplePage : public Page {
public:
    ExamplePage(const char* title, uint32_t background)
        : title(title)
        , background(background)
    {
    }

    void build(lv_obj_t* parent) override
    {
        lv_obj_set_style_bg_color(parent, lv_color_hex(background), 0);
        lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

        lv_obj_t* label = theme::label(parent, fonts::title, theme::text());
        lv_label_set_text(label, title);
        lv_obj_center(label);
    }

private:
    const char* title;
    uint32_t background;
};

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

    lv_display_t* lvDisplay = display.begin();
    theme::apply(lvDisplay);
    touch.begin();
    backlight.begin();

    navigator.add(std::make_unique<ExamplePage>("Página A — ação, coração", 0x3B6D11));
    navigator.add(std::make_unique<ExamplePage>("Página B — ação, coração", 0x185FA5));
    navigator.begin();
}

void loop()
{
    navigator.tick(millis());
    uint32_t untilNextRun = lv_timer_handler();

    delay(std::min(untilNextRun, MAX_LOOP_SLEEP_MILLIS));
}
