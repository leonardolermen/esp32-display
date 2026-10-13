#include <unity.h>

#include "photos/UploadCheck.h"

void setUp()
{
}

void tearDown()
{
}

void test_too_big()
{
    std::optional<std::string> refusal = UploadCheck::refuse(300 * 1024, 8 * 1024 * 1024, 10);

    TEST_ASSERT_TRUE(refusal.has_value());
    TEST_ASSERT_EQUAL_STRING("foto maior que 200 KB", refusal->c_str());
}

void test_flash_nearly_full()
{
    std::optional<std::string> refusal = UploadCheck::refuse(100 * 1024, 600 * 1024, 10);

    TEST_ASSERT_TRUE(refusal.has_value());
    TEST_ASSERT_EQUAL_STRING("memória cheia", refusal->c_str());
}

void test_too_many_photos()
{
    std::optional<std::string> refusal = UploadCheck::refuse(100 * 1024, 8 * 1024 * 1024, 200);

    TEST_ASSERT_TRUE(refusal.has_value());
    TEST_ASSERT_EQUAL_STRING("limite de 200 fotos", refusal->c_str());
}

void test_accepts_a_normal_photo()
{
    TEST_ASSERT_FALSE(UploadCheck::refuse(100 * 1024, 2 * 1024 * 1024, 10).has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_too_big);
    RUN_TEST(test_flash_nearly_full);
    RUN_TEST(test_too_many_photos);
    RUN_TEST(test_accepts_a_normal_photo);
    return UNITY_END();
}
