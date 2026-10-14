#pragma once

#include <optional>
#include <string>
#include <vector>

#include "crypto/MarketState.h"

namespace MarketParser {

std::optional<Quote> coinGecko(const std::string& json, const char* coinId);
std::optional<double> dollarBid(const std::string& json);
std::vector<float> dollarSeries(const std::string& json);
Quote dollarQuote(double bid, std::vector<float> series);

}
