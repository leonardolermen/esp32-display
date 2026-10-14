#pragma once

#include <array>
#include <cstdint>
#include <string>

// SHA-256 próprio na lógica pura: o PKCE precisa dele e assim ele é testado no PC contra os vetores da NIST.
namespace Sha256 {

std::array<uint8_t, 32> digest(const std::string& message);

}
