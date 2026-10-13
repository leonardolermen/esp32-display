#include <unity.h>

#include "board/AxsTouchReport.h"

void setUp()
{
}

void tearDown()
{
}

void test_reads_one_touch_point()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x01, 0x00, 0x64, 0x01, 0x2C, 0x00, 0x00};

    std::optional<PanelPoint> point = AxsTouchReport::parse(report);

    TEST_ASSERT_TRUE(point.has_value());
    TEST_ASSERT_EQUAL_UINT16(100, point->x);
    TEST_ASSERT_EQUAL_UINT16(300, point->y);
}

void test_ignores_event_bits_in_the_high_nibble()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x01, 0x80, 0x64, 0x41, 0x2C, 0x00, 0x00};

    std::optional<PanelPoint> point = AxsTouchReport::parse(report);

    TEST_ASSERT_TRUE(point.has_value());
    TEST_ASSERT_EQUAL_UINT16(100, point->x);
    TEST_ASSERT_EQUAL_UINT16(300, point->y);
}

void test_no_finger_means_no_point()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x00, 0x00, 0x64, 0x01, 0x2C, 0x00, 0x00};

    TEST_ASSERT_FALSE(AxsTouchReport::parse(report).has_value());
}

void test_more_points_than_requested_is_bus_noise()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x02, 0x00, 0x64, 0x01, 0x2C, 0x00, 0x00};

    TEST_ASSERT_FALSE(AxsTouchReport::parse(report).has_value());
}

void test_point_outside_the_panel_is_rejected()
{
    const uint8_t report[AxsTouchReport::LENGTH] = {0x00, 0x01, 0x01, 0x40, 0x01, 0x2C, 0x00, 0x00};

    TEST_ASSERT_FALSE(AxsTouchReport::parse(report).has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_reads_one_touch_point);
    RUN_TEST(test_ignores_event_bits_in_the_high_nibble);
    RUN_TEST(test_no_finger_means_no_point);
    RUN_TEST(test_more_points_than_requested_is_bus_noise);
    RUN_TEST(test_point_outside_the_panel_is_rejected);
    return UNITY_END();
}
