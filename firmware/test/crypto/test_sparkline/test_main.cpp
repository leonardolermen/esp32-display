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
