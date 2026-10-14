#pragma once

#include <cstdint>
#include <string>

#include "spotify/Command.h"

namespace PlayerApi {

std::string pollUrl();
const char* method(CommandKind kind);
std::string commandUrl(CommandKind kind, uint8_t volume);

}
