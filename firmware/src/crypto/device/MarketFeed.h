#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "crypto/MarketState.h"
#include "support/Published.h"

// Busca as cotações numa tarefa própria (núcleo 0) e publica para a UI. Uma falha numa API não derruba as outras.
class MarketFeed {
public:
    void begin();
    const Published<MarketState>& state() const;

private:
    // Corpo de resposta na PSRAM: o CoinGecko manda ~20 KB, grande demais para a heap interna (regra do projeto).
    class Body {
    public:
        static std::unique_ptr<Body> allocate(size_t capacity);
        ~Body();

        char* data();
        size_t capacity() const;
        void setSize(size_t size);
        std::string_view view() const;

    private:
        Body(char* data, size_t capacity);

        char* bytes;
        size_t room;
        size_t used = 0;
    };

    static void run(void* self);
    void loop();
    void poll(uint32_t nowMillis, bool wifiConnected);
    std::unique_ptr<Body> fetch(const char* url);
    bool refreshCrypto();
    bool refreshDollar();
    void markOffline();

    Published<MarketState> published;
    MarketState current;
    uint32_t lastPollMillis = 0;
    uint32_t intervalMillis = 0;
    bool rateLimited = false;
};
