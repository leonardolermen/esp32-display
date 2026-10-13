#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>
#include <memory>

#include "board/ScreenRotation.h"
#include "board/device/Backlight.h"
#include "board/device/Display.h"
#include "board/device/Touch.h"
#include "navigation/screen/Navigator.h"
#include "setup/SetupProgress.h"
#include "setup/device/WifiLink.h"
#include "setup/screen/SetupPage.h"
#include "support/Published.h"
#include "support/device/NetworkClock.h"
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

Published<SetupState> setupState;
WifiLink wifi;
NetworkClock networkClock;

void networkTask(void*)
{
    wifi.begin();
    networkClock.begin();

    for (;;) {
        wifi.process();
        setupState.publish(SetupProgress::compute(wifi.connected(), wifi.ip(), false));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
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

    lv_display_t* lvDisplay = display.begin();
    theme::apply(lvDisplay);
    touch.begin();
    backlight.begin();

    navigator.add(std::make_unique<SetupPage>(setupState));
    navigator.begin();

    xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 1, nullptr, 0);
}

void loop()
{
    navigator.tick(millis());
    uint32_t untilNextRun = lv_timer_handler();

    delay(std::min(untilNextRun, MAX_LOOP_SLEEP_MILLIS));
}
