#pragma once

#include <cstdint>
#include <memory>

#include "crypto/MarketState.h"
#include "support/Published.h"
#include "support/device/Https.h"

// Busca as cotações numa tarefa própria (núcleo 0) e publica para a UI. Uma falha numa API não derruba as outras.
class MarketFeed {
public:
    void begin();
    const Published<MarketState>& state() const;

private:
    static void run(void* self);
    void loop();
    void poll(uint32_t nowMillis, bool wifiConnected);
    std::unique_ptr<PsramBuffer> fetch(const char* url);
    bool refreshCrypto();
    bool refreshDollar();
    void markOffline();

    Published<MarketState> published;
    MarketState current;
    uint32_t lastPollMillis = 0;
    uint32_t intervalMillis = 0;
    bool rateLimited = false;
};
