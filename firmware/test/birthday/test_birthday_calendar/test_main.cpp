#include <unity.h>

#include "birthday/BirthdayCalendar.h"

void setUp()
{
}

void tearDown()
{
}

void test_first_boot_shows_even_without_a_clock()
{
    TEST_ASSERT_TRUE(BirthdayCalendar::shouldShow(true, std::nullopt, std::nullopt));
}

void test_after_first_boot_no_clock_means_no_show()
{
    TEST_ASSERT_FALSE(BirthdayCalendar::shouldShow(false, std::nullopt, 2026));
}

void test_birthday_next_year_shows_again()
{
    LocalTime birthday{8, 0, 22, 10, 2027, 5};

    TEST_ASSERT_TRUE(BirthdayCalendar::shouldShow(false, birthday, 2026));
}

void test_same_year_only_once()
{
    LocalTime birthday{20, 0, 22, 10, 2026, 4};

    TEST_ASSERT_FALSE(BirthdayCalendar::shouldShow(false, birthday, 2026));
}

void test_any_other_day_is_quiet()
{
    LocalTime dayAfter{8, 0, 23, 10, 2027, 6};

    TEST_ASSERT_FALSE(BirthdayCalendar::shouldShow(false, dayAfter, 2026));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_shows_even_without_a_clock);
    RUN_TEST(test_after_first_boot_no_clock_means_no_show);
    RUN_TEST(test_birthday_next_year_shows_again);
    RUN_TEST(test_same_year_only_once);
    RUN_TEST(test_any_other_day_is_quiet);
    return UNITY_END();
}
