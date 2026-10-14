#pragma once

#include <cstdint>
#include <string>

#include "spotify/PlayerState.h"

namespace Progress {

uint32_t estimate(const PlayerState& state, uint32_t nowMillis);
std::string clock(uint32_t millis);

}
