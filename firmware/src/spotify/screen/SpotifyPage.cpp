#include "spotify/screen/SpotifyPage.h"

// O lvgl.h do 9.4 não expõe o cache de imagens; precisamos dele para trocar a capa no mesmo descritor.
#include <src/misc/cache/instance/lv_image_cache.h>

#include "spotify/Progress.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t COVER_X = 20;
constexpr int32_t COVER_Y = 40;
constexpr int32_t COVER_RADIUS = 12;
constexpr int32_t DETAILS_X = 240;
constexpr int32_t DETAILS_WIDTH = 220;
constexpr int32_t CONTROL_SIZE = 56;
constexpr int32_t CONTROLS_Y = 182;
constexpr int32_t QR_SIZE = 140;
constexpr int32_t PROGRESS_SCALE = 1000;
constexpr uint32_t PROGRESS_EVERY_MILLIS = 250;
constexpr uint32_t FADE_MILLIS = 400;
constexpr uint32_t STALE_NOTICE_AFTER_MILLIS = 60000;

lv_obj_t* makeControl(lv_obj_t* parent, const char* symbol, int32_t x, lv_event_cb_t callback, void* page)
{
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, CONTROL_SIZE, CONTROL_SIZE);
    lv_obj_set_pos(button, x, CONTROLS_Y);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, theme::text(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, page);

    lv_obj_t* label = theme::label(button, fonts::title, theme::text());
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    return label;
}

SpotifyPage* pageOf(lv_event_t* event)
{
    return static_cast<SpotifyPage*>(lv_event_get_user_data(event));
}

}

SpotifyPage::SpotifyPage(SpotifyClient& client)
    : client(client)
{
}

void SpotifyPage::build(lv_obj_t* parent)
{
    root = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(DominantColor::FALLBACK), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    buildCover(parent);
    buildDetails(parent);
    buildControls(parent);
    buildNotice(parent);

    staleLabel = theme::label(parent, fonts::small, theme::textSecondary());
    lv_label_set_text(staleLabel, "sem conexão");
    lv_obj_align(staleLabel, LV_ALIGN_TOP_RIGHT, -12, 10);
    lv_obj_add_flag(staleLabel, LV_OBJ_FLAG_HIDDEN);
}

void SpotifyPage::buildCover(lv_obj_t* parent)
{
    coverPlaceholder = lv_obj_create(parent);
    lv_obj_remove_style_all(coverPlaceholder);
    lv_obj_set_size(coverPlaceholder, COVER_SIZE, COVER_SIZE);
    lv_obj_set_pos(coverPlaceholder, COVER_X, COVER_Y);
    lv_obj_set_style_radius(coverPlaceholder, COVER_RADIUS, 0);
    lv_obj_set_style_bg_color(coverPlaceholder, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(coverPlaceholder, LV_OPA_30, 0);
    lv_obj_add_flag(coverPlaceholder, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* note = theme::label(coverPlaceholder, fonts::title, theme::textSecondary());
    lv_label_set_text(note, LV_SYMBOL_AUDIO);
    lv_obj_center(note);

    cover = lv_image_create(parent);
    lv_obj_set_size(cover, COVER_SIZE, COVER_SIZE);
    lv_obj_set_pos(cover, COVER_X, COVER_Y);
    lv_obj_set_style_radius(cover, COVER_RADIUS, 0);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_HIDDEN);
}

void SpotifyPage::buildDetails(lv_obj_t* parent)
{
    title = theme::label(parent, fonts::large, theme::text());
    lv_obj_set_size(title, DETAILS_WIDTH, 48);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(title, DETAILS_X, COVER_Y);

    artists = theme::label(parent, fonts::medium, theme::text());
    lv_obj_set_size(artists, DETAILS_WIDTH, 20);
    lv_label_set_long_mode(artists, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(artists, DETAILS_X, COVER_Y + 52);

    device = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_set_size(device, DETAILS_WIDTH, 16);
    lv_label_set_long_mode(device, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(device, DETAILS_X, COVER_Y + 76);

    progressBar = lv_bar_create(parent);
    lv_obj_set_size(progressBar, DETAILS_WIDTH, 4);
    lv_obj_set_pos(progressBar, DETAILS_X, COVER_Y + 106);
    lv_bar_set_range(progressBar, 0, PROGRESS_SCALE);
    lv_obj_set_style_bg_color(progressBar, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progressBar, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar, theme::text(), LV_PART_INDICATOR);

    elapsed = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_align_to(elapsed, progressBar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    total = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_align_to(total, progressBar, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);
}

void SpotifyPage::buildControls(lv_obj_t* parent)
{
    makeControl(parent, LV_SYMBOL_PREV, DETAILS_X + 4, onPrevious, this);
    playPauseLabel = makeControl(parent, LV_SYMBOL_PLAY, DETAILS_X + 82, onPlayPause, this);
    makeControl(parent, LV_SYMBOL_NEXT, DETAILS_X + 160, onNext, this);

    volumeIcon = theme::label(parent, fonts::medium, theme::textSecondary());
    lv_label_set_text(volumeIcon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_pos(volumeIcon, DETAILS_X, CONTROLS_Y + 76);

    volumeSlider = lv_slider_create(parent);
    lv_obj_set_size(volumeSlider, DETAILS_WIDTH - 36, 6);
    lv_obj_set_pos(volumeSlider, DETAILS_X + 30, CONTROLS_Y + 82);
    lv_slider_set_range(volumeSlider, 0, 100);
    lv_obj_set_style_bg_color(volumeSlider, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volumeSlider, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(volumeSlider, theme::text(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volumeSlider, theme::text(), LV_PART_KNOB);
    lv_obj_add_event_cb(volumeSlider, onVolume, LV_EVENT_VALUE_CHANGED, this);
}

void SpotifyPage::buildNotice(lv_obj_t* parent)
{
    notice = lv_obj_create(parent);
    lv_obj_remove_style_all(notice);
    lv_obj_set_size(notice, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(notice, lv_color_hex(DominantColor::FALLBACK), 0);
    lv_obj_set_style_bg_opa(notice, LV_OPA_COVER, 0);
    lv_obj_add_flag(notice, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(notice, LV_OBJ_FLAG_SCROLLABLE);

    qr = lv_qrcode_create(notice);
    lv_qrcode_set_size(qr, QR_SIZE);
    lv_qrcode_set_dark_color(qr, theme::background());
    lv_qrcode_set_light_color(qr, theme::text());
    lv_obj_set_style_border_width(qr, 6, 0);
    lv_obj_set_style_border_color(qr, theme::text(), 0);
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 40, -10);

    noticeText = theme::label(notice, fonts::large, theme::text());
    lv_obj_set_width(noticeText, DETAILS_WIDTH);
    lv_label_set_long_mode(noticeText, LV_LABEL_LONG_WRAP);

    lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
}

void SpotifyPage::onShow()
{
    client.watch(true);
}

void SpotifyPage::onHide()
{
    client.watch(false);
}

void SpotifyPage::onTick(uint32_t nowMillis)
{
    PlayerState state;
    if (client.state().readIfChanged(state, seenState)) {
        render(state, nowMillis);
    }

    CoverArt art;
    if (client.cover().readIfChanged(art, seenCover)) {
        renderCover(std::move(art));
    }

    if (nowMillis - lastProgressMillis >= PROGRESS_EVERY_MILLIS) {
        lastProgressMillis = nowMillis;
        renderProgress(nowMillis);
    }
}

void SpotifyPage::render(const PlayerState& state, uint32_t nowMillis)
{
    if (state.stale && !shown.stale) {
        staleSinceMillis = nowMillis;
    }
    shown = state;
    renderNotice(state);

    bool somethingPlays = state.status == PlayerStatus::Playing || state.status == PlayerStatus::Paused;
    std::string titleText = state.hasItem ? state.track.title : (somethingPlays ? "Tocando algo" : "");
    lv_label_set_text(title, titleText.c_str());
    lv_label_set_text(artists, state.hasItem ? state.track.artists.c_str() : "");
    lv_label_set_text(device, state.device.c_str());
    lv_label_set_text(total, Progress::clock(state.track.durationMs).c_str());
    lv_label_set_text(playPauseLabel, state.status == PlayerStatus::Playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    bool volumeShown = state.volume.has_value() && state.volumeAllowed;
    lv_obj_set_flag(volumeSlider, LV_OBJ_FLAG_HIDDEN, !volumeShown);
    lv_obj_set_flag(volumeIcon, LV_OBJ_FLAG_HIDDEN, !volumeShown);
    if (volumeShown && !lv_obj_has_state(volumeSlider, LV_STATE_PRESSED)) {
        lv_slider_set_value(volumeSlider, *state.volume, LV_ANIM_OFF);
    }

    renderProgress(nowMillis);
}

void SpotifyPage::renderCover(CoverArt art)
{
    shownCover = std::move(art);
    bool hasPixels = shownCover.pixels.size() == static_cast<size_t>(COVER_SIZE * COVER_SIZE);
    lv_obj_set_flag(cover, LV_OBJ_FLAG_HIDDEN, !hasPixels);
    lv_obj_set_flag(coverPlaceholder, LV_OBJ_FLAG_HIDDEN, hasPixels);

    if (hasPixels) {
        coverDescriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        coverDescriptor.header.cf = LV_COLOR_FORMAT_RGB565;
        coverDescriptor.header.w = COVER_SIZE;
        coverDescriptor.header.h = COVER_SIZE;
        coverDescriptor.header.stride = COVER_SIZE * 2;
        coverDescriptor.data_size = COVER_SIZE * COVER_SIZE * 2;
        coverDescriptor.data = reinterpret_cast<const uint8_t*>(shownCover.pixels.data());
        // O descritor é o mesmo a cada capa; sem tirar do cache o LVGL desenharia a capa anterior.
        lv_image_cache_drop(&coverDescriptor);
        lv_image_set_src(cover, &coverDescriptor);
        lv_obj_invalidate(cover);
    }

    fadeBackgroundTo(hasPixels ? shownCover.color : DominantColor::FALLBACK);
}

void SpotifyPage::renderProgress(uint32_t nowMillis)
{
    uint32_t position = Progress::estimate(shown, nowMillis);
    uint32_t duration = shown.track.durationMs;
    int32_t scaled = duration > 0 ? static_cast<int32_t>(uint64_t(position) * PROGRESS_SCALE / duration) : 0;
    lv_bar_set_value(progressBar, scaled, LV_ANIM_OFF);
    lv_label_set_text(elapsed, Progress::clock(position).c_str());

    bool staleLong = shown.stale && nowMillis - staleSinceMillis > STALE_NOTICE_AFTER_MILLIS;
    lv_obj_set_flag(staleLabel, LV_OBJ_FLAG_HIDDEN, !staleLong);
}

void SpotifyPage::renderNotice(const PlayerState& state)
{
    bool disconnected = state.status == PlayerStatus::Disconnected;
    bool noDevice = state.status == PlayerStatus::NoDevice;
    lv_obj_set_flag(notice, LV_OBJ_FLAG_HIDDEN, !disconnected && !noDevice);
    lv_obj_set_flag(qr, LV_OBJ_FLAG_HIDDEN, !disconnected);
    lv_obj_move_foreground(notice);

    if (disconnected) {
        lv_label_set_text(noticeText, "Escaneie para conectar o Spotify");
        lv_obj_align(noticeText, LV_ALIGN_RIGHT_MID, -20, -10);
        if (!state.loginUrl.empty() && state.loginUrl != shownLoginUrl) {
            shownLoginUrl = state.loginUrl;
            lv_qrcode_update(qr, shownLoginUrl.c_str(), shownLoginUrl.size());
        }
        return;
    }

    lv_label_set_text(noticeText, LV_SYMBOL_AUDIO "  Abra o Spotify em algum aparelho");
    lv_obj_align(noticeText, LV_ALIGN_CENTER, 0, -10);
}

void SpotifyPage::fadeBackgroundTo(uint32_t color)
{
    backgroundFrom = currentBackground;
    backgroundTo = color;

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, this);
    lv_anim_set_values(&fade, 0, 255);
    lv_anim_set_duration(&fade, FADE_MILLIS);
    lv_anim_set_exec_cb(&fade, fadeStep);
    lv_anim_start(&fade);
}

void SpotifyPage::fadeStep(void* page, int32_t mix)
{
    SpotifyPage* self = static_cast<SpotifyPage*>(page);
    lv_color_t color = lv_color_mix(lv_color_hex(self->backgroundTo), lv_color_hex(self->backgroundFrom),
        static_cast<uint8_t>(mix));

    lv_obj_set_style_bg_color(self->root, color, 0);
    self->currentBackground = lv_color_to_u32(color) & 0xFFFFFF;
}

void SpotifyPage::onPrevious(lv_event_t* event)
{
    pageOf(event)->client.send(Command{CommandKind::Previous});
}

void SpotifyPage::onPlayPause(lv_event_t* event)
{
    SpotifyPage* self = pageOf(event);
    bool playing = self->shown.status == PlayerStatus::Playing;
    self->client.send(Command{playing ? CommandKind::Pause : CommandKind::Play});
}

void SpotifyPage::onNext(lv_event_t* event)
{
    pageOf(event)->client.send(Command{CommandKind::Next});
}

void SpotifyPage::onVolume(lv_event_t* event)
{
    lv_obj_t* slider = lv_event_get_target_obj(event);
    uint8_t volume = static_cast<uint8_t>(lv_slider_get_value(slider));
    pageOf(event)->client.send(Command{CommandKind::Volume, volume});
}
