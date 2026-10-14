#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// base64 com "-" e "_" no lugar de "+" e "/", sem "=": o formato que o PKCE pede para o code_challenge.
namespace Base64Url {

std::string encode(const uint8_t* bytes, size_t length);

}
