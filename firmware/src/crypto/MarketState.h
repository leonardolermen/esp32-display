#pragma once

#include <optional>
#include <vector>

#include "crypto/Asset.h"
#include "support/LocalTime.h"

struct Quote {
    bool known = false;
    double priceBrl = 0;
    double changePercent = 0;
    std::vector<float> series;
};

struct MarketState {
    Quote quotes[ASSET_COUNT];
    std::optional<LocalTime> updatedAt;
    bool lastFetchFailed = false;
    bool everFetched = false;
};
