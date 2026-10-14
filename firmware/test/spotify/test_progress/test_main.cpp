#include <unity.h>

#include "spotify/Progress.h"

namespace {

PlayerState sampled(PlayerStatus status, uint32_t progressMs, uint32_t sampledAt)
{
    PlayerState state;
    state.status = status;
    state.hasItem = true;
    state.progressMs = progressMs;
    state.sampledAtMillis = sampledAt;
    state.track.durationMs = 200000;
    return state;
}

}

void setUp()
{
}

void tearDown()
{
}

void test_playing_advances_with_the_clock()
{
    TEST_ASSERT_EQUAL_UINT32(3500, Progress::estimate(sampled(PlayerStatus::Playing, 1000, 5000), 7500));
}

void test_paused_stays_where_it_was()
{
    TEST_ASSERT_EQUAL_UINT32(1000, Progress::estimate(sampled(PlayerStatus::Paused, 1000, 5000), 99999));
}

void test_never_passes_the_duration()
{
    TEST_ASSERT_EQUAL_UINT32(200000, Progress::estimate(sampled(PlayerStatus::Playing, 199000, 0), 5000));
}

void test_survives_the_millis_wraparound()
{
    PlayerState state = sampled(PlayerStatus::Playing, 1000, 0xFFFFF000u);

    TEST_ASSERT_EQUAL_UINT32(1000 + 0x1800, Progress::estimate(state, 0x00000800u));
}

void test_clock_is_minutes_and_two_digit_seconds()
{
    TEST_ASSERT_EQUAL_STRING("0:00", Progress::clock(0).c_str());
    TEST_ASSERT_EQUAL_STRING("1:01", Progress::clock(61000).c_str());
    TEST_ASSERT_EQUAL_STRING("9:59", Progress::clock(599999).c_str());
    TEST_ASSERT_EQUAL_STRING("60:00", Progress::clock(3600000).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_playing_advances_with_the_clock);
    RUN_TEST(test_paused_stays_where_it_was);
    RUN_TEST(test_never_passes_the_duration);
    RUN_TEST(test_survives_the_millis_wraparound);
    RUN_TEST(test_clock_is_minutes_and_two_digit_seconds);
    return UNITY_END();
}
