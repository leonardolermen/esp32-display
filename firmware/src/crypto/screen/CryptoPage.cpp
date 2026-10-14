#include "crypto/screen/CryptoPage.h"

#include <algorithm>
#include <vector>

#include "board/ScreenGeometry.h"
#include "crypto/PriceFormat.h"
#include "crypto/Sparkline.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t MARGIN = 12;
constexpr int32_t ROW_HEIGHT = 86;
constexpr int32_t ROW_GAP = 6;
constexpr int32_t CHART_HEIGHT = 22;
constexpr size_t CHART_POINTS = 30;
constexpr uint32_t UP_COLOR = 0x9FD49A;
constexpr uint32_t DOWN_COLOR = 0xE24B4A;

lv_color_t changeColor(double changePercent)
{
    return lv_color_hex(changePercent < 0 ? DOWN_COLOR : UP_COLOR);
}

}

CryptoPage::CryptoPage(const Published<MarketState>& state)
    : state(state)
{
}

void CryptoPage::build(lv_obj_t* parent)
{
    lv_obj_set_style_bg_color(parent, theme::background(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    for (size_t index = 0; index < ASSET_COUNT; index++) {
        buildRow(parent, static_cast<Asset>(index), MARGIN + static_cast<int32_t>(index) * (ROW_HEIGHT + ROW_GAP));
    }

    footer = theme::label(parent, fonts::small, theme::textSecondary());
    lv_label_set_text(footer, "buscando…");
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, MARGIN, -16);
}

void CryptoPage::buildRow(lv_obj_t* parent, Asset asset, int32_t y)
{
    Row& row = rows[static_cast<size_t>(asset)];
    const AssetInfo& info = assetInfo(asset);

    row.panel = theme::panel(parent);
    lv_obj_set_size(row.panel, SCREEN_WIDTH - 2 * MARGIN, ROW_HEIGHT);
    lv_obj_set_pos(row.panel, MARGIN, y);
    lv_obj_set_style_pad_all(row.panel, 10, 0);

    lv_obj_t* symbol = theme::label(row.panel, fonts::large, theme::text());
    lv_label_set_text(symbol, info.symbol);
    lv_obj_align(symbol, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* name = theme::label(row.panel, fonts::small, theme::textSecondary());
    lv_label_set_text(name, info.name);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 24);

    row.price = theme::label(row.panel, fonts::title, theme::text());
    lv_label_set_text(row.price, "—");
    lv_obj_align(row.price, LV_ALIGN_TOP_LEFT, 110, 2);

    row.change = theme::label(row.panel, fonts::medium, theme::textSecondary());
    lv_label_set_text(row.change, "");
    lv_obj_align(row.change, LV_ALIGN_TOP_RIGHT, 0, 4);

    lv_obj_t* period = theme::label(row.panel, fonts::small, theme::textSecondary());
    lv_label_set_text(period, info.period);
    lv_obj_align(period, LV_ALIGN_TOP_RIGHT, 0, 24);

    row.chart = lv_chart_create(row.panel);
    lv_obj_remove_style_all(row.chart);
    lv_obj_set_size(row.chart, SCREEN_WIDTH - 2 * MARGIN - 20, CHART_HEIGHT);
    lv_obj_align(row.chart, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_chart_set_type(row.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(row.chart, 0, 0);
    lv_chart_set_point_count(row.chart, CHART_POINTS);
    lv_obj_set_style_size(row.chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(row.chart, 1, LV_PART_ITEMS);
    row.line = lv_chart_add_series(row.chart, lv_color_hex(UP_COLOR), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
}

void CryptoPage::onTick(uint32_t)
{
    MarketState market;
    if (state.readIfChanged(market, seenVersion)) {
        render(market);
    }
}

void CryptoPage::render(const MarketState& market)
{
    for (size_t index = 0; index < ASSET_COUNT; index++) {
        renderRow(rows[index], market.quotes[index]);
    }

    renderFooter(market);
}

void CryptoPage::renderRow(Row& row, const Quote& quote)
{
    if (!quote.known) {
        lv_label_set_text(row.price, "—");
        lv_label_set_text(row.change, "");
        lv_obj_add_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(row.price, PriceFormat::reais(quote.priceBrl).c_str());
    lv_label_set_text(row.change, PriceFormat::percent(quote.changePercent).c_str());
    lv_obj_set_style_text_color(row.change, changeColor(quote.changePercent), 0);

    std::vector<int32_t> points = Sparkline::toChart(quote.series, CHART_POINTS);
    if (points.size() < 2) {
        lv_obj_add_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int32_t low = *std::min_element(points.begin(), points.end());
    int32_t high = *std::max_element(points.begin(), points.end());
    lv_chart_set_point_count(row.chart, static_cast<uint32_t>(points.size()));
    lv_chart_set_axis_range(row.chart, LV_CHART_AXIS_PRIMARY_Y, low, high == low ? low + 1 : high);
    lv_chart_set_series_color(row.chart, row.line, changeColor(quote.changePercent));
    for (size_t index = 0; index < points.size(); index++) {
        lv_chart_set_value_by_id(row.chart, row.line, static_cast<uint32_t>(index), points[index]);
    }
    lv_chart_refresh(row.chart);
    lv_obj_remove_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
}

void CryptoPage::renderFooter(const MarketState& market)
{
    if (!market.everFetched) {
        lv_label_set_text(footer, market.lastFetchFailed ? "sem resposta ainda" : "buscando…");
        return;
    }

    std::string when = market.updatedAt ? " " + market.updatedAt->hourMinute() : "";
    std::string text = market.lastFetchFailed ? "visto às" + when : "atualizado" + when;
    lv_label_set_text(footer, text.c_str());
}
