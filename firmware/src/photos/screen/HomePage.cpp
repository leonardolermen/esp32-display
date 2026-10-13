#include "photos/screen/HomePage.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "board/ScreenGeometry.h"
#include "support/LocalTime.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr uint32_t SWAP_EVERY_MILLIS = 12000;
constexpr uint32_t FADE_MILLIS = 600;
constexpr uint32_t CLOCK_EVERY_MILLIS = 1000;
constexpr int32_t BAND_HEIGHT = 78;

}

HomePage::HomePage(PhotoStore& store)
    : store(store)
    , view(store)
{
}

lv_image_dsc_t HomePage::makeFrame()
{
    lv_image_dsc_t frame = {};
    frame.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame.header.cf = LV_COLOR_FORMAT_RGB565;
    frame.header.w = SCREEN_WIDTH;
    frame.header.h = SCREEN_HEIGHT;
    frame.header.stride = SCREEN_WIDTH * 2;
    frame.data_size = SCREEN_WIDTH * SCREEN_HEIGHT * 2;
    frame.data = static_cast<uint8_t*>(heap_caps_malloc(frame.data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return frame;
}

void HomePage::build(lv_obj_t* parent)
{
    lv_obj_set_style_bg_color(parent, theme::background(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    for (size_t slot = 0; slot < 2; slot++) {
        frames[slot] = makeFrame();
        images[slot] = lv_image_create(parent);
        lv_image_set_src(images[slot], &frames[slot]);
        lv_obj_set_style_opa(images[slot], LV_OPA_TRANSP, 0);
    }

    invite = theme::label(parent, fonts::large, theme::textSecondary());
    lv_label_set_text(invite, "Mande fotos em claudinho.local");
    lv_obj_center(invite);

    band = lv_obj_create(parent);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, SCREEN_WIDTH, BAND_HEIGHT);
    lv_obj_align(band, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(band, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_50, 0);

    clockLabel = theme::label(band, fonts::clock_, theme::text());
    lv_obj_align(clockLabel, LV_ALIGN_LEFT_MID, 16, -8);
    dateLabel = theme::label(band, fonts::small, theme::textSecondary());
    lv_obj_align(dateLabel, LV_ALIGN_BOTTOM_LEFT, 16, -10);
    lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
}

void HomePage::onShow()
{
    reloadAlbum();
}

void HomePage::reloadAlbum()
{
    names = store.list();
    order.emplace(names.size(), static_cast<uint32_t>(millis()));

    lastSwapMillis = 0;
}

// A decodificação (~150 ms) acontece aqui e segura a UI por esse tempo a cada 12 s; aceito nesta versão.
void HomePage::onTick(uint32_t nowMillis)
{
    uint32_t albumVersion = 0;
    if (store.albumVersion().readIfChanged(albumVersion, seenAlbumVersion)) {
        reloadAlbum();
    }

    if (nowMillis - lastSwapMillis >= SWAP_EVERY_MILLIS) {
        showNextPhoto(nowMillis);
    }

    if (nowMillis - lastClockMillis >= CLOCK_EVERY_MILLIS) {
        lastClockMillis = nowMillis;
        refreshClock();
    }
}

void HomePage::showNextPhoto(uint32_t nowMillis)
{
    lastSwapMillis = nowMillis;

    std::optional<size_t> pick = order->next();
    bool hasPhotos = pick.has_value();
    lv_obj_set_flag(invite, LV_OBJ_FLAG_HIDDEN, hasPhotos);
    if (!hasPhotos) {
        return;
    }

    size_t incoming = 1 - visible;
    if (frames[incoming].data == nullptr) {
        return;
    }

    if (!view.load(names[*pick], frames[incoming])) {
        // Foto apagada ou corrompida: a próxima volta do loop já tenta a seguinte, sem esperar os 12 s.
        lastSwapMillis = nowMillis - SWAP_EVERY_MILLIS;
        return;
    }

    lv_image_set_src(images[incoming], &frames[incoming]);
    lv_obj_move_foreground(images[incoming]);
    lv_obj_move_foreground(band);
    lv_obj_fade_in(images[incoming], FADE_MILLIS, 0);
    lv_obj_fade_out(images[visible], FADE_MILLIS, 0);
    visible = incoming;
}

void HomePage::refreshClock()
{
    std::optional<LocalTime> now = LocalTime::now();
    lv_obj_set_flag(band, LV_OBJ_FLAG_HIDDEN, !now.has_value());
    if (!now) {
        return;
    }

    lv_label_set_text(clockLabel, now->hourMinute().c_str());
    lv_label_set_text(dateLabel, now->longDate().c_str());
}
