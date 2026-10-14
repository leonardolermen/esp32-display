#include <unity.h>

#include <vector>

#include "spotify/CoverScaler.h"

namespace {

std::vector<uint16_t> scaled(const std::vector<uint16_t>& source, int width, int height)
{
    std::vector<uint16_t> target(200 * 200, 0xAAAA);
    CoverScaler::downscale(source.data(), width, height, target.data(), 200);
    return target;
}

void assertEverywhere(const std::vector<uint16_t>& target, uint16_t expected)
{
    for (uint16_t pixel : target) {
        TEST_ASSERT_EQUAL_HEX16(expected, pixel);
    }
}

}

void setUp()
{
}

void tearDown()
{
}

void test_uniform_300_keeps_its_color()
{
    assertEverywhere(scaled(std::vector<uint16_t>(300 * 300, 0x1234), 300, 300), 0x1234);
}

void test_alternating_columns_average_to_gray()
{
    std::vector<uint16_t> stripes(400 * 400);
    for (int y = 0; y < 400; y++) {
        for (int x = 0; x < 400; x++) {
            stripes[y * 400 + x] = x % 2 == 0 ? 0x0000 : 0xFFFF;
        }
    }

    assertEverywhere(scaled(stripes, 400, 400), 0x7BEF);
}

void test_large_640_cover_fits()
{
    assertEverywhere(scaled(std::vector<uint16_t>(640 * 640, 0xF800), 640, 640), 0xF800);
}

void test_small_64_cover_is_enlarged()
{
    assertEverywhere(scaled(std::vector<uint16_t>(64 * 64, 0x07E0), 64, 64), 0x07E0);
}

void test_jpeg_scale_keeps_the_decoded_cover_small()
{
    TEST_ASSERT_EQUAL_INT(0, CoverScaler::jpegScaleShift(300));
    TEST_ASSERT_EQUAL_INT(1, CoverScaler::jpegScaleShift(640));
    TEST_ASSERT_EQUAL_INT(2, CoverScaler::jpegScaleShift(1300));
    TEST_ASSERT_EQUAL_INT(3, CoverScaler::jpegScaleShift(3000));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_uniform_300_keeps_its_color);
    RUN_TEST(test_alternating_columns_average_to_gray);
    RUN_TEST(test_large_640_cover_fits);
    RUN_TEST(test_small_64_cover_is_enlarged);
    RUN_TEST(test_jpeg_scale_keeps_the_decoded_cover_small);
    return UNITY_END();
}
