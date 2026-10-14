#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "spotify/PlayerState.h"

namespace PlayerParser {

std::optional<PlayerState> parse(std::string_view json, uint32_t nowMillis);

}
