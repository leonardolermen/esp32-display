#include <unity.h>

#include "spotify/TokenResponse.h"

void setUp()
{
}

void tearDown()
{
}

void test_first_exchange_brings_both_tokens()
{
    TokenResult result = TokenResponse::parse(
        R"({"access_token":"AT","token_type":"Bearer","expires_in":3600,"refresh_token":"RT","scope":"x"})");

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL_STRING("AT", result.accessToken.c_str());
    TEST_ASSERT_EQUAL_STRING("RT", result.refreshToken->c_str());
    TEST_ASSERT_EQUAL_UINT32(3600, result.expiresInSeconds);
}

void test_refresh_without_a_new_refresh_token_keeps_it_absent()
{
    TokenResult result = TokenResponse::parse(R"({"access_token":"AT2","token_type":"Bearer","expires_in":3600})");

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_FALSE(result.refreshToken.has_value());
}

void test_revoked_refresh_token_is_an_invalid_grant()
{
    TokenResult result = TokenResponse::parse(R"({"error":"invalid_grant","error_description":"Refresh token revoked"})");

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_TRUE(result.invalidGrant);
}

void test_other_errors_are_not_invalid_grant()
{
    TokenResult result = TokenResponse::parse(R"({"error":"invalid_client"})");

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_FALSE(result.invalidGrant);
    TEST_ASSERT_EQUAL_STRING("invalid_client", result.error.c_str());
}

void test_broken_json_is_not_ok()
{
    TEST_ASSERT_FALSE(TokenResponse::parse("<html>").ok);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_exchange_brings_both_tokens);
    RUN_TEST(test_refresh_without_a_new_refresh_token_keeps_it_absent);
    RUN_TEST(test_revoked_refresh_token_is_an_invalid_grant);
    RUN_TEST(test_other_errors_are_not_invalid_grant);
    RUN_TEST(test_broken_json_is_not_ok);
    return UNITY_END();
}
