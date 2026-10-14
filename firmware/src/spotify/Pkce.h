#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Pkce {

constexpr size_t VERIFIER_LENGTH = 64;

std::string verifier(const uint8_t (&random)[VERIFIER_LENGTH]);
std::string challenge(const std::string& verifier);
std::string hex(const uint8_t* bytes, size_t length);

}
