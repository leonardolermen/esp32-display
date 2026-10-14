#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>
#include <memory>

#include "birthday/BirthdayCalendar.h"
#include "birthday/screen/BirthdayOverlay.h"
#include "birthday/storage/BirthdayMemory.h"
#include "birthday/web/BirthdayRoutes.h"
#include "board/ScreenRotation.h"
#include "board/device/Backlight.h"
#include "board/device/Display.h"
#include "board/device/Touch.h"
#include "crypto/device/MarketFeed.h"
#include "crypto/screen/CryptoPage.h"
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
#include "support/LocalTime.h"
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
BirthdayMemory birthdayMemory;
BirthdayOverlay birthdayOverlay;
BirthdayRoutes birthdayRoutes(http);
MarketFeed marketFeed;

constexpr uint32_t BIRTHDAY_CHECK_EVERY_MILLIS = 60000;

uint32_t seenSetupVersion = 0;
bool setupWasPending = false;
bool configured = false;
bool birthdayOnScreen = false;
uint32_t lastBirthdayCheckMillis = 0;
uint32_t seenReplayRequests = 0;

void logToSerial(lv_log_level_t, const char* text)
{
    Serial.print(text);
}

void networkTask(void*)
{
    wifi.begin();
    adminRoutes.begin();
    photoRoutes.begin();
    birthdayRoutes.begin();
    http.begin();
    networkClock.begin();

    for (;;) {
        wifi.process();
        marketFeed.poll(millis(), wifi.connected());
        setupState.publish(SetupProgress::compute(wifi.connected(), wifi.ip(), adminSecret.isSet()));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Quando a configuração termina, reiniciar é o jeito mais simples de montar as páginas normais sem estado meio
// trocado. Só vale com a tela de configuração na tela: numa placa pronta, o Wi-Fi caindo e voltando passaria por
// WaitingWifi → Done e reiniciaria o aparelho a cada queda do roteador.
void restartWhenSetupCompletes()
{
    if (configured) {
        return;
    }

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

void showBirthday()
{
    if (birthdayOnScreen) {
        return;
    }

    birthdayOnScreen = true;
    birthdayOverlay.show([]() {
        std::optional<LocalTime> now = LocalTime::now();
        birthdayMemory.markShown(now ? std::optional<int>(now->year) : std::nullopt);
        birthdayOnScreen = false;
    });
}

void showBirthdayIfDue()
{
    if (BirthdayCalendar::shouldShow(birthdayMemory.neverShown(), LocalTime::now(), birthdayMemory.lastShownYear())) {
        showBirthday();
    }
}

// A página tem um botão "mostrar de novo": serve para testar e para o pai reler quando quiser.
void replayBirthdayWhenAsked()
{
    uint32_t requests = 0;
    if (birthdayRoutes.replayRequests().readIfChanged(requests, seenReplayRequests)) {
        showBirthday();
    }
}

// No ligar a frio não há relógio: o 22/10 só é conhecido depois do NTP, ou quando a meia-noite passa com a placa
// ligada. Por isso a data é reconferida de tempos em tempos, não só no boot.
void checkBirthdayPeriodically(uint32_t nowMillis)
{
    if (nowMillis - lastBirthdayCheckMillis < BIRTHDAY_CHECK_EVERY_MILLIS) {
        return;
    }

    lastBirthdayCheckMillis = nowMillis;
    showBirthdayIfDue();
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

    photoStore.begin();
    adminSecret.begin();

    configured = adminSecret.isSet() && wifi.hasSavedNetwork();
    if (configured) {
        navigator.add(std::make_unique<HomePage>(photoStore));
        navigator.add(std::make_unique<CryptoPage>(marketFeed.state()));
    } else {
        navigator.add(std::make_unique<SetupPage>(setupState));
    }
    navigator.begin();

    // A tarefa de rede sobe em paralelo: a mensagem do primeiro boot nunca espera Wi-Fi.
    birthdayMemory.begin();
    showBirthdayIfDue();

    xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 1, nullptr, 0);
}

void loop()
{
    restartWhenSetupCompletes();
    checkBirthdayPeriodically(millis());
    replayBirthdayWhenAsked();
    navigator.tick(millis());
    uint32_t untilNextRun = lv_timer_handler();

    delay(std::min(untilNextRun, MAX_LOOP_SLEEP_MILLIS));
}
