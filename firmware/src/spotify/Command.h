#pragma once

#include <cstdint>

enum class CommandKind : uint8_t {
    Play,
    Pause,
    Next,
    Previous,
    Volume,
};

struct Command {
    CommandKind kind;
    uint8_t volume = 0;
};
