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
    // O ArduinoJson guarda números curtos como float; a tela mostra centavos e uma casa de porcentagem.
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 580123.45, quote->priceBrl);
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, 2.34, quote->changePercent);
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
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, 5.4321, *bid);
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
    TEST_ASSERT_DOUBLE_WITHIN(0.000001, 5.40, quote.priceBrl);
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
