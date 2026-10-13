#include <unity.h>

#include "navigation/PageIndex.h"

void setUp()
{
}

void tearDown()
{
}

void test_starts_at_home()
{
    PageIndex index(3);

    TEST_ASSERT_EQUAL_size_t(0, index.current());
}

void test_next_stops_at_the_last_page()
{
    PageIndex index(3);

    TEST_ASSERT_TRUE(index.next());
    TEST_ASSERT_TRUE(index.next());
    TEST_ASSERT_FALSE(index.next());
    TEST_ASSERT_EQUAL_size_t(2, index.current());
}

void test_previous_stops_at_home()
{
    PageIndex index(3);

    TEST_ASSERT_FALSE(index.previous());
    TEST_ASSERT_EQUAL_size_t(0, index.current());
}

void test_jump_to_clamps_to_the_last_page()
{
    PageIndex index(3);

    index.jumpTo(9);

    TEST_ASSERT_EQUAL_size_t(2, index.current());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_at_home);
    RUN_TEST(test_next_stops_at_the_last_page);
    RUN_TEST(test_previous_stops_at_home);
    RUN_TEST(test_jump_to_clamps_to_the_last_page);
    return UNITY_END();
}
