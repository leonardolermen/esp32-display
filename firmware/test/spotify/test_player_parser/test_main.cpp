#include <unity.h>

#include "spotify/PlayerParser.h"

namespace {

const char* PLAYING = R"({"device":{"id":"d1","name":"MacBook do Pai","type":"Computer","volume_percent":64},
 "progress_ms":42000,"is_playing":true,"currently_playing_type":"track",
 "item":{"name":"Águas de Março","duration_ms":212000,
   "artists":[{"name":"Elis Regina"},{"name":"Tom Jobim"}],
   "album":{"id":"alb1","name":"Elis & Tom","images":[{"url":"https://i.scdn.co/640","width":640},
     {"url":"https://i.scdn.co/300","width":300},{"url":"https://i.scdn.co/64","width":64}]}}})";

const char* PAUSED_WITHOUT_VOLUME = R"({"device":{"name":"Echo","volume_percent":null},"progress_ms":10,
 "is_playing":false,"item":{"name":"Faixa","duration_ms":1000,"artists":[{"name":"A"}],
 "album":{"id":"alb2","name":"B","images":[]}}})";

const char* ADVERTISEMENT = R"({"device":{"name":"Celular","volume_percent":30},"progress_ms":0,"is_playing":true,
 "currently_playing_type":"ad","item":null})";

const char* EPISODE = R"({"device":{"name":"Celular","volume_percent":30},"progress_ms":5000,"is_playing":true,
 "item":{"id":"ep1","name":"Episódio 12","duration_ms":3600000,"show":{"name":"Podcast X"},
   "images":[{"url":"https://i.scdn.co/ep640","width":640},{"url":"https://i.scdn.co/ep300","width":300}]}})";

}

void setUp()
{
}

void tearDown()
{
}

void test_playing_track_fills_every_field()
{
    std::optional<PlayerState> state = PlayerParser::parse(PLAYING, 1234);

    TEST_ASSERT_TRUE(state.has_value());
    TEST_ASSERT_EQUAL(PlayerStatus::Playing, state->status);
    TEST_ASSERT_TRUE(state->hasItem);
    TEST_ASSERT_EQUAL_STRING("Águas de Março", state->track.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Elis Regina, Tom Jobim", state->track.artists.c_str());
    TEST_ASSERT_EQUAL_STRING("Elis & Tom", state->track.album.c_str());
    TEST_ASSERT_EQUAL_STRING("alb1", state->track.albumId.c_str());
    TEST_ASSERT_EQUAL_STRING("https://i.scdn.co/300", state->track.coverUrl.c_str());
    TEST_ASSERT_EQUAL_UINT32(212000, state->track.durationMs);
    TEST_ASSERT_EQUAL_UINT32(42000, state->progressMs);
    TEST_ASSERT_EQUAL_UINT32(1234, state->sampledAtMillis);
    TEST_ASSERT_EQUAL_STRING("MacBook do Pai", state->device.c_str());
    TEST_ASSERT_EQUAL_INT(64, *state->volume);
}

void test_paused_without_volume()
{
    std::optional<PlayerState> state = PlayerParser::parse(PAUSED_WITHOUT_VOLUME, 0);

    TEST_ASSERT_EQUAL(PlayerStatus::Paused, state->status);
    TEST_ASSERT_FALSE(state->volume.has_value());
    TEST_ASSERT_EQUAL_STRING("", state->track.coverUrl.c_str());
}

void test_advertisement_has_no_item()
{
    std::optional<PlayerState> state = PlayerParser::parse(ADVERTISEMENT, 0);

    TEST_ASSERT_TRUE(state.has_value());
    TEST_ASSERT_EQUAL(PlayerStatus::Playing, state->status);
    TEST_ASSERT_FALSE(state->hasItem);
}

void test_podcast_episode_uses_the_show_and_its_images()
{
    std::optional<PlayerState> state = PlayerParser::parse(EPISODE, 0);

    TEST_ASSERT_TRUE(state->hasItem);
    TEST_ASSERT_EQUAL_STRING("Podcast X", state->track.artists.c_str());
    TEST_ASSERT_EQUAL_STRING("ep1", state->track.albumId.c_str());
    TEST_ASSERT_EQUAL_STRING("https://i.scdn.co/ep300", state->track.coverUrl.c_str());
}

void test_broken_json_is_nothing()
{
    TEST_ASSERT_FALSE(PlayerParser::parse("{oops", 0).has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_playing_track_fills_every_field);
    RUN_TEST(test_paused_without_volume);
    RUN_TEST(test_advertisement_has_no_item);
    RUN_TEST(test_podcast_episode_uses_the_show_and_its_images);
    RUN_TEST(test_broken_json_is_nothing);
    return UNITY_END();
}
