#pragma once

#include <Stream.h>

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
    // Corpo de resposta na PSRAM: o CoinGecko manda vários KB, grande demais para a heap interna (regra do projeto).
    // É um Stream para o HTTPClient escrever nele já sem a codificação chunked.
    class Body : public Stream {
    public:
        static std::unique_ptr<Body> allocate(size_t capacity);
        ~Body() override;

        std::string_view view() const;

        size_t write(uint8_t byte) override;
        size_t write(const uint8_t* buffer, size_t size) override;
        int available() override;
        int read() override;
        int peek() override;

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
