#include <unity.h>

#include "spotify/PollSchedule.h"

void setUp()
{
}

void tearDown()
{
}

void test_disconnected_does_not_poll()
{
    TEST_ASSERT_FALSE(PollSchedule::intervalMillis(PlayerStatus::Disconnected, true, 0).has_value());
}

void test_playing_and_watched_is_fast()
{
    TEST_ASSERT_EQUAL_UINT32(3000, *PollSchedule::intervalMillis(PlayerStatus::Playing, true, 0));
}

void test_playing_in_the_background_is_slower()
{
    TEST_ASSERT_EQUAL_UINT32(15000, *PollSchedule::intervalMillis(PlayerStatus::Playing, false, 0));
}

void test_no_device_is_idle_even_when_watched()
{
    TEST_ASSERT_EQUAL_UINT32(30000, *PollSchedule::intervalMillis(PlayerStatus::NoDevice, true, 0));
}

void test_recent_pause_keeps_the_pace_and_long_pause_goes_idle()
{
    TEST_ASSERT_EQUAL_UINT32(3000, *PollSchedule::intervalMillis(PlayerStatus::Paused, true, 10000));
    TEST_ASSERT_EQUAL_UINT32(30000, *PollSchedule::intervalMillis(PlayerStatus::Paused, true, 70000));
}

void test_retry_after_is_in_seconds_with_a_safe_default()
{
    TEST_ASSERT_EQUAL_UINT32(12000, PollSchedule::retryAfterMillis("12"));
    TEST_ASSERT_EQUAL_UINT32(1000, PollSchedule::retryAfterMillis("0"));
    TEST_ASSERT_EQUAL_UINT32(30000, PollSchedule::retryAfterMillis(""));
    TEST_ASSERT_EQUAL_UINT32(30000, PollSchedule::retryAfterMillis("abc"));
}

void test_repeated_unauthorized_backs_off_up_to_a_minute()
{
    TEST_ASSERT_EQUAL_UINT32(1000, PollSchedule::unauthorizedBackoffMillis(1));
    TEST_ASSERT_EQUAL_UINT32(2000, PollSchedule::unauthorizedBackoffMillis(2));
    TEST_ASSERT_EQUAL_UINT32(8000, PollSchedule::unauthorizedBackoffMillis(4));
    TEST_ASSERT_EQUAL_UINT32(60000, PollSchedule::unauthorizedBackoffMillis(30));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_disconnected_does_not_poll);
    RUN_TEST(test_playing_and_watched_is_fast);
    RUN_TEST(test_playing_in_the_background_is_slower);
    RUN_TEST(test_no_device_is_idle_even_when_watched);
    RUN_TEST(test_recent_pause_keeps_the_pace_and_long_pause_goes_idle);
    RUN_TEST(test_retry_after_is_in_seconds_with_a_safe_default);
    RUN_TEST(test_repeated_unauthorized_backs_off_up_to_a_minute);
    return UNITY_END();
}
