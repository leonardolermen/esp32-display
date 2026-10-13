#include <unity.h>

#include <set>

#include "photos/ShuffleOrder.h"

void setUp()
{
}

void tearDown()
{
}

void test_visits_every_photo_once_per_round()
{
    ShuffleOrder order(5, 42);
    std::set<size_t> seen;

    for (int round = 0; round < 5; round++) {
        seen.insert(*order.next());
    }

    TEST_ASSERT_EQUAL_size_t(5, seen.size());
}

void test_next_round_does_not_repeat_the_last_photo()
{
    for (uint32_t seed = 1; seed < 50; seed++) {
        ShuffleOrder order(3, seed);
        order.next();
        order.next();
        size_t last = *order.next();

        TEST_ASSERT_NOT_EQUAL(last, *order.next());
    }
}

void test_no_photos_means_nothing_to_show()
{
    ShuffleOrder order(0, 1);

    TEST_ASSERT_FALSE(order.next().has_value());
}

void test_a_single_photo_repeats()
{
    ShuffleOrder order(1, 1);

    TEST_ASSERT_EQUAL_size_t(0, *order.next());
    TEST_ASSERT_EQUAL_size_t(0, *order.next());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_visits_every_photo_once_per_round);
    RUN_TEST(test_next_round_does_not_repeat_the_last_photo);
    RUN_TEST(test_no_photos_means_nothing_to_show);
    RUN_TEST(test_a_single_photo_repeats);
    return UNITY_END();
}
