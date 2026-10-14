#include <unity.h>

#include <cstring>
#include <string>

#include "spotify/Pkce.h"

namespace {

constexpr const char* ALLOWED = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

}

void setUp()
{
}

void tearDown()
{
}

void test_rfc7636_example_challenge()
{
    std::string challenge = Pkce::challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");

    TEST_ASSERT_EQUAL_STRING("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM", challenge.c_str());
}

void test_verifier_has_64_characters_from_the_allowed_alphabet()
{
    uint8_t random[Pkce::VERIFIER_LENGTH];
    for (size_t index = 0; index < Pkce::VERIFIER_LENGTH; index++) {
        random[index] = static_cast<uint8_t>(index * 4);
    }

    std::string verifier = Pkce::verifier(random);

    TEST_ASSERT_EQUAL_size_t(64, verifier.size());
    TEST_ASSERT_EQUAL_INT('A', verifier[0]);
    for (char character : verifier) {
        TEST_ASSERT_NOT_NULL(strchr(ALLOWED, character));
    }
}

void test_byte_65_is_the_last_letter_of_the_alphabet()
{
    uint8_t random[Pkce::VERIFIER_LENGTH] = {};
    random[1] = 65;

    TEST_ASSERT_EQUAL_INT('~', Pkce::verifier(random)[1]);
}

void test_hex_is_lowercase_and_padded()
{
    const uint8_t bytes[] = {0x00, 0x9f, 0xff};

    TEST_ASSERT_EQUAL_STRING("009fff", Pkce::hex(bytes, sizeof(bytes)).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_rfc7636_example_challenge);
    RUN_TEST(test_verifier_has_64_characters_from_the_allowed_alphabet);
    RUN_TEST(test_byte_65_is_the_last_letter_of_the_alphabet);
    RUN_TEST(test_hex_is_lowercase_and_padded);
    return UNITY_END();
}
