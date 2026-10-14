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
    TEST_ASSERT_EQUAL_STRING("−" "0,5 %", PriceFormat::percent(-0.5).c_str());
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
