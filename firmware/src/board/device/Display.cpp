#include "board/device/Display.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "board/BoardPins.h"

namespace {

constexpr size_t FRAME_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t);

uint16_t* allocateFrame()
{
    return static_cast<uint16_t*>(heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

}

Display::Display(const ScreenRotation& rotation)
    : rotation(rotation)
{
}

lv_display_t* Display::begin()
{
    bus = std::make_unique<Arduino_ESP32QSPI>(
        pins::LCD_CS, pins::LCD_SCK, pins::LCD_D0, pins::LCD_D1, pins::LCD_D2, pins::LCD_D3);
    // Desde a 1.6.1 o driver precisa da sequência de inicialização explícita; com a padrão a tela pisca
    // (issues #717 e #803 do Arduino_GFX). Se a imagem vier errada, a alternativa é a type2.
    panel = std::make_unique<Arduino_AXS15231B>(
        bus.get(), GFX_NOT_DEFINED, 0, false, PANEL_WIDTH, PANEL_HEIGHT, 0, 0, 0, 0,
        axs15231b_320480_type1_init_operations, sizeof(axs15231b_320480_type1_init_operations));
    if (!panel->begin()) {
        log_e("the display panel did not start");
    }
    panel->fillScreen(0x0000);

    screenFrame = allocateFrame();
    panelFrame = allocateFrame();

    lv_display_t* display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_user_data(display, this);
    lv_display_set_flush_cb(display, flush);
    // O AXS15231B corrompe a imagem com janelas parciais: o LVGL sempre renderiza e manda o quadro inteiro.
    lv_display_set_buffers(display, screenFrame, nullptr, FRAME_BYTES, LV_DISPLAY_RENDER_MODE_FULL);

    return display;
}

#ifdef CLAUDINHO_PROFILE_FLUSH
namespace {

// Medição do flush para achar onde o quadro gasta tempo: rotação, envio ao painel, e o resto (render do LVGL).
struct FlushProfile {
    int64_t windowStart = 0;
    int64_t previousFlushEnd = 0;
    int64_t rotateMicros = 0;
    int64_t drawMicros = 0;
    int64_t renderMicros = 0;
    uint32_t frames = 0;
};

FlushProfile profile;

void reportFlushProfile(int64_t rotateStart, int64_t drawStart, int64_t flushEnd)
{
    if (profile.previousFlushEnd != 0) {
        profile.renderMicros += rotateStart - profile.previousFlushEnd;
    }
    profile.rotateMicros += drawStart - rotateStart;
    profile.drawMicros += flushEnd - drawStart;
    profile.frames++;
    profile.previousFlushEnd = flushEnd;

    if (profile.windowStart == 0) {
        profile.windowStart = flushEnd;
    }
    if (flushEnd - profile.windowStart < 1000000) {
        return;
    }

    uint32_t frames = profile.frames;
    Serial.printf("flush: %u fps, rotate %.1f ms, draw %.1f ms, render+idle %.1f ms\n", frames,
        profile.rotateMicros / 1000.0 / frames, profile.drawMicros / 1000.0 / frames,
        profile.renderMicros / 1000.0 / frames);

    profile = FlushProfile{};
    profile.windowStart = flushEnd;
    profile.previousFlushEnd = flushEnd;
}

}
#endif

void Display::flush(lv_display_t* display, const lv_area_t*, uint8_t* pixels)
{
    Display* self = static_cast<Display*>(lv_display_get_user_data(display));

#ifdef CLAUDINHO_PROFILE_FLUSH
    int64_t rotateStart = esp_timer_get_time();
#endif
    self->rotation.rotate(reinterpret_cast<const uint16_t*>(pixels), self->panelFrame);
#ifdef CLAUDINHO_PROFILE_FLUSH
    int64_t drawStart = esp_timer_get_time();
#endif
    self->panel->draw16bitBeRGBBitmap(0, 0, self->panelFrame, PANEL_WIDTH, PANEL_HEIGHT);
#ifdef CLAUDINHO_PROFILE_FLUSH
    reportFlushProfile(rotateStart, drawStart, esp_timer_get_time());
#endif

    lv_display_flush_ready(display);
}
