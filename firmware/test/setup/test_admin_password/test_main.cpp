#include <unity.h>

#include "setup/AdminPassword.h"

void setUp()
{
}

void tearDown()
{
}

void test_accepts_eight_characters()
{
    TEST_ASSERT_TRUE(AdminPassword::parse("12345678").has_value());
}

void test_refuses_seven_characters()
{
    TEST_ASSERT_FALSE(AdminPassword::parse("1234567").has_value());
}

void test_refuses_sixty_five_characters()
{
    TEST_ASSERT_FALSE(AdminPassword::parse(std::string(65, 'a')).has_value());
}

void test_trims_the_ends()
{
    std::optional<AdminPassword> password = AdminPassword::parse("  senha123  ");

    TEST_ASSERT_TRUE(password.has_value());
    TEST_ASSERT_EQUAL_STRING("senha123", password->value().c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_eight_characters);
    RUN_TEST(test_refuses_seven_characters);
    RUN_TEST(test_refuses_sixty_five_characters);
    RUN_TEST(test_trims_the_ends);
    return UNITY_END();
}
