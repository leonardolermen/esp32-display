#include "board/device/Display.h"

#include <esp_heap_caps.h>
#include <esp_lcd_io_spi.h>
#include <esp_timer.h>

#include <atomic>

#include "board/BoardPins.h"

namespace {

constexpr size_t FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);

// O AXS15231B só aceita até 32 MHz no QSPI; é a velocidade que o Arduino_GFX usa para inicializá-lo.
constexpr unsigned int PIXEL_CLOCK_HZ = 32 * 1000 * 1000;

// Comandos QSPI do AXS15231B: o byte de opcode (0x02 para registrador, 0x32 para pixels em 4 linhas) seguido do
// comando no meio do endereço de 24 bits. 0x2C recomeça a escrita no topo da janela e 0x3C continua de onde parou.
constexpr int SET_COLUMNS = (0x02 << 24) | (0x2A << 8);
constexpr int SET_ROWS = (0x02 << 24) | (0x2B << 8);
constexpr int WRITE_FROM_TOP = (0x32 << 24) | (0x2C << 8);
constexpr int WRITE_CONTINUING = (0x32 << 24) | (0x3C << 8);

constexpr UBaseType_t PANEL_TASK_PRIORITY = 5;
constexpr uint32_t PANEL_TASK_STACK_BYTES = 4096;
constexpr BaseType_t PANEL_TASK_CORE = 0;

uint16_t* allocateFrame()
{
    return static_cast<uint16_t*>(heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

}

#ifdef CLAUDINHO_PROFILE_FLUSH
namespace {

// Medição do envio para achar onde o quadro gasta tempo: a rotação no núcleo 0, a espera pelo SPI, e quanto o LVGL
// ficou parado esperando o painel liberar o buffer.
struct PanelProfile {
    int64_t windowStart = 0;
    int64_t rotateMicros = 0;
    int64_t spiWaitMicros = 0;
    uint32_t frames = 0;
};

PanelProfile profile;
std::atomic<int64_t> lvglWaitMicros{0};

void reportPanelProfile(int64_t frameEnd)
{
    profile.frames++;
    if (profile.windowStart == 0) {
        profile.windowStart = frameEnd;
    }
    if (frameEnd - profile.windowStart < 1000000) {
        return;
    }

    uint32_t frames = profile.frames;
    Serial.printf("panel: %u fps, rotate %.1f ms, spi wait %.1f ms, lvgl waited %.1f ms\n", frames,
        profile.rotateMicros / 1000.0 / frames, profile.spiWaitMicros / 1000.0 / frames,
        lvglWaitMicros.exchange(0) / 1000.0 / frames);

    profile = PanelProfile{};
    profile.windowStart = frameEnd;
}

}
#endif

Display::Display(const ScreenRotation& rotation)
    : rotation(rotation)
{
}

lv_display_t* Display::begin()
{
    // Compartilhado: o Arduino_GFX só segura o barramento enquanto inicializa, e depois os pixels seguem pelo esp_lcd.
    bus = std::make_unique<Arduino_ESP32QSPI>(
        pins::LCD_CS, pins::LCD_SCK, pins::LCD_D0, pins::LCD_D1, pins::LCD_D2, pins::LCD_D3, true);
    // Desde a 1.6.1 o driver precisa da sequência de inicialização explícita; com a padrão a tela pisca
    // (issues #717 e #803 do Arduino_GFX). Se a imagem vier errada, a alternativa é a type2.
    panel = std::make_unique<Arduino_AXS15231B>(
        bus.get(), GFX_NOT_DEFINED, 0, false, PANEL_WIDTH, PANEL_HEIGHT, 0, 0, 0, 0,
        axs15231b_320480_type1_init_operations, sizeof(axs15231b_320480_type1_init_operations));
    if (!panel->begin()) {
        log_e("the display panel did not start");
    }
    panel->fillScreen(0x0000);

    openPixelChannel();

    lv_display_t* display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_user_data(display, this);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_flush_wait_cb(display, waitForPanel);
    // O AXS15231B corrompe a imagem com janelas parciais: o LVGL sempre renderiza e manda o quadro inteiro. Com dois
    // buffers ele desenha o próximo quadro enquanto o anterior vai para o painel.
    lv_display_set_buffers(display, allocateFrame(), allocateFrame(), FRAME_BYTES, LV_DISPLAY_RENDER_MODE_FULL);

    return display;
}

void Display::openPixelChannel()
{
    esp_lcd_panel_io_spi_config_t config = {};
    config.cs_gpio_num = pins::LCD_CS;
    config.dc_gpio_num = -1;
    config.spi_mode = 0;
    config.pclk_hz = PIXEL_CLOCK_HZ;
    config.trans_queue_depth = BAND_BUFFERS;
    config.on_color_trans_done = bandSent;
    config.user_ctx = this;
    config.lcd_cmd_bits = 32;
    config.lcd_param_bits = 8;
    config.flags.quad_mode = true;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST), &config, &pixelChannel));

    const uint8_t columns[] = {0x00, 0x00, (PANEL_WIDTH - 1) >> 8, (PANEL_WIDTH - 1) & 0xFF};
    const uint8_t rows[] = {0x00, 0x00, (PANEL_HEIGHT - 1) >> 8, (PANEL_HEIGHT - 1) & 0xFF};
    esp_lcd_panel_io_tx_param(pixelChannel, SET_COLUMNS, columns, sizeof(columns));
    esp_lcd_panel_io_tx_param(pixelChannel, SET_ROWS, rows, sizeof(rows));

    // As faixas ficam na RAM interna, fora da regra da PSRAM: o DMA lê dela sem passar pelo cache, e a rotação
    // escreve fora de ordem sem pagar uma linha de cache por pixel.
    for (uint16_t*& band : bands) {
        band = static_cast<uint16_t*>(
            heap_caps_malloc(PANEL_WIDTH * BAND_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    }
    framesToSend = xQueueCreate(1, sizeof(const uint16_t*));
    freeBands = xSemaphoreCreateCounting(BAND_BUFFERS, BAND_BUFFERS);
    frameRotated = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(
        sendFrames, "panel", PANEL_TASK_STACK_BYTES, this, PANEL_TASK_PRIORITY, nullptr, PANEL_TASK_CORE);
}

// Roda no loop do LVGL: entrega o quadro à tarefa do painel e volta a renderizar.
void Display::flush(lv_display_t* display, const lv_area_t*, uint8_t* pixels)
{
    Display* self = static_cast<Display*>(lv_display_get_user_data(display));
    const uint16_t* landscape = reinterpret_cast<const uint16_t*>(pixels);

    xQueueSend(self->framesToSend, &landscape, portMAX_DELAY);
}

// O LVGL chama antes de reusar um buffer. Fica pronto assim que a última faixa foi girada, sem esperar o SPI.
void Display::waitForPanel(lv_display_t* display)
{
    Display* self = static_cast<Display*>(lv_display_get_user_data(display));

#ifdef CLAUDINHO_PROFILE_FLUSH
    int64_t waitStart = esp_timer_get_time();
#endif
    xSemaphoreTake(self->frameRotated, portMAX_DELAY);
#ifdef CLAUDINHO_PROFILE_FLUSH
    lvglWaitMicros += esp_timer_get_time() - waitStart;
#endif
}

bool Display::bandSent(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* context)
{
    Display* self = static_cast<Display*>(context);
    BaseType_t higherPriorityWoken = pdFALSE;

    xSemaphoreGiveFromISR(self->freeBands, &higherPriorityWoken);

    return higherPriorityWoken == pdTRUE;
}

void Display::sendFrames(void* context)
{
    Display* self = static_cast<Display*>(context);

    while (true) {
        const uint16_t* landscape = nullptr;
        xQueueReceive(self->framesToSend, &landscape, portMAX_DELAY);
        self->sendFrame(landscape);
    }
}

void Display::sendFrame(const uint16_t* landscape)
{
    constexpr size_t BAND_BYTES = PANEL_WIDTH * BAND_ROWS * sizeof(uint16_t);
    static_assert(PANEL_HEIGHT % BAND_ROWS == 0, "the bands must cover the panel");

    for (uint16_t firstRow = 0; firstRow < PANEL_HEIGHT; firstRow += BAND_ROWS) {
#ifdef CLAUDINHO_PROFILE_FLUSH
        int64_t waitStart = esp_timer_get_time();
#endif
        xSemaphoreTake(freeBands, portMAX_DELAY);
        uint16_t* band = bands[(firstRow / BAND_ROWS) % BAND_BUFFERS];
#ifdef CLAUDINHO_PROFILE_FLUSH
        int64_t rotateStart = esp_timer_get_time();
        profile.spiWaitMicros += rotateStart - waitStart;
#endif
        rotation.rotateRows(landscape, firstRow, BAND_ROWS, band);
#ifdef CLAUDINHO_PROFILE_FLUSH
        int64_t sendStart = esp_timer_get_time();
        profile.rotateMicros += sendStart - rotateStart;
#endif

        bool lastBand = firstRow + BAND_ROWS == PANEL_HEIGHT;
        if (lastBand) {
            xSemaphoreGive(frameRotated);
        }
        esp_lcd_panel_io_tx_color(pixelChannel, firstRow == 0 ? WRITE_FROM_TOP : WRITE_CONTINUING, band, BAND_BYTES);
#ifdef CLAUDINHO_PROFILE_FLUSH
        profile.spiWaitMicros += esp_timer_get_time() - sendStart;
#endif
    }
#ifdef CLAUDINHO_PROFILE_FLUSH
    reportPanelProfile(esp_timer_get_time());
#endif
}
