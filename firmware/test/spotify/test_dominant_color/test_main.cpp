#include <unity.h>

#include <vector>

#include "spotify/DominantColor.h"

void setUp()
{
}

void tearDown()
{
}

void test_red_cover_gives_a_darker_red()
{
    std::vector<uint16_t> red(100, 0xF800);

    uint32_t color = DominantColor::of(red.data(), red.size());

    TEST_ASSERT_TRUE((color >> 16) > 0);
    TEST_ASSERT_EQUAL_UINT32(0, (color >> 8) & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(0, color & 0xFF);
    TEST_ASSERT_TRUE(DominantColor::luminance(color) <= DominantColor::MAX_LUMINANCE);
}

void test_black_and_white_only_falls_back()
{
    std::vector<uint16_t> pixels(100, 0x0000);
    for (size_t index = 0; index < pixels.size(); index += 2) {
        pixels[index] = 0xFFFF;
    }

    TEST_ASSERT_EQUAL_HEX32(DominantColor::FALLBACK, DominantColor::of(pixels.data(), pixels.size()));
}

void test_the_most_common_color_wins()
{
    std::vector<uint16_t> pixels(100, 0x07E0);
    for (size_t index = 0; index < 70; index++) {
        pixels[index] = 0x001F;
    }

    uint32_t color = DominantColor::of(pixels.data(), pixels.size());

    TEST_ASSERT_TRUE((color & 0xFF) > ((color >> 8) & 0xFF));
}

void test_a_dark_color_is_kept_as_is()
{
    std::vector<uint16_t> pixels(100, 0x2188);

    TEST_ASSERT_EQUAL_HEX32(0x213042, DominantColor::of(pixels.data(), pixels.size()));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_red_cover_gives_a_darker_red);
    RUN_TEST(test_black_and_white_only_falls_back);
    RUN_TEST(test_the_most_common_color_wins);
    RUN_TEST(test_a_dark_color_is_kept_as_is);
    return UNITY_END();
}
