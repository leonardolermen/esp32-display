#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "crypto/MarketState.h"

namespace MarketParser {

std::optional<Quote> coinGecko(std::string_view json, const char* coinId);
std::optional<double> dollarBid(std::string_view json);
std::vector<float> dollarSeries(std::string_view json);
Quote dollarQuote(double bid, std::vector<float> series);

}
