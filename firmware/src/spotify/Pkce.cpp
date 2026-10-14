#include "spotify/Pkce.h"

#include <cstdio>

#include "spotify/Base64Url.h"
#include "spotify/Sha256.h"

namespace {

// Alfabeto que a RFC 7636 permite no code_verifier. O resto da divisão por 66 tem viés desprezível para 64 bytes.
constexpr const char* VERIFIER_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
constexpr size_t ALPHABET_SIZE = 66;

}

namespace Pkce {

std::string verifier(const uint8_t (&random)[VERIFIER_LENGTH])
{
    std::string text;
    for (uint8_t byte : random) {
        text += VERIFIER_ALPHABET[byte % ALPHABET_SIZE];
    }
    return text;
}

std::string challenge(const std::string& verifier)
{
    std::array<uint8_t, 32> digest = Sha256::digest(verifier);
    return Base64Url::encode(digest.data(), digest.size());
}

std::string hex(const uint8_t* bytes, size_t length)
{
    std::string text;
    char buffer[3];
    for (size_t index = 0; index < length; index++) {
        snprintf(buffer, sizeof(buffer), "%02x", bytes[index]);
        text += buffer;
    }
    return text;
}

}
