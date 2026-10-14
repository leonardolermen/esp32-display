#include <unity.h>

#include "spotify/PlayerApi.h"

void setUp()
{
}

void tearDown()
{
}

void test_poll_asks_for_podcast_episodes_too()
{
    TEST_ASSERT_EQUAL_STRING("https://api.spotify.com/v1/me/player?additional_types=track,episode",
        PlayerApi::pollUrl().c_str());
}

void test_commands_use_the_player_path_without_the_poll_query()
{
    TEST_ASSERT_EQUAL_STRING("https://api.spotify.com/v1/me/player/next",
        PlayerApi::commandUrl(CommandKind::Next, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("POST", PlayerApi::method(CommandKind::Next));
    TEST_ASSERT_EQUAL_STRING("PUT", PlayerApi::method(CommandKind::Pause));
}

void test_volume_carries_its_percent()
{
    TEST_ASSERT_EQUAL_STRING("https://api.spotify.com/v1/me/player/volume?volume_percent=42",
        PlayerApi::commandUrl(CommandKind::Volume, 42).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_poll_asks_for_podcast_episodes_too);
    RUN_TEST(test_commands_use_the_player_path_without_the_poll_query);
    RUN_TEST(test_volume_carries_its_percent);
    return UNITY_END();
}
