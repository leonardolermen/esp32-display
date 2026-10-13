#include <unity.h>

#include <vector>

#include "board/ScreenRotation.h"

namespace {

constexpr uint16_t MARK = 0xF800;
constexpr uint16_t MARK_FOR_PANEL = 0x00F8;

PanelPoint whereTheMarkLands(const ScreenRotation& rotation, ScreenPoint marked)
{
    std::vector<uint16_t> landscape(SCREEN_WIDTH * SCREEN_HEIGHT, 0);
    std::vector<uint16_t> portrait(PANEL_WIDTH * PANEL_HEIGHT, 0);
    landscape[marked.y * SCREEN_WIDTH + marked.x] = MARK;

    rotation.rotate(landscape.data(), portrait.data());

    for (size_t index = 0; index < portrait.size(); index++) {
        if (portrait[index] == MARK_FOR_PANEL) {
            return PanelPoint{static_cast<uint16_t>(index % PANEL_WIDTH), static_cast<uint16_t>(index / PANEL_WIDTH)};
        }
    }
    TEST_FAIL_MESSAGE("the mark is missing from the rotated frame");
    return PanelPoint{0, 0};
}

void assertTouchLandsWhereDrawn(const ScreenRotation& rotation, ScreenPoint drawn)
{
    PanelPoint onPanel = whereTheMarkLands(rotation, drawn);

    ScreenPoint touched = rotation.toScreen(onPanel);

    TEST_ASSERT_EQUAL_UINT16(drawn.x, touched.x);
    TEST_ASSERT_EQUAL_UINT16(drawn.y, touched.y);
}

}

void setUp()
{
}

void tearDown()
{
}

void test_rotation90_puts_screen_origin_at_panel_top_right()
{
    PanelPoint onPanel = whereTheMarkLands(Rotation90(), ScreenPoint{0, 0});

    TEST_ASSERT_EQUAL_UINT16(PANEL_WIDTH - 1, onPanel.x);
    TEST_ASSERT_EQUAL_UINT16(0, onPanel.y);
}

void test_rotation270_puts_screen_origin_at_panel_bottom_left()
{
    PanelPoint onPanel = whereTheMarkLands(Rotation270(), ScreenPoint{0, 0});

    TEST_ASSERT_EQUAL_UINT16(0, onPanel.x);
    TEST_ASSERT_EQUAL_UINT16(PANEL_HEIGHT - 1, onPanel.y);
}

void test_rotation_hands_the_panel_big_endian_pixels()
{
    std::vector<uint16_t> landscape(SCREEN_WIDTH * SCREEN_HEIGHT, 0x1234);
    std::vector<uint16_t> portrait(PANEL_WIDTH * PANEL_HEIGHT, 0);

    Rotation90().rotate(landscape.data(), portrait.data());
    TEST_ASSERT_EACH_EQUAL_UINT16(0x3412, portrait.data(), portrait.size());

    Rotation270().rotate(landscape.data(), portrait.data());
    TEST_ASSERT_EACH_EQUAL_UINT16(0x3412, portrait.data(), portrait.size());
}

void test_rotation90_touch_lands_where_it_was_drawn()
{
    Rotation90 rotation;

    assertTouchLandsWhereDrawn(rotation, ScreenPoint{0, 0});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{123, 45});
}

void test_rotation270_touch_lands_where_it_was_drawn()
{
    Rotation270 rotation;

    assertTouchLandsWhereDrawn(rotation, ScreenPoint{0, 0});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1});
    assertTouchLandsWhereDrawn(rotation, ScreenPoint{123, 45});
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_rotation90_puts_screen_origin_at_panel_top_right);
    RUN_TEST(test_rotation270_puts_screen_origin_at_panel_bottom_left);
    RUN_TEST(test_rotation_hands_the_panel_big_endian_pixels);
    RUN_TEST(test_rotation90_touch_lands_where_it_was_drawn);
    RUN_TEST(test_rotation270_touch_lands_where_it_was_drawn);
    return UNITY_END();
}
