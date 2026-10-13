#include <unity.h>

#include "setup/AdminPin.h"

void setUp()
{
}

void tearDown()
{
}

void test_accepts_four_digits()
{
    TEST_ASSERT_TRUE(AdminPin::parse("1234").has_value());
}

void test_accepts_six_digits()
{
    std::optional<AdminPin> pin = AdminPin::parse("123456");

    TEST_ASSERT_TRUE(pin.has_value());
    TEST_ASSERT_EQUAL_STRING("123456", pin->value().c_str());
}

void test_refuses_three_digits()
{
    TEST_ASSERT_FALSE(AdminPin::parse("123").has_value());
}

void test_refuses_seven_digits()
{
    TEST_ASSERT_FALSE(AdminPin::parse("1234567").has_value());
}

void test_refuses_anything_that_is_not_a_digit()
{
    TEST_ASSERT_FALSE(AdminPin::parse("12a4").has_value());
    TEST_ASSERT_FALSE(AdminPin::parse("12 34").has_value());
    TEST_ASSERT_FALSE(AdminPin::parse("").has_value());
}

void test_trims_the_ends()
{
    std::optional<AdminPin> pin = AdminPin::parse("  2580  ");

    TEST_ASSERT_TRUE(pin.has_value());
    TEST_ASSERT_EQUAL_STRING("2580", pin->value().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_four_digits);
    RUN_TEST(test_accepts_six_digits);
    RUN_TEST(test_refuses_three_digits);
    RUN_TEST(test_refuses_seven_digits);
    RUN_TEST(test_refuses_anything_that_is_not_a_digit);
    RUN_TEST(test_trims_the_ends);
    return UNITY_END();
}
