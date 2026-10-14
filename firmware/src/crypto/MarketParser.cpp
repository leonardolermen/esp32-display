#include "crypto/MarketParser.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdlib>

namespace {

constexpr size_t SPARKLINE_POINTS = 24;

// A resposta tem 168 pontos por moeda e campos que não usamos; o filtro deixa só o que a tela precisa.
JsonDocument coinGeckoFilter()
{
    JsonDocument filter;
    filter[0]["id"] = true;
    filter[0]["current_price"] = true;
    filter[0]["price_change_percentage_24h"] = true;
    filter[0]["sparkline_in_7d"]["price"] = true;
    return filter;
}

std::vector<float> lastPoints(JsonArrayConst prices, size_t count)
{
    std::vector<float> series;
    size_t skip = prices.size() > count ? prices.size() - count : 0;

    size_t index = 0;
    for (JsonVariantConst price : prices) {
        if (index++ >= skip) {
            series.push_back(price.as<float>());
        }
    }

    return series;
}

}

namespace MarketParser {

std::optional<Quote> coinGecko(std::string_view json, const char* coinId)
{
    JsonDocument filter = coinGeckoFilter();
    JsonDocument document;
    DeserializationError error = deserializeJson(document, json.data(), json.size(),
        DeserializationOption::Filter(filter));
    if (error != DeserializationError::Ok) {
        return std::nullopt;
    }

    for (JsonObjectConst coin : document.as<JsonArrayConst>()) {
        if (std::string(coin["id"] | "") != coinId) {
            continue;
        }

        Quote quote;
        quote.known = coin["current_price"].is<double>();
        quote.priceBrl = coin["current_price"] | 0.0;
        quote.changePercent = coin["price_change_percentage_24h"] | 0.0;
        quote.series = lastPoints(coin["sparkline_in_7d"]["price"].as<JsonArrayConst>(), SPARKLINE_POINTS);
        return quote;
    }

    return std::nullopt;
}

std::optional<double> dollarBid(std::string_view json)
{
    JsonDocument document;
    if (deserializeJson(document, json.data(), json.size()) != DeserializationError::Ok) {
        return std::nullopt;
    }

    const char* bid = document["USDBRL"]["bid"];
    if (bid == nullptr) {
        return std::nullopt;
    }

    return std::strtod(bid, nullptr);
}

std::vector<float> dollarSeries(std::string_view json)
{
    JsonDocument document;
    if (deserializeJson(document, json.data(), json.size()) != DeserializationError::Ok) {
        return {};
    }

    std::vector<float> series;
    for (JsonObjectConst day : document.as<JsonArrayConst>()) {
        const char* bid = day["bid"];
        if (bid != nullptr) {
            series.push_back(static_cast<float>(std::strtod(bid, nullptr)));
        }
    }

    // A API devolve do mais novo para o mais antigo; a sparkline lê da esquerda para a direita no tempo.
    std::reverse(series.begin(), series.end());
    return series;
}

Quote dollarQuote(double bid, std::vector<float> series)
{
    Quote quote;
    quote.known = true;
    quote.priceBrl = bid;
    quote.series = std::move(series);

    if (quote.series.size() >= 2 && quote.series.front() != 0.0f) {
        quote.changePercent = (quote.series.back() - quote.series.front()) / quote.series.front() * 100.0;
    }

    return quote;
}

}
