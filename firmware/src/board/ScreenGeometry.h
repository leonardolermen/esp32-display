#pragma once

#include <cstdint>

constexpr uint16_t PANEL_WIDTH = 320;
constexpr uint16_t PANEL_HEIGHT = 480;
constexpr uint16_t SCREEN_WIDTH = 480;
constexpr uint16_t SCREEN_HEIGHT = 320;

struct PanelPoint {
    uint16_t x;
    uint16_t y;
};

struct ScreenPoint {
    uint16_t x;
    uint16_t y;
};
