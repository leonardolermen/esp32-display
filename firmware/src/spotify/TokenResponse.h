#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct TokenResult {
    bool ok = false;
    std::string accessToken;
    std::optional<std::string> refreshToken;
    uint32_t expiresInSeconds = 0;
    bool invalidGrant = false;
    std::string error;
};

namespace TokenResponse {

TokenResult parse(std::string_view json);

}
