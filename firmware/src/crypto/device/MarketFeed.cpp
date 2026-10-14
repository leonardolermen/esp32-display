#include "crypto/device/MarketFeed.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <algorithm>

#include "crypto/MarketParser.h"

namespace {

constexpr uint32_t NORMAL_INTERVAL_MILLIS = 60000;
constexpr uint32_t MAX_BACKOFF_MILLIS = 600000;
constexpr uint16_t TIMEOUT_MILLIS = 5000;
constexpr int TOO_MANY_REQUESTS = 429;

constexpr const char* COINGECKO_URL = "https://api.coingecko.com/api/v3/coins/markets?vs_currency=brl"
    "&ids=bitcoin,ethereum&sparkline=true&price_change_percentage=24h";
constexpr const char* DOLLAR_LAST_URL = "https://economia.awesomeapi.com.br/json/last/USD-BRL";
constexpr const char* DOLLAR_DAILY_URL = "https://economia.awesomeapi.com.br/json/daily/USD-BRL/30";

}

void MarketFeed::poll(uint32_t nowMillis, bool wifiConnected)
{
    if (!wifiConnected || nowMillis - lastPollMillis < intervalMillis) {
        return;
    }

    lastPollMillis = nowMillis;
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
    intervalMillis = rateLimited ? std::min(std::max(intervalMillis, NORMAL_INTERVAL_MILLIS) * 2, MAX_BACKOFF_MILLIS)
        : NORMAL_INTERVAL_MILLIS;
}

const Published<MarketState>& MarketFeed::state() const
{
    return published;
}

std::optional<std::string> MarketFeed::fetch(const char* url)
{
    NetworkClientSecure client;
    // Certificados raiz embutidos no core: a conexão é verificada de verdade, sem setInsecure.
    client.useBuiltinCACertBundle();
    client.setTimeout(TIMEOUT_MILLIS);

    HTTPClient http;
    http.setTimeout(TIMEOUT_MILLIS);
    http.setConnectTimeout(TIMEOUT_MILLIS);
    if (!http.begin(client, url)) {
        return std::nullopt;
    }

    int status = http.GET();
    std::optional<std::string> body;
    if (status == HTTP_CODE_OK) {
        body = http.getString().c_str();
    }
    if (status == TOO_MANY_REQUESTS) {
        rateLimited = true;
    }
    http.end();

    return body;
}

bool MarketFeed::refreshCrypto()
{
    std::optional<std::string> body = fetch(COINGECKO_URL);
    if (!body) {
        return false;
    }

    std::optional<Quote> bitcoin = MarketParser::coinGecko(*body, "bitcoin");
    std::optional<Quote> ethereum = MarketParser::coinGecko(*body, "ethereum");
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
    std::optional<std::string> last = fetch(DOLLAR_LAST_URL);
    std::optional<std::string> daily = fetch(DOLLAR_DAILY_URL);
    if (!last || !daily) {
        return false;
    }

    std::optional<double> bid = MarketParser::dollarBid(*last);
    if (!bid) {
        return false;
    }

    std::vector<float> series = MarketParser::dollarSeries(*daily);
    current.quotes[static_cast<size_t>(Asset::Dollar)] = MarketParser::dollarQuote(*bid, std::move(series));
    return true;
}
