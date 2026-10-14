# Plano 2 — Cripto — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** uma página entre o Início e o Spotify com Bitcoin, Ethereum e dólar em reais, variação do período em verde
ou vermelho e uma sparkline por ativo, atualizada a cada 60 s pela rede.

**Architecture:** lógica pura em `src/crypto/` (formatação, parsing dos JSONs, reamostragem da série) testada no PC;
`crypto/device/MarketFeed` faz as chamadas HTTPS na `networkTask` (núcleo 0) e publica `Published<MarketState>`;
`crypto/screen/CryptoPage` lê no `onTick` e redesenha só quando a versão mudou. Mesmo padrão das páginas do plano 1.

**Tech Stack:** o que já existe no projeto (PlatformIO/pioarduino, LVGL 9.4, ArduinoJson 7.4.2, Unity) mais
`HTTPClient` + `NetworkClientSecure` do core Arduino 3.3.12, com o pacote de certificados embutido
(`useBuiltinCACertBundle()`; `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` confirmado no sdkconfig do core instalado).

**Spec:** `docs/superpowers/specs/2026-10-13-cripto-design.md`

## Global Constraints

- Regras de código de `~/.claude/CLAUDE.md` e `CLAUDE.md` do projeto: nomes por extenso, chaves sempre, um comando por
  linha, até 120 colunas, continuação com 4 espaços, linha em branco entre blocos, comentário só de porquê.
- Lógica pura fora de `device/`, `screen/`, `web/`, `storage/`; `std::string` na lógica, `String` só nas bordas.
- Só o loop do Arduino (núcleo 1) chama LVGL; a rede publica com `Published<T>`.
- Ordem do deslize: **Início → Cripto → Spotify → Claude** (Spotify e Claude ainda não existem; Cripto entra logo
  depois da `HomePage`).
- Cores: verde `#9FD49A` (variação ≥ 0), vermelho `#E24B4A` (< 0); o resto vem de `theme`.
- Formato: `R$ 580.000` (≥ 1000, sem centavos, ponto de milhar), `R$ 5,43` (< 1000, vírgula), variação `+2,3 %` /
  `−0,5 %` (U+2212, uma casa, espaço antes do `%`).
- Período: 24 h para BTC e ETH (últimos 24 pontos da série horária), 30 d para o dólar.
- Atualização a cada 60 s; timeout de 5 s por chamada; `429` → recuo dobrando até 10 min.
- URLs exatas:
  - `https://api.coingecko.com/api/v3/coins/markets?vs_currency=brl&ids=bitcoin,ethereum&sparkline=true&price_change_percentage=24h`
  - `https://economia.awesomeapi.com.br/json/last/USD-BRL`
  - `https://economia.awesomeapi.com.br/json/daily/USD-BRL/30`
- Testes espelham `src/`: `src/crypto/PriceFormat.cpp` → `test/crypto/test_price_format/test_main.cpp`.
- Commits em português, conventional commits, terminando com `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- Build no Windows: `PLATFORMIO_CORE_DIR=C:\pio`; `pio run` pelo PowerShell.

## Review Focus

1. **Preço com casas estranhas** — `0.999` tem que virar `R$ 1,00`, não `R$ 0,100`; `999.995` vira `R$ 1.000`
   (arredonda para cima e cruza o limite). Teste em `test_price_format` (Task 1).
2. **JSON do CoinGecko com `current_price: null`** (acontece em instabilidade) — o ativo fica `known = false`, os
   outros seguem. Teste em `test_market_parser` (Task 2).
3. **Série com um ponto só ou vazia** — sparkline escondida, variação do dólar `0`, nada divide por zero. Testes em
   `test_market_parser` e `test_sparkline` (Tasks 2 e 3).
4. **Rede caindo no meio** — `HTTPClient` com timeout de 5 s; uma chamada que falha não trava a `networkTask` nem
   impede o `wifi.process()`; o estado anterior permanece com `lastFetchFailed = true`. Roteiro (Task 4).
5. **Página aberta antes da primeira resposta** — mostra `—` e `buscando…`, não lixo nem crash. Teste manual no
   roteiro e `MarketState` com `known = false` por padrão (Task 2 e 5).

---

### Task 1: Formatação em reais e porcentagem

**Files:**
- Create: `firmware/src/crypto/PriceFormat.h`, `firmware/src/crypto/PriceFormat.cpp`
- Test: `firmware/test/crypto/test_price_format/test_main.cpp`

**Interfaces:**
- Produces: `namespace PriceFormat { std::string reais(double value); std::string percent(double value); }`

- [ ] **Step 1: Teste**

`firmware/test/crypto/test_price_format/test_main.cpp`:

```cpp
#include <unity.h>

#include "crypto/PriceFormat.h"

void setUp()
{
}

void tearDown()
{
}

void test_large_prices_drop_cents_and_group_thousands()
{
    TEST_ASSERT_EQUAL_STRING("R$ 580.000", PriceFormat::reais(580000.4).c_str());
    TEST_ASSERT_EQUAL_STRING("R$ 1.000", PriceFormat::reais(1000).c_str());
    TEST_ASSERT_EQUAL_STRING("R$ 1.234.567", PriceFormat::reais(1234567.0).c_str());
}

void test_small_prices_keep_two_decimals_with_comma()
{
    TEST_ASSERT_EQUAL_STRING("R$ 5,43", PriceFormat::reais(5.4321).c_str());
    TEST_ASSERT_EQUAL_STRING("R$ 999,99", PriceFormat::reais(999.99).c_str());
    TEST_ASSERT_EQUAL_STRING("R$ 0,50", PriceFormat::reais(0.5).c_str());
}

void test_rounding_can_cross_the_thousand_line()
{
    TEST_ASSERT_EQUAL_STRING("R$ 1,00", PriceFormat::reais(0.999).c_str());
    TEST_ASSERT_EQUAL_STRING("R$ 1.000", PriceFormat::reais(999.995).c_str());
}

void test_percent_always_carries_a_sign()
{
    TEST_ASSERT_EQUAL_STRING("+2,3 %", PriceFormat::percent(2.34).c_str());
    TEST_ASSERT_EQUAL_STRING("\u2212" "0,5 %", PriceFormat::percent(-0.5).c_str());
    TEST_ASSERT_EQUAL_STRING("+0,0 %", PriceFormat::percent(0).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_large_prices_drop_cents_and_group_thousands);
    RUN_TEST(test_small_prices_keep_two_decimals_with_comma);
    RUN_TEST(test_rounding_can_cross_the_thousand_line);
    RUN_TEST(test_percent_always_carries_a_sign);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f crypto/test_price_format`
Expected: FAIL — `crypto/PriceFormat.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`firmware/src/crypto/PriceFormat.h`:

```cpp
#pragma once

#include <string>

// Números do jeito que se lê no Brasil: ponto de milhar, vírgula decimal, "R$ " na frente, espaço antes do "%".
namespace PriceFormat {

std::string reais(double value);
std::string percent(double value);

}
```

`firmware/src/crypto/PriceFormat.cpp`:

```cpp
#include "crypto/PriceFormat.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr double CENTS_THRESHOLD = 1000.0;
constexpr const char* MINUS_SIGN = "\u2212";

std::string groupThousands(long long value)
{
    std::string digits = std::to_string(value);
    std::string grouped;

    for (size_t position = 0; position < digits.size(); position++) {
        size_t remaining = digits.size() - position;
        if (position > 0 && remaining % 3 == 0) {
            grouped += '.';
        }
        grouped += digits[position];
    }

    return grouped;
}

std::string oneDecimalWithComma(double value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f", value);

    std::string text = buffer;
    text.replace(text.find('.'), 1, ",");
    return text;
}

}

namespace PriceFormat {

std::string reais(double value)
{
    // Arredonda nos centavos primeiro: 999,995 vira 1.000,00 e cai na regra de milhar.
    double cents = std::round(value * 100.0);
    if (cents >= CENTS_THRESHOLD * 100.0) {
        return "R$ " + groupThousands(static_cast<long long>(std::round(cents / 100.0)));
    }

    long long whole = static_cast<long long>(cents / 100.0);
    long long fraction = static_cast<long long>(cents) % 100;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "R$ %lld,%02lld", whole, fraction);
    return buffer;
}

std::string percent(double value)
{
    std::string magnitude = oneDecimalWithComma(std::fabs(value));
    bool negative = magnitude != "0,0" && value < 0;

    return (negative ? MINUS_SIGN : "+") + magnitude + " %";
}

}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native -f crypto/test_price_format`
Expected: `4 test cases: 4 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/crypto firmware/test/crypto
git commit -m "feat(crypto): formatação de preço em reais e variação em porcentagem

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: Estado do mercado e parsing dos JSONs

**Files:**
- Create: `firmware/src/crypto/Asset.h`
- Create: `firmware/src/crypto/MarketState.h`
- Create: `firmware/src/crypto/MarketParser.h`, `firmware/src/crypto/MarketParser.cpp`
- Test: `firmware/test/crypto/test_market_parser/test_main.cpp`

**Interfaces:**
- Consumes: `LocalTime` de `support/LocalTime.h`.
- Produces:
  - `enum class Asset { Bitcoin, Ethereum, Dollar }; constexpr size_t ASSET_COUNT = 3;`
    `struct AssetInfo { const char* symbol; const char* name; const char* period; };`
    `const AssetInfo& assetInfo(Asset asset);`
  - `struct Quote { bool known = false; double priceBrl = 0; double changePercent = 0; std::vector<float> series; };`
    `struct MarketState { Quote quotes[ASSET_COUNT]; std::optional<LocalTime> updatedAt; bool lastFetchFailed = false; bool everFetched = false; };`
  - `namespace MarketParser { std::optional<Quote> coinGecko(const std::string& json, const char* coinId);
    std::optional<double> dollarBid(const std::string& json); std::vector<float> dollarSeries(const std::string& json);
    Quote dollarQuote(double bid, std::vector<float> series); }`
    — `coinGecko` devolve a série com no máximo os **últimos 24** pontos; `dollarSeries` devolve do mais antigo para
    o mais novo; `dollarQuote` calcula a variação `(último − primeiro) / primeiro × 100` (0 se a série tem menos de 2).

- [ ] **Step 1: Teste**

`firmware/test/crypto/test_market_parser/test_main.cpp`:

```cpp
#include <unity.h>

#include "crypto/MarketParser.h"

namespace {

// Recorte real da resposta do CoinGecko: dois ativos, série encurtada para 26 pontos.
const char* COINGECKO = R"([
  {"id":"bitcoin","symbol":"btc","current_price":580123.45,"price_change_percentage_24h":2.34,
   "sparkline_in_7d":{"price":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26]}},
  {"id":"ethereum","symbol":"eth","current_price":null,"price_change_percentage_24h":-1.2,
   "sparkline_in_7d":{"price":[]}}
])";

const char* DOLLAR_LAST = R"({"USDBRL":{"code":"USD","codein":"BRL","bid":"5.4321","ask":"5.4350","pctChange":"-0.21"}})";

// A AwesomeAPI devolve do mais novo para o mais antigo.
const char* DOLLAR_DAILY = R"([{"bid":"5.40"},{"bid":"5.50"},{"bid":"5.00"}])";

}

void setUp()
{
}

void tearDown()
{
}

void test_coingecko_quote_keeps_the_last_24_points()
{
    std::optional<Quote> quote = MarketParser::coinGecko(COINGECKO, "bitcoin");

    TEST_ASSERT_TRUE(quote.has_value());
    TEST_ASSERT_TRUE(quote->known);
    TEST_ASSERT_EQUAL_DOUBLE(580123.45, quote->priceBrl);
    TEST_ASSERT_EQUAL_DOUBLE(2.34, quote->changePercent);
    TEST_ASSERT_EQUAL_size_t(24, quote->series.size());
    TEST_ASSERT_EQUAL_FLOAT(3.0f, quote->series.front());
    TEST_ASSERT_EQUAL_FLOAT(26.0f, quote->series.back());
}

void test_coingecko_null_price_is_unknown()
{
    std::optional<Quote> quote = MarketParser::coinGecko(COINGECKO, "ethereum");

    TEST_ASSERT_TRUE(quote.has_value());
    TEST_ASSERT_FALSE(quote->known);
    TEST_ASSERT_EQUAL_size_t(0, quote->series.size());
}

void test_coingecko_missing_coin_is_nothing()
{
    TEST_ASSERT_FALSE(MarketParser::coinGecko(COINGECKO, "solana").has_value());
}

void test_coingecko_broken_json_is_nothing()
{
    TEST_ASSERT_FALSE(MarketParser::coinGecko("{not json", "bitcoin").has_value());
}

void test_dollar_bid_is_a_number_inside_a_string()
{
    std::optional<double> bid = MarketParser::dollarBid(DOLLAR_LAST);

    TEST_ASSERT_TRUE(bid.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(5.4321, *bid);
}

void test_dollar_series_is_reversed_to_oldest_first()
{
    std::vector<float> series = MarketParser::dollarSeries(DOLLAR_DAILY);

    TEST_ASSERT_EQUAL_size_t(3, series.size());
    TEST_ASSERT_EQUAL_FLOAT(5.00f, series[0]);
    TEST_ASSERT_EQUAL_FLOAT(5.40f, series[2]);
}

void test_dollar_quote_change_is_over_the_whole_series()
{
    Quote quote = MarketParser::dollarQuote(5.40, {5.00f, 5.50f, 5.40f});

    TEST_ASSERT_TRUE(quote.known);
    TEST_ASSERT_EQUAL_DOUBLE(5.40, quote.priceBrl);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 8.0, quote.changePercent);
}

void test_dollar_quote_with_a_single_point_has_no_change()
{
    Quote quote = MarketParser::dollarQuote(5.40, {5.40f});

    TEST_ASSERT_EQUAL_DOUBLE(0.0, quote.changePercent);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_coingecko_quote_keeps_the_last_24_points);
    RUN_TEST(test_coingecko_null_price_is_unknown);
    RUN_TEST(test_coingecko_missing_coin_is_nothing);
    RUN_TEST(test_coingecko_broken_json_is_nothing);
    RUN_TEST(test_dollar_bid_is_a_number_inside_a_string);
    RUN_TEST(test_dollar_series_is_reversed_to_oldest_first);
    RUN_TEST(test_dollar_quote_change_is_over_the_whole_series);
    RUN_TEST(test_dollar_quote_with_a_single_point_has_no_change);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f crypto/test_market_parser`
Expected: FAIL — `crypto/MarketParser.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`firmware/src/crypto/Asset.h`:

```cpp
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
```

`firmware/src/crypto/MarketState.h`:

```cpp
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
```

`firmware/src/crypto/MarketParser.h`:

```cpp
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
```

`firmware/src/crypto/MarketParser.cpp`:

```cpp
#include "crypto/MarketParser.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cstdlib>

namespace {

constexpr size_t SPARKLINE_POINTS = 24;

// A resposta tem 168 pontos por moeda e campos que não usamos; o filtro deixa só o que a tela precisa.
JsonDocument coinGeckoFilter()
{
    JsonDocument filter;
    filter[0]["id"] = true;
    filter[0]["current_price"] = true;
    filter[0]["price_change_percentage_24h"] = true;
    filter[0]["sparkline_in_7d"]["price"] = true;
    return filter;
}

std::vector<float> lastPoints(JsonArrayConst prices, size_t count)
{
    std::vector<float> series;
    size_t skip = prices.size() > count ? prices.size() - count : 0;

    size_t index = 0;
    for (JsonVariantConst price : prices) {
        if (index++ >= skip) {
            series.push_back(price.as<float>());
        }
    }

    return series;
}

}

namespace MarketParser {

std::optional<Quote> coinGecko(const std::string& json, const char* coinId)
{
    JsonDocument document;
    DeserializationError error = deserializeJson(document, json, DeserializationOption::Filter(coinGeckoFilter()));
    if (error != DeserializationError::Ok) {
        return std::nullopt;
    }

    for (JsonObjectConst coin : document.as<JsonArrayConst>()) {
        if (std::string(coin["id"] | "") != coinId) {
            continue;
        }

        Quote quote;
        quote.known = coin["current_price"].is<double>();
        quote.priceBrl = coin["current_price"] | 0.0;
        quote.changePercent = coin["price_change_percentage_24h"] | 0.0;
        quote.series = lastPoints(coin["sparkline_in_7d"]["price"].as<JsonArrayConst>(), SPARKLINE_POINTS);
        return quote;
    }

    return std::nullopt;
}

std::optional<double> dollarBid(const std::string& json)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok) {
        return std::nullopt;
    }

    const char* bid = document["USDBRL"]["bid"];
    if (bid == nullptr) {
        return std::nullopt;
    }

    return std::strtod(bid, nullptr);
}

std::vector<float> dollarSeries(const std::string& json)
{
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok) {
        return {};
    }

    std::vector<float> series;
    for (JsonObjectConst day : document.as<JsonArrayConst>()) {
        const char* bid = day["bid"];
        if (bid != nullptr) {
            series.push_back(static_cast<float>(std::strtod(bid, nullptr)));
        }
    }

    // A API devolve do mais novo para o mais antigo; a sparkline lê da esquerda para a direita no tempo.
    std::reverse(series.begin(), series.end());
    return series;
}

Quote dollarQuote(double bid, std::vector<float> series)
{
    Quote quote;
    quote.known = true;
    quote.priceBrl = bid;
    quote.series = std::move(series);

    if (quote.series.size() >= 2 && quote.series.front() != 0.0f) {
        quote.changePercent = (quote.series.back() - quote.series.front()) / quote.series.front() * 100.0;
    }

    return quote;
}

}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native`
Expected: todos verdes (`test_market_parser` com 8 casos). Se o ArduinoJson reclamar de `Filter` com `JsonDocument`
temporário, guarde o filtro numa variável local antes de chamar `deserializeJson`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/crypto firmware/test/crypto
git commit -m "feat(crypto): estado do mercado e leitura dos JSONs do CoinGecko e da AwesomeAPI

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: Sparkline — série para pontos do gráfico

**Files:**
- Create: `firmware/src/crypto/Sparkline.h`, `firmware/src/crypto/Sparkline.cpp`
- Test: `firmware/test/crypto/test_sparkline/test_main.cpp`

**Interfaces:**
- Produces: `namespace Sparkline { std::vector<int32_t> toChart(const std::vector<float>& series, size_t maxPoints); }`
  — pega os **últimos** `maxPoints`, converte para centavos inteiros (`×100`, arredondado); o `lv_chart` escala
  sozinho pelo mínimo e máximo. Vazio → vazio.

- [ ] **Step 1: Teste**

`firmware/test/crypto/test_sparkline/test_main.cpp`:

```cpp
#include <unity.h>

#include "crypto/Sparkline.h"

void setUp()
{
}

void tearDown()
{
}

void test_keeps_the_last_points_in_cents()
{
    std::vector<int32_t> points = Sparkline::toChart({1.0f, 2.5f, 3.0f, 4.25f}, 3);

    TEST_ASSERT_EQUAL_size_t(3, points.size());
    TEST_ASSERT_EQUAL_INT32(250, points[0]);
    TEST_ASSERT_EQUAL_INT32(425, points[2]);
}

void test_short_series_is_kept_whole()
{
    std::vector<int32_t> points = Sparkline::toChart({5.4f, 5.5f}, 30);

    TEST_ASSERT_EQUAL_size_t(2, points.size());
    TEST_ASSERT_EQUAL_INT32(540, points[0]);
}

void test_empty_series_is_empty()
{
    TEST_ASSERT_EQUAL_size_t(0, Sparkline::toChart({}, 24).size());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_keeps_the_last_points_in_cents);
    RUN_TEST(test_short_series_is_kept_whole);
    RUN_TEST(test_empty_series_is_empty);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f crypto/test_sparkline`
Expected: FAIL por include ausente.

- [ ] **Step 3: Implementar**

`firmware/src/crypto/Sparkline.h`:

```cpp
#pragma once

#include <cstdint>
#include <vector>

namespace Sparkline {

std::vector<int32_t> toChart(const std::vector<float>& series, size_t maxPoints);

}
```

`firmware/src/crypto/Sparkline.cpp`:

```cpp
#include "crypto/Sparkline.h"

#include <cmath>

namespace Sparkline {

// O lv_chart só aceita inteiros; centavos preservam a forma da linha mesmo no dólar, que varia nos centavos.
std::vector<int32_t> toChart(const std::vector<float>& series, size_t maxPoints)
{
    std::vector<int32_t> points;
    size_t skip = series.size() > maxPoints ? series.size() - maxPoints : 0;

    for (size_t index = skip; index < series.size(); index++) {
        points.push_back(static_cast<int32_t>(std::lround(series[index] * 100.0f)));
    }

    return points;
}

}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native`
Expected: todos verdes.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/crypto firmware/test/crypto
git commit -m "feat(crypto): série de preços para pontos de sparkline

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: MarketFeed — chamadas HTTPS na tarefa de rede

**Files:**
- Create: `firmware/src/crypto/device/MarketFeed.h`, `firmware/src/crypto/device/MarketFeed.cpp`
- Modify: `firmware/src/main.cpp` (global `MarketFeed marketFeed;`, `marketFeed.poll(millis(), wifi.connected());`
  no `for (;;)` da `networkTask`)
- Modify: `docs/roteiro-de-teste.md`

**Interfaces:**
- Consumes: `MarketParser::*`, `MarketState`, `Published<T>`, `LocalTime::now()`.
- Produces: `class MarketFeed { void poll(uint32_t nowMillis, bool wifiConnected); const Published<MarketState>& state() const; }`

- [ ] **Step 1: Implementar**

`firmware/src/crypto/device/MarketFeed.h`:

```cpp
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
```

`firmware/src/crypto/device/MarketFeed.cpp`:

```cpp
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

    current.quotes[static_cast<size_t>(Asset::Dollar)] = MarketParser::dollarQuote(*bid, MarketParser::dollarSeries(*daily));
    return true;
}
```

(`http.getString()` carrega o corpo inteiro em `String`; a resposta do CoinGecko tem ~20 KB. Se o heap interno
reclamar no serial, troque por `http.getStream()` lido em pedaços para um `std::string` reservado em PSRAM com
`heap_caps_malloc`. Confirme ao gravar olhando `ESP.getFreeHeap()` antes e depois do primeiro `poll`.)

- [ ] **Step 2: Ligar no `main.cpp`**

Inclua `"crypto/device/MarketFeed.h"`, declare `MarketFeed marketFeed;` junto dos outros globais e, dentro do
`for (;;)` da `networkTask`, logo depois de `wifi.process();`:

```cpp
        marketFeed.poll(millis(), wifi.connected());
```

Enquanto não existe a página, um log temporário ajuda a conferir na placa. Acrescente **só para este passo** no
`loop()`, e apague no Step 4:

```cpp
    static uint32_t seenMarket = 0;
    MarketState market;
    if (marketFeed.state().readIfChanged(market, seenMarket)) {
        Serial.printf("mercado: btc %.0f eth %.0f usd %.2f falhou=%d\n",
            market.quotes[0].priceBrl, market.quotes[1].priceBrl, market.quotes[2].priceBrl, market.lastFetchFailed);
    }
```

- [ ] **Step 3: Compilar e gravar**

Run (PowerShell, `PLATFORMIO_CORE_DIR=C:\pio`): `cd firmware; pio run -e board -t upload --upload-port COM8`
Expected: compila; no serial, em até ~70 s depois de conectar o Wi-Fi, uma linha `mercado: btc 5xxxxx eth 1xxxx usd
5.xx falhou=0`. Se `falhou=1` com os três zerados, o TLS não fechou: confira o horário (certificados exigem relógio;
o NTP precisa ter sincronizado) e, persistindo, registre no ledger e teste `client.setInsecure()` só para
diagnosticar — **não** deixe o `setInsecure` no commit.

- [ ] **Step 4: Tirar o log temporário e commitar**

Apague o bloco do Step 2 no `loop()`. Acrescente ao roteiro:

```markdown
## Cripto

- [ ] Em até 2 min depois do Wi-Fi, a página mostra BTC, ETH e dólar; valores batem com coingecko.com e com o Google.
- [ ] Variação positiva em verde, negativa em vermelho, com a sparkline na mesma cor.
- [ ] Roteador desligado: valores ficam e o rodapé vira "visto às HH:MM"; religado, volta a "atualizado" em até 2 min.
- [ ] Página aberta antes da primeira resposta: traços e "buscando…", sem travar.
```

```bash
git add firmware/src docs/roteiro-de-teste.md
git commit -m "feat(crypto): busca das cotações por HTTPS na tarefa de rede

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 5: CryptoPage e navegação

**Files:**
- Create: `firmware/src/crypto/screen/CryptoPage.h`, `firmware/src/crypto/screen/CryptoPage.cpp`
- Modify: `firmware/src/main.cpp` (registrar depois da `HomePage`)
- Modify: `README.md` (linha de `crypto/` na tabela de pastas; "cotações" sai da lista de ideias)

**Interfaces:**
- Consumes: `MarketState`, `assetInfo`, `PriceFormat`, `Sparkline::toChart`, `theme`, `fonts`, `Page`.
- Produces: `class CryptoPage : public Page { explicit CryptoPage(const Published<MarketState>&); }`

- [ ] **Step 1: Implementar**

`firmware/src/crypto/screen/CryptoPage.h`:

```cpp
#pragma once

#include <lvgl.h>

#include "crypto/MarketState.h"
#include "navigation/Page.h"
#include "support/Published.h"

class CryptoPage : public Page {
public:
    explicit CryptoPage(const Published<MarketState>& state);

    void build(lv_obj_t* parent) override;
    void onTick(uint32_t nowMillis) override;

private:
    struct Row {
        lv_obj_t* panel = nullptr;
        lv_obj_t* price = nullptr;
        lv_obj_t* change = nullptr;
        lv_obj_t* chart = nullptr;
        lv_chart_series_t* line = nullptr;
    };

    void buildRow(lv_obj_t* parent, Asset asset, int32_t y);
    void render(const MarketState& market);
    void renderRow(Row& row, const Quote& quote);
    void renderFooter(const MarketState& market);

    const Published<MarketState>& state;
    uint32_t seenVersion = 0;
    Row rows[ASSET_COUNT];
    lv_obj_t* footer = nullptr;
};
```

`firmware/src/crypto/screen/CryptoPage.cpp`:

```cpp
#include "crypto/screen/CryptoPage.h"

#include <vector>

#include "board/ScreenGeometry.h"
#include "crypto/PriceFormat.h"
#include "crypto/Sparkline.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t MARGIN = 12;
constexpr int32_t ROW_HEIGHT = 86;
constexpr int32_t ROW_GAP = 6;
constexpr int32_t CHART_HEIGHT = 22;
constexpr size_t CHART_POINTS = 30;
constexpr uint32_t UP_COLOR = 0x9FD49A;
constexpr uint32_t DOWN_COLOR = 0xE24B4A;

lv_color_t changeColor(double changePercent)
{
    return lv_color_hex(changePercent < 0 ? DOWN_COLOR : UP_COLOR);
}

}

CryptoPage::CryptoPage(const Published<MarketState>& state)
    : state(state)
{
}

void CryptoPage::build(lv_obj_t* parent)
{
    lv_obj_set_style_bg_color(parent, theme::background(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    for (size_t index = 0; index < ASSET_COUNT; index++) {
        buildRow(parent, static_cast<Asset>(index), MARGIN + static_cast<int32_t>(index) * (ROW_HEIGHT + ROW_GAP));
    }

    footer = theme::label(parent, fonts::small, theme::textSecondary());
    lv_label_set_text(footer, "buscando…");
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, MARGIN, -16);
}

void CryptoPage::buildRow(lv_obj_t* parent, Asset asset, int32_t y)
{
    Row& row = rows[static_cast<size_t>(asset)];
    const AssetInfo& info = assetInfo(asset);

    row.panel = theme::panel(parent);
    lv_obj_set_size(row.panel, SCREEN_WIDTH - 2 * MARGIN, ROW_HEIGHT);
    lv_obj_set_pos(row.panel, MARGIN, y);
    lv_obj_set_style_pad_all(row.panel, 10, 0);

    lv_obj_t* symbol = theme::label(row.panel, fonts::large, theme::text());
    lv_label_set_text(symbol, info.symbol);
    lv_obj_align(symbol, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* name = theme::label(row.panel, fonts::small, theme::textSecondary());
    lv_label_set_text(name, info.name);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 24);

    row.price = theme::label(row.panel, fonts::title, theme::text());
    lv_label_set_text(row.price, "—");
    lv_obj_align(row.price, LV_ALIGN_TOP_LEFT, 110, 2);

    row.change = theme::label(row.panel, fonts::medium, theme::textSecondary());
    lv_label_set_text(row.change, "");
    lv_obj_align(row.change, LV_ALIGN_TOP_RIGHT, 0, 4);

    lv_obj_t* period = theme::label(row.panel, fonts::small, theme::textSecondary());
    lv_label_set_text(period, info.period);
    lv_obj_align(period, LV_ALIGN_TOP_RIGHT, 0, 24);

    row.chart = lv_chart_create(row.panel);
    lv_obj_remove_style_all(row.chart);
    lv_obj_set_size(row.chart, SCREEN_WIDTH - 2 * MARGIN - 20, CHART_HEIGHT);
    lv_obj_align(row.chart, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_chart_set_type(row.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(row.chart, 0, 0);
    lv_chart_set_point_count(row.chart, CHART_POINTS);
    lv_obj_set_style_size(row.chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(row.chart, 1, LV_PART_ITEMS);
    row.line = lv_chart_add_series(row.chart, lv_color_hex(UP_COLOR), LV_CHART_AXIS_PRIMARY_Y);
    lv_obj_add_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
}

void CryptoPage::onTick(uint32_t)
{
    MarketState market;
    if (state.readIfChanged(market, seenVersion)) {
        render(market);
    }
}

void CryptoPage::render(const MarketState& market)
{
    for (size_t index = 0; index < ASSET_COUNT; index++) {
        renderRow(rows[index], market.quotes[index]);
    }

    renderFooter(market);
}

void CryptoPage::renderRow(Row& row, const Quote& quote)
{
    if (!quote.known) {
        lv_label_set_text(row.price, "—");
        lv_label_set_text(row.change, "");
        lv_obj_add_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(row.price, PriceFormat::reais(quote.priceBrl).c_str());
    lv_label_set_text(row.change, PriceFormat::percent(quote.changePercent).c_str());
    lv_obj_set_style_text_color(row.change, changeColor(quote.changePercent), 0);

    std::vector<int32_t> points = Sparkline::toChart(quote.series, CHART_POINTS);
    if (points.size() < 2) {
        lv_obj_add_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int32_t low = *std::min_element(points.begin(), points.end());
    int32_t high = *std::max_element(points.begin(), points.end());
    lv_chart_set_point_count(row.chart, static_cast<uint32_t>(points.size()));
    lv_chart_set_axis_range(row.chart, LV_CHART_AXIS_PRIMARY_Y, low, high == low ? low + 1 : high);
    lv_chart_set_series_color(row.chart, row.line, changeColor(quote.changePercent));
    for (size_t index = 0; index < points.size(); index++) {
        lv_chart_set_value_by_id(row.chart, row.line, static_cast<uint32_t>(index), points[index]);
    }
    lv_chart_refresh(row.chart);
    lv_obj_remove_flag(row.chart, LV_OBJ_FLAG_HIDDEN);
}

void CryptoPage::renderFooter(const MarketState& market)
{
    if (!market.everFetched) {
        lv_label_set_text(footer, market.lastFetchFailed ? "sem resposta ainda" : "buscando…");
        return;
    }

    std::string when = market.updatedAt ? " " + market.updatedAt->hourMinute() : "";
    std::string text = market.lastFetchFailed ? "visto às" + when : "atualizado" + when;
    lv_label_set_text(footer, text.c_str());
}
```

(Nomes da API do `lv_chart` no LVGL 9.4: `lv_chart_set_axis_range` (antes `lv_chart_set_range`),
`lv_chart_set_value_by_id`, `lv_chart_set_series_color`, `lv_obj_remove_flag` (antes `lv_obj_clear_flag`). Se o
compilador apontar um nome, procure em `.pio/libdeps/board/lvgl/src/widgets/chart/lv_chart.h`. Inclua `<algorithm>`
para `min_element`.)

- [ ] **Step 2: Registrar no `main.cpp`**

Inclua `"crypto/screen/CryptoPage.h"` e, no bloco `if (configured)`, logo depois da `HomePage`:

```cpp
        navigator.add(std::make_unique<CryptoPage>(marketFeed.state()));
```

- [ ] **Step 3: Compilar, gravar, conferir**

Run (PowerShell): `cd firmware; pio run -e board -t upload --upload-port COM8`
Expected: deslizar para a esquerda a partir do Início mostra a página; em até 2 min os valores aparecem; a tela
não trava. Passe a seção "Cripto" do roteiro.

- [ ] **Step 4: README**

Na tabela de pastas, depois de `photos/`:

```markdown
| `firmware/src/crypto/` | cotações: Bitcoin, Ethereum e dólar em reais com sparkline |
```

Em "Ideias para você", tire "cotações" da lista.

- [ ] **Step 5: Commit**

```bash
git add firmware/src README.md docs/roteiro-de-teste.md
git commit -m "feat(crypto): página com Bitcoin, Ethereum e dólar em reais

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

## Self-review

- **Cobertura da spec:** §2 ordem e pasta (T5, T1–3); §3 tela e formato (T1, T5); §4 dados, filtro, 429, HTTPS
  verificado (T2, T4); §5 código (T1–T5, um arquivo por responsabilidade); §6 erros (T2 `known`, T4 `lastFetchFailed`
  e intervalo, T5 `—`/`visto às`); §7 testes (T1, T2, T3 no PC; roteiro em T4/T5). §8 fora de escopo: nada aqui.
- **Placeholders:** nenhum; todo passo tem código ou comando com `Expected`.
- **Tipos:** `Quote`/`MarketState` (T2) usados em T4 e T5; `MarketParser::coinGecko/dollarBid/dollarSeries/dollarQuote`
  (T2) usados em T4; `Sparkline::toChart` (T3) em T5; `MarketFeed::state()` devolve `const Published<MarketState>&`
  e `CryptoPage` recebe o mesmo tipo (T4 → T5); `assetInfo` (T2) em T5.
- **Review Focus → testes:** 1 → `test_rounding_can_cross_the_thousand_line` (T1); 2 →
  `test_coingecko_null_price_is_unknown` (T2); 3 → `test_dollar_quote_with_a_single_point_has_no_change` (T2) e
  `test_empty_series_is_empty` (T3); 4 → roteiro (T4) e `poll` não bloqueia além dos timeouts; 5 → `Quote::known`
  falso por padrão (T2) e `renderRow` (T5).
- **Ponto frágil declarado:** a API do `lv_chart` muda de nome entre versões 9.x; os nomes acima são os do 9.4 e o
  passo diz onde conferir. `getString()` em `String` para ~20 KB: aceitável na PSRAM/heap atual, com o plano B
  anotado em T4.
