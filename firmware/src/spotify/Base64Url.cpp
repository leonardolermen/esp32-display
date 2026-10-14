#include "spotify/Base64Url.h"

namespace {

constexpr const char* ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

}

namespace Base64Url {

std::string encode(const uint8_t* bytes, size_t length)
{
    std::string encoded;
    uint32_t buffer = 0;
    int bits = 0;

    for (size_t index = 0; index < length; index++) {
        buffer = (buffer << 8) | bytes[index];
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            encoded += ALPHABET[(buffer >> bits) & 0x3F];
        }
    }

    if (bits > 0) {
        encoded += ALPHABET[(buffer << (6 - bits)) & 0x3F];
    }
    return encoded;
}

}
