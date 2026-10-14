#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "crypto/MarketState.h"
#include "support/Published.h"

// Busca as cotações na tarefa de rede e publica para a UI. Uma falha numa API não derruba as outras nem a tarefa.
class MarketFeed {
public:
    void poll(uint32_t nowMillis, bool wifiConnected);
    const Published<MarketState>& state() const;

private:
    std::optional<std::string> fetch(const char* url);
    bool refreshCrypto();
    bool refreshDollar();

    Published<MarketState> published;
    MarketState current;
    uint32_t lastPollMillis = 0;
    uint32_t intervalMillis = 0;
    bool rateLimited = false;
};
