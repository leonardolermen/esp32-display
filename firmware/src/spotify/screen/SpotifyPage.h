#pragma once

#include <lvgl.h>

#include <string>

#include "navigation/Page.h"
#include "spotify/CoverArt.h"
#include "spotify/PlayerState.h"
#include "spotify/device/SpotifyClient.h"

class SpotifyPage : public Page {
public:
    explicit SpotifyPage(SpotifyClient& client);

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;
    void onTick(uint32_t nowMillis) override;

private:
    void buildCover(lv_obj_t* parent);
    void buildDetails(lv_obj_t* parent);
    void buildControls(lv_obj_t* parent);
    void buildNotice(lv_obj_t* parent);
    void render(const PlayerState& state, uint32_t nowMillis);
    void renderCover(CoverArt art);
    void renderProgress(uint32_t nowMillis);
    void renderNotice(const PlayerState& state);
    void fadeBackgroundTo(uint32_t color);

    static void onPrevious(lv_event_t* event);
    static void onPlayPause(lv_event_t* event);
    static void onNext(lv_event_t* event);
    static void onVolume(lv_event_t* event);
    static void fadeStep(void* page, int32_t mix);

    SpotifyClient& client;
    uint32_t seenState = 0;
    uint32_t seenCover = 0;
    PlayerState shown;
    CoverArt shownCover;
    lv_image_dsc_t coverDescriptor = {};
    uint32_t backgroundFrom = DominantColor::FALLBACK;
    uint32_t backgroundTo = DominantColor::FALLBACK;
    uint32_t currentBackground = DominantColor::FALLBACK;
    uint32_t lastProgressMillis = 0;
    uint32_t staleSinceMillis = 0;
    std::string shownLoginUrl;

    lv_obj_t* root = nullptr;
    lv_obj_t* cover = nullptr;
    lv_obj_t* coverPlaceholder = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* artists = nullptr;
    lv_obj_t* device = nullptr;
    lv_obj_t* progressBar = nullptr;
    lv_obj_t* elapsed = nullptr;
    lv_obj_t* total = nullptr;
    lv_obj_t* playPauseLabel = nullptr;
    lv_obj_t* volumeIcon = nullptr;
    lv_obj_t* volumeSlider = nullptr;
    lv_obj_t* notice = nullptr;
    lv_obj_t* noticeText = nullptr;
    lv_obj_t* qr = nullptr;
    lv_obj_t* staleLabel = nullptr;
};
