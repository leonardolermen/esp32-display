#include <unity.h>

#include "setup/SetupProgress.h"

void setUp()
{
}

void tearDown()
{
}

void test_without_wifi_shows_the_hotspot_qr()
{
    SetupState state = SetupProgress::compute(false, "", false);

    TEST_ASSERT_EQUAL(SetupStep::WaitingWifi, state.step);
    TEST_ASSERT_EQUAL_STRING("WIFI:S:Claudinho;T:nopass;;", state.qrText.c_str());
}

void test_with_wifi_shows_the_page_qr_and_ip()
{
    SetupState state = SetupProgress::compute(true, "192.168.0.42", false);

    TEST_ASSERT_EQUAL(SetupStep::WaitingAdmin, state.step);
    TEST_ASSERT_EQUAL_STRING("http://claudinho.local", state.qrText.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.0.42", state.ip.c_str());
}

void test_with_password_setup_is_done()
{
    SetupState state = SetupProgress::compute(true, "192.168.0.42", true);

    TEST_ASSERT_EQUAL(SetupStep::Done, state.step);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_without_wifi_shows_the_hotspot_qr);
    RUN_TEST(test_with_wifi_shows_the_page_qr_and_ip);
    RUN_TEST(test_with_password_setup_is_done);
    return UNITY_END();
}
