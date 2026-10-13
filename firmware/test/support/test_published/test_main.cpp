#include <unity.h>

#include "support/Published.h"

struct Weather {
    int degrees;
};

void setUp()
{
}

void tearDown()
{
}

void test_reader_sees_a_new_value_once()
{
    Published<Weather> published;
    uint32_t seen = 0;
    Weather weather{};

    published.publish(Weather{25});

    TEST_ASSERT_TRUE(published.readIfChanged(weather, seen));
    TEST_ASSERT_EQUAL_INT(25, weather.degrees);
    TEST_ASSERT_FALSE(published.readIfChanged(weather, seen));
}

void test_nothing_published_means_nothing_changed()
{
    Published<Weather> published;
    uint32_t seen = 0;
    Weather weather{};

    TEST_ASSERT_FALSE(published.readIfChanged(weather, seen));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_reader_sees_a_new_value_once);
    RUN_TEST(test_nothing_published_means_nothing_changed);
    return UNITY_END();
}
