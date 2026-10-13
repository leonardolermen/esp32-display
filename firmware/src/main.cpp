#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>
#include <memory>

#include "board/ScreenRotation.h"
#include "board/device/Backlight.h"
#include "board/device/Display.h"
#include "board/device/Touch.h"
#include "navigation/screen/Navigator.h"
#include "photos/screen/HomePage.h"
#include "photos/storage/PhotoStore.h"
#include "photos/web/PhotoRoutes.h"
#include "setup/SetupProgress.h"
#include "setup/device/WifiLink.h"
#include "setup/screen/SetupPage.h"
#include "setup/storage/AdminSecret.h"
#include "setup/web/AdminRoutes.h"
#include "setup/web/HttpServer.h"
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

Published<SetupState> setupState;
WifiLink wifi;
NetworkClock networkClock;
AdminSecret adminSecret;
HttpServer http(adminSecret);
AdminRoutes adminRoutes(http, adminSecret, wifi);
PhotoStore photoStore;
PhotoRoutes photoRoutes(http, photoStore);

uint32_t seenSetupVersion = 0;
bool setupWasPending = false;

void logToSerial(lv_log_level_t, const char* text)
{
    Serial.print(text);
}

void networkTask(void*)
{
    wifi.begin();
    adminRoutes.begin();
    photoRoutes.begin();
    http.begin();
    networkClock.begin();

    for (;;) {
        wifi.process();
        setupState.publish(SetupProgress::compute(wifi.connected(), wifi.ip(), adminSecret.isSet()));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Quando a configuração termina, reiniciar é o jeito mais simples de montar as páginas normais sem estado meio
// trocado. Só reinicia se esta execução viu a configuração pendente: um boot que já começa pronto não entra em loop.
void restartWhenSetupCompletes()
{
    SetupState current;
    if (!setupState.readIfChanged(current, seenSetupVersion)) {
        return;
    }

    if (current.step != SetupStep::Done) {
        setupWasPending = true;
        return;
    }

    if (setupWasPending) {
        ESP.restart();
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

    adminSecret.begin();
    photoStore.begin();

    bool configured = adminSecret.isSet() && wifi.hasSavedNetwork();
    if (configured) {
        navigator.add(std::make_unique<HomePage>(photoStore));
    } else {
        navigator.add(std::make_unique<SetupPage>(setupState));
    }
    navigator.begin();

    xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 1, nullptr, 0);
}

void loop()
{
    restartWhenSetupCompletes();
    navigator.tick(millis());
    uint32_t untilNextRun = lv_timer_handler();

    delay(std::min(untilNextRun, MAX_LOOP_SLEEP_MILLIS));
}
