#include <unity.h>

#include "spotify/AuthFlow.h"

namespace {

const std::string NONCE = "0123456789abcdef0123456789abcdef";
const std::string STATE = "192.168.1.34:80." + NONCE;

}

void setUp()
{
}

void tearDown()
{
}

void test_url_encode_keeps_unreserved_and_escapes_the_rest()
{
    TEST_ASSERT_EQUAL_STRING("a%20b%2Fc%3A~-._", AuthFlow::urlEncode("a b/c:~-._").c_str());
}

void test_authorize_url_has_every_parameter()
{
    std::string url = AuthFlow::authorizeUrl("CID", "CHALLENGE", STATE);

    std::string expected = "https://accounts.spotify.com/authorize?response_type=code&client_id=CID"
        "&scope=user-read-playback-state%20user-modify-playback-state%20user-read-currently-playing"
        "&redirect_uri=https%3A%2F%2Fleonardolermen.github.io%2Fesp32-display%2Fcallback%2F"
        "&code_challenge_method=S256&code_challenge=CHALLENGE&state=192.168.1.34%3A80." + NONCE;
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), url.c_str());
}

void test_state_round_trip()
{
    std::string state = AuthFlow::state("192.168.1.34", 80, NONCE);
    std::optional<AuthFlow::HostAndNonce> parsed = AuthFlow::parseState(state);

    TEST_ASSERT_EQUAL_STRING(STATE.c_str(), state.c_str());
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL_STRING("192.168.1.34", parsed->host.c_str());
    TEST_ASSERT_EQUAL_UINT16(80, parsed->port);
    TEST_ASSERT_EQUAL_STRING(NONCE.c_str(), parsed->nonce.c_str());
}

void test_malformed_states_are_refused()
{
    TEST_ASSERT_FALSE(AuthFlow::parseState("").has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34").has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34:abc." + NONCE).has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34:80.short").has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState(":80." + NONCE).has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34:99999." + NONCE).has_value());
}

void test_right_nonce_inside_ten_minutes_is_accepted()
{
    AuthFlow::Pending pending{"verifier", NONCE, 1000};

    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Accepted, AuthFlow::check(pending, STATE, 1000 + 599999));
}

void test_old_callback_is_expired()
{
    AuthFlow::Pending pending{"verifier", NONCE, 1000};

    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Expired, AuthFlow::check(pending, STATE, 1000 + 600000));
}

void test_callback_from_another_login_is_a_mismatch()
{
    AuthFlow::Pending other{"verifier", "ffffffffffffffffffffffffffffffff", 1000};
    AuthFlow::Pending none{};

    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Mismatch, AuthFlow::check(other, STATE, 2000));
    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Mismatch, AuthFlow::check(none, STATE, 2000));
    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Mismatch, AuthFlow::check(other, "lixo", 2000));
}

void test_pasted_url_gives_code_and_state()
{
    std::optional<AuthFlow::Callback> callback = AuthFlow::fromPastedUrl(
        "https://leonardolermen.github.io/esp32-display/callback/?code=AQB%2Dx&state=192.168.1.34%3A80." + NONCE);

    TEST_ASSERT_TRUE(callback.has_value());
    TEST_ASSERT_EQUAL_STRING("AQB-x", callback->code.c_str());
    TEST_ASSERT_EQUAL_STRING(STATE.c_str(), callback->state.c_str());
}

void test_pasted_url_without_code_is_nothing()
{
    TEST_ASSERT_FALSE(AuthFlow::fromPastedUrl("https://example.com/callback/?state=x").has_value());
    TEST_ASSERT_FALSE(AuthFlow::fromPastedUrl("sem interrogação").has_value());
}

void test_token_request_body()
{
    std::string body = AuthFlow::tokenRequestBody("CID", "AQB", "VER");

    TEST_ASSERT_EQUAL_STRING("grant_type=authorization_code&code=AQB"
        "&redirect_uri=https%3A%2F%2Fleonardolermen.github.io%2Fesp32-display%2Fcallback%2F"
        "&client_id=CID&code_verifier=VER", body.c_str());
}

void test_refresh_request_body()
{
    TEST_ASSERT_EQUAL_STRING("grant_type=refresh_token&refresh_token=R%2B1&client_id=CID",
        AuthFlow::refreshRequestBody("CID", "R+1").c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_url_encode_keeps_unreserved_and_escapes_the_rest);
    RUN_TEST(test_authorize_url_has_every_parameter);
    RUN_TEST(test_state_round_trip);
    RUN_TEST(test_malformed_states_are_refused);
    RUN_TEST(test_right_nonce_inside_ten_minutes_is_accepted);
    RUN_TEST(test_old_callback_is_expired);
    RUN_TEST(test_callback_from_another_login_is_a_mismatch);
    RUN_TEST(test_pasted_url_gives_code_and_state);
    RUN_TEST(test_pasted_url_without_code_is_nothing);
    RUN_TEST(test_token_request_body);
    RUN_TEST(test_refresh_request_body);
    return UNITY_END();
}
