#include "spotify/screen/NowPlayingPill.h"

#include <algorithm>
#include <string>

#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t PILL_HEIGHT = 28;
constexpr int32_t MAX_TEXT_WIDTH = 220;
constexpr int32_t MARGIN = 12;

}

NowPlayingPill::NowPlayingPill(SpotifyClient& client)
    : client(client)
{
}

void NowPlayingPill::build(lv_obj_t* parent)
{
    pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, PILL_HEIGHT);
    lv_obj_set_style_bg_color(pill, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_50, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, MARGIN, 0);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, -MARGIN, MARGIN);
    lv_obj_add_flag(pill, LV_OBJ_FLAG_HIDDEN);

    text = theme::label(pill, fonts::small, theme::text());
    lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
    lv_obj_align(text, LV_ALIGN_LEFT_MID, 0, 0);
}

void NowPlayingPill::onShow()
{
    client.watch(true);
}

void NowPlayingPill::onHide()
{
    client.watch(false);
}

void NowPlayingPill::onTick(uint32_t)
{
    PlayerState state;
    if (!client.state().readIfChanged(state, seenState)) {
        return;
    }

    bool showing = state.status == PlayerStatus::Playing && state.hasItem && !state.track.title.empty();
    lv_obj_set_flag(pill, LV_OBJ_FLAG_HIDDEN, !showing);
    if (!showing) {
        return;
    }

    std::string line = std::string(LV_SYMBOL_AUDIO) + "  " + state.track.title;
    if (!state.track.artists.empty()) {
        line += " — " + state.track.artists;
    }
    // Largura medida e uma linha de altura: com tamanho automático e largura máxima o LVGL quebra o texto em várias
    // linhas e a cápsula mostra um pedaço do meio, sem reticências.
    lv_point_t size;
    lv_text_get_size(&size, line.c_str(), fonts::small, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_size(text, std::min<int32_t>(size.x, MAX_TEXT_WIDTH), lv_font_get_line_height(fonts::small));
    lv_label_set_text(text, line.c_str());
}
