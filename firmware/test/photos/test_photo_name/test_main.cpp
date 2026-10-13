#include <unity.h>

#include "photos/PhotoName.h"

void setUp()
{
}

void tearDown()
{
}

void test_accepts_the_canonical_form()
{
    std::optional<PhotoName> name = PhotoName::parse("p0001.jpg");

    TEST_ASSERT_TRUE(name.has_value());
    TEST_ASSERT_EQUAL_STRING("/photos/p0001.jpg", name->path().c_str());
}

void test_refuses_anything_that_is_not_the_canonical_form()
{
    TEST_ASSERT_FALSE(PhotoName::parse("../x").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("a/b.jpg").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("p0001.png").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("P0001.jpg").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("p1.jpg").has_value());
    TEST_ASSERT_FALSE(PhotoName::parse("").has_value());
}

void test_from_sequence_pads_to_four_digits()
{
    TEST_ASSERT_EQUAL_STRING("p0007.jpg", PhotoName::fromSequence(7).fileName().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_the_canonical_form);
    RUN_TEST(test_refuses_anything_that_is_not_the_canonical_form);
    RUN_TEST(test_from_sequence_pads_to_four_digits);
    return UNITY_END();
}
