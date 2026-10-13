#pragma once

#include <Arduino_GFX_Library.h>
#include <esp_lcd_panel_io.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include <memory>

#include "board/ScreenRotation.h"

// O LVGL renderiza no núcleo 1 enquanto uma tarefa no núcleo 0 gira o quadro anterior em faixas e o DMA manda cada
// faixa ao painel enquanto a próxima é girada. O quadro custa a etapa mais lenta, não a soma das três.
class Display {
public:
    explicit Display(const ScreenRotation& rotation);

    lv_display_t* begin();

private:
    static constexpr uint16_t BAND_ROWS = 32;
    static constexpr uint8_t BAND_BUFFERS = 2;

    static void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
    static void waitForPanel(lv_display_t* display);
    static bool bandSent(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t* event, void* context);
    static void sendFrames(void* context);

    void openPixelChannel();
    void sendFrame(const uint16_t* landscape);

    const ScreenRotation& rotation;
    std::unique_ptr<Arduino_ESP32QSPI> bus;
    std::unique_ptr<Arduino_AXS15231B> panel;
    esp_lcd_panel_io_handle_t pixelChannel = nullptr;
    uint16_t* bands[BAND_BUFFERS] = {};
    QueueHandle_t framesToSend = nullptr;
    SemaphoreHandle_t freeBands = nullptr;
    SemaphoreHandle_t frameRotated = nullptr;
};
