#include "crypto/device/MarketFeed.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include <algorithm>

#include "crypto/MarketParser.h"

namespace {

constexpr uint32_t NORMAL_INTERVAL_MILLIS = 60000;
constexpr uint32_t MAX_BACKOFF_MILLIS = 600000;
constexpr uint32_t LOOP_SLEEP_MILLIS = 1000;
constexpr uint16_t TIMEOUT_MILLIS = 5000;
constexpr int TOO_MANY_REQUESTS = 429;
constexpr size_t MAX_BODY_BYTES = 64 * 1024;
constexpr size_t READ_CHUNK_BYTES = 1024;
// mbedTLS precisa de pilha folgada para o handshake; 12 KB é o que o core recomenda para HTTPS numa tarefa.
constexpr uint32_t TASK_STACK_BYTES = 12288;

constexpr const char* COINGECKO_URL = "https://api.coingecko.com/api/v3/coins/markets?vs_currency=brl"
    "&ids=bitcoin,ethereum&sparkline=true&price_change_percentage=24h";
constexpr const char* DOLLAR_LAST_URL = "https://economia.awesomeapi.com.br/json/last/USD-BRL";
constexpr const char* DOLLAR_DAILY_URL = "https://economia.awesomeapi.com.br/json/daily/USD-BRL/30";

}

std::unique_ptr<MarketFeed::Body> MarketFeed::Body::allocate(size_t capacity)
{
    char* bytes = static_cast<char*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (bytes == nullptr) {
        return nullptr;
    }

    return std::unique_ptr<Body>(new Body(bytes, capacity));
}

MarketFeed::Body::Body(char* data, size_t capacity)
    : bytes(data)
    , room(capacity)
{
}

MarketFeed::Body::~Body()
{
    heap_caps_free(bytes);
}

char* MarketFeed::Body::data()
{
    return bytes;
}

size_t MarketFeed::Body::capacity() const
{
    return room;
}

void MarketFeed::Body::setSize(size_t size)
{
    used = size;
}

std::string_view MarketFeed::Body::view() const
{
    return std::string_view(bytes, used);
}

// Tarefa própria: três chamadas HTTPS podem levar dezenas de segundos com rede ruim, e a tarefa de rede precisa
// continuar atendendo o Wi-Fi, o portal e o mDNS nesse meio-tempo.
void MarketFeed::begin()
{
    xTaskCreatePinnedToCore(run, "market", TASK_STACK_BYTES, this, 1, nullptr, 0);
}

void MarketFeed::run(void* self)
{
    static_cast<MarketFeed*>(self)->loop();
}

void MarketFeed::loop()
{
    for (;;) {
        poll(millis(), WiFi.status() == WL_CONNECTED);
        vTaskDelay(pdMS_TO_TICKS(LOOP_SLEEP_MILLIS));
    }
}

const Published<MarketState>& MarketFeed::state() const
{
    return published;
}

void MarketFeed::poll(uint32_t nowMillis, bool wifiConnected)
{
    if (nowMillis - lastPollMillis < intervalMillis) {
        return;
    }

    lastPollMillis = nowMillis;
    if (!wifiConnected) {
        markOffline();
        return;
    }

    rateLimited = false;
    bool cryptoOk = refreshCrypto();
    bool dollarOk = refreshDollar();

    current.everFetched = current.everFetched || cryptoOk || dollarOk;
    current.lastFetchFailed = !(cryptoOk && dollarOk);
    if (cryptoOk || dollarOk) {
        current.updatedAt = LocalTime::now();
    }
    published.publish(current);

    // 429: a API pediu calma; dobra o intervalo até 10 min e volta ao normal na próxima resposta boa.
    uint32_t doubled = std::max(intervalMillis, NORMAL_INTERVAL_MILLIS) * 2;
    intervalMillis = rateLimited ? std::min(doubled, MAX_BACKOFF_MILLIS) : NORMAL_INTERVAL_MILLIS;
}

// Sem Wi-Fi os valores ficam na tela, mas o rodapé tem que avisar que são velhos ("visto às").
void MarketFeed::markOffline()
{
    if (current.lastFetchFailed) {
        return;
    }

    current.lastFetchFailed = true;
    published.publish(current);
}

std::unique_ptr<MarketFeed::Body> MarketFeed::fetch(const char* url)
{
    NetworkClientSecure client;
    // Certificados raiz embutidos no core: a conexão é verificada de verdade, sem setInsecure.
    client.useBuiltinCACertBundle();
    client.setTimeout(TIMEOUT_MILLIS);

    HTTPClient http;
    http.setTimeout(TIMEOUT_MILLIS);
    http.setConnectTimeout(TIMEOUT_MILLIS);
    if (!http.begin(client, url)) {
        return nullptr;
    }

    int status = http.GET();
    if (status == TOO_MANY_REQUESTS) {
        rateLimited = true;
    }
    if (status != HTTP_CODE_OK) {
        http.end();
        return nullptr;
    }

    // Lido em pedaços direto para a PSRAM: getString() copiaria o corpo inteiro para a heap interna.
    std::unique_ptr<Body> body = Body::allocate(MAX_BODY_BYTES);
    size_t received = 0;
    if (body) {
        NetworkClient* stream = http.getStreamPtr();
        uint32_t deadline = millis() + TIMEOUT_MILLIS;
        while (http.connected() && received < body->capacity() && millis() < deadline) {
            int got = stream->read(reinterpret_cast<uint8_t*>(body->data() + received),
                std::min(READ_CHUNK_BYTES, body->capacity() - received));
            if (got > 0) {
                received += static_cast<size_t>(got);
                deadline = millis() + TIMEOUT_MILLIS;
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        body->setSize(received);
    }
    http.end();

    return received > 0 ? std::move(body) : nullptr;
}

bool MarketFeed::refreshCrypto()
{
    std::unique_ptr<Body> body = fetch(COINGECKO_URL);
    if (!body) {
        return false;
    }

    std::optional<Quote> bitcoin = MarketParser::coinGecko(body->view(), "bitcoin");
    std::optional<Quote> ethereum = MarketParser::coinGecko(body->view(), "ethereum");
    if (bitcoin) {
        current.quotes[static_cast<size_t>(Asset::Bitcoin)] = *bitcoin;
    }
    if (ethereum) {
        current.quotes[static_cast<size_t>(Asset::Ethereum)] = *ethereum;
    }

    return bitcoin.has_value() && ethereum.has_value();
}

bool MarketFeed::refreshDollar()
{
    std::unique_ptr<Body> last = fetch(DOLLAR_LAST_URL);
    if (!last) {
        return false;
    }

    std::optional<double> bid = MarketParser::dollarBid(last->view());
    if (!bid) {
        return false;
    }

    std::unique_ptr<Body> daily = fetch(DOLLAR_DAILY_URL);
    std::vector<float> series = daily ? MarketParser::dollarSeries(daily->view()) : std::vector<float>();
    current.quotes[static_cast<size_t>(Asset::Dollar)] = MarketParser::dollarQuote(*bid, std::move(series));
    return daily != nullptr;
}
