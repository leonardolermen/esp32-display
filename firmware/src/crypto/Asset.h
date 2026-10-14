#pragma once

#include <cstddef>

enum class Asset {
    Bitcoin,
    Ethereum,
    Dollar,
};

constexpr size_t ASSET_COUNT = 3;

struct AssetInfo {
    const char* symbol;
    const char* name;
    const char* period;
};

inline const AssetInfo& assetInfo(Asset asset)
{
    static const AssetInfo INFOS[ASSET_COUNT] = {
        {"BTC", "Bitcoin", "24 h"},
        {"ETH", "Ethereum", "24 h"},
        {"USD", "Dólar", "30 d"},
    };

    return INFOS[static_cast<size_t>(asset)];
}
