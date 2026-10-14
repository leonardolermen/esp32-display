#pragma once

#include "navigation/Page.h"
#include "spotify/device/SpotifyClient.h"

// Cápsula "tocando agora" no canto do Início. Some quando pausa ou não há aparelho.
class NowPlayingPill : public Page {
public:
    explicit NowPlayingPill(SpotifyClient& client);

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;
    void onTick(uint32_t nowMillis) override;

private:
    SpotifyClient& client;
    uint32_t seenState = 0;
    lv_obj_t* pill = nullptr;
    lv_obj_t* text = nullptr;
};
