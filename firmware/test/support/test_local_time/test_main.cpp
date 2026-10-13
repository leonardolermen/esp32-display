#include <unity.h>

#include "support/LocalTime.h"

void setUp()
{
}

void tearDown()
{
}

void test_hour_minute_has_two_digits()
{
    LocalTime time{9, 5, 1, 1, 2027, 0};

    TEST_ASSERT_EQUAL_STRING("09:05", time.hourMinute().c_str());
}

void test_long_date_in_portuguese()
{
    LocalTime time{14, 32, 22, 10, 2026, 4};

    TEST_ASSERT_EQUAL_STRING("quinta, 22 de outubro", time.longDate().c_str());
}

void test_sunday_first_of_january()
{
    LocalTime time{0, 0, 1, 1, 2027, 0};

    TEST_ASSERT_EQUAL_STRING("domingo, 1 de janeiro", time.longDate().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_hour_minute_has_two_digits);
    RUN_TEST(test_long_date_in_portuguese);
    RUN_TEST(test_sunday_first_of_january);
    return UNITY_END();
}
