#pragma once

#include <lvgl.h>

#include "crypto/MarketState.h"
#include "navigation/Page.h"
#include "support/Published.h"

class CryptoPage : public Page {
public:
    explicit CryptoPage(const Published<MarketState>& state);

    void build(lv_obj_t* parent) override;
    void onTick(uint32_t nowMillis) override;

private:
    struct Row {
        lv_obj_t* panel = nullptr;
        lv_obj_t* price = nullptr;
        lv_obj_t* change = nullptr;
        lv_obj_t* chart = nullptr;
        lv_chart_series_t* line = nullptr;
    };

    void buildRow(lv_obj_t* parent, Asset asset, int32_t y);
    void render(const MarketState& market);
    void renderRow(Row& row, const Quote& quote);
    void renderFooter(const MarketState& market);

    const Published<MarketState>& state;
    uint32_t seenVersion = 0;
    Row rows[ASSET_COUNT];
    lv_obj_t* footer = nullptr;
};
