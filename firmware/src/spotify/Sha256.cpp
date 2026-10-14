#include "spotify/Sha256.h"

// Nomes de variável seguem as letras da FIPS 180-4 (a…h, W, K) de propósito: quem revisa confere linha a linha
// contra o padrão.

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr uint32_t INITIAL_HASH[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

constexpr size_t BLOCK_BYTES = 64;
constexpr size_t LENGTH_OFFSET = 56;

uint32_t rotateRight(uint32_t value, int bits)
{
    return (value >> bits) | (value << (32 - bits));
}

void compress(uint32_t (&hash)[8], const uint8_t* block)
{
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = (uint32_t(block[4 * i]) << 24) | (uint32_t(block[4 * i + 1]) << 16) | (uint32_t(block[4 * i + 2]) << 8)
            | uint32_t(block[4 * i + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotateRight(W[i - 15], 7) ^ rotateRight(W[i - 15], 18) ^ (W[i - 15] >> 3);
        uint32_t s1 = rotateRight(W[i - 2], 17) ^ rotateRight(W[i - 2], 19) ^ (W[i - 2] >> 10);
        W[i] = W[i - 16] + s0 + W[i - 7] + s1;
    }

    uint32_t a = hash[0];
    uint32_t b = hash[1];
    uint32_t c = hash[2];
    uint32_t d = hash[3];
    uint32_t e = hash[4];
    uint32_t f = hash[5];
    uint32_t g = hash[6];
    uint32_t h = hash[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + K[i] + W[i];
        uint32_t S0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
}

}

namespace Sha256 {

std::array<uint8_t, 32> digest(const std::string& message)
{
    std::string padded = message;
    padded += static_cast<char>(0x80);
    while (padded.size() % BLOCK_BYTES != LENGTH_OFFSET) {
        padded += static_cast<char>(0);
    }
    uint64_t bits = static_cast<uint64_t>(message.size()) * 8;
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded += static_cast<char>((bits >> shift) & 0xff);
    }

    uint32_t hash[8];
    for (int i = 0; i < 8; i++) {
        hash[i] = INITIAL_HASH[i];
    }
    for (size_t offset = 0; offset < padded.size(); offset += BLOCK_BYTES) {
        compress(hash, reinterpret_cast<const uint8_t*>(padded.data() + offset));
    }

    std::array<uint8_t, 32> result{};
    for (int i = 0; i < 8; i++) {
        result[4 * i] = static_cast<uint8_t>(hash[i] >> 24);
        result[4 * i + 1] = static_cast<uint8_t>(hash[i] >> 16);
        result[4 * i + 2] = static_cast<uint8_t>(hash[i] >> 8);
        result[4 * i + 3] = static_cast<uint8_t>(hash[i]);
    }
    return result;
}

}
