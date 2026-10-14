#include <unity.h>

#include <cstdio>
#include <string>

#include "spotify/Sha256.h"

namespace {

std::string hexOf(const std::array<uint8_t, 32>& digest)
{
    char buffer[65];
    for (size_t index = 0; index < digest.size(); index++) {
        snprintf(buffer + 2 * index, 3, "%02x", digest[index]);
    }
    return buffer;
}

}

void setUp()
{
}

void tearDown()
{
}

void test_abc_matches_the_nist_vector()
{
    TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        hexOf(Sha256::digest("abc")).c_str());
}

void test_empty_message_matches_the_nist_vector()
{
    TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        hexOf(Sha256::digest("")).c_str());
}

void test_message_that_needs_two_blocks()
{
    TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        hexOf(Sha256::digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_abc_matches_the_nist_vector);
    RUN_TEST(test_empty_message_matches_the_nist_vector);
    RUN_TEST(test_message_that_needs_two_blocks);
    return UNITY_END();
}
