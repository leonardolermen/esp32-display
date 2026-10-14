# Plano 3 — Spotify — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** página do Spotify (capa, música, progresso, controles, volume) e pílula "tocando agora" no Início, com
login PKCE feito uma vez pelo celular através de um relay estático no GitHub Pages.

**Architecture:** lógica pura em `firmware/src/spotify/` (SHA-256, PKCE, URL e `state` do login, parsing do player e do
token, agenda do polling, progresso, redução da capa, cor predominante) testada no PC. `support/device/Https` vira o
cliente HTTPS comum (extraído do `MarketFeed`). `spotify/device/` tem o login (`SpotifyAuth`), a tarefa que consulta e
manda comandos (`SpotifyClient`) e o download da capa (`CoverFetcher`). A UI lê `Published<PlayerState>` e
`Published<CoverArt>` e manda comandos por uma fila FreeRTOS. O relay é uma página estática em `relay/`.

**Tech Stack:** o que o projeto já usa (pioarduino 55.03.312 / Arduino core 3.3.12, LVGL 9.4, ArduinoJson 7.4.2,
JPEGDEC 1.8.2, ESPAsyncWebServer 3.7.10, Unity, `node --test`) e GitHub Actions para o Pages.

**Spec:** `docs/superpowers/specs/2026-10-14-spotify-design.md`

## Global Constraints

- Regras de `~/.claude/CLAUDE.md` e `CLAUDE.md`: nomes por extenso, chaves sempre, um comando por linha, até 120
  colunas, continuação com 4 espaços, linha em branco entre blocos, comentário só de porquê, sem cadeia de `if`.
- Exceção declarada: dentro de `Sha256.cpp` as variáveis seguem as letras da FIPS 180-4 (`a`…`h`, `W`, `K`), porque o
  leitor confere o código contra o padrão; o arquivo diz isso num comentário.
- Lógica pura fora de `device/`, `screen/`, `web/`, `storage/`; só `device/` e `screen/` falam com Arduino e LVGL.
- Só o loop do Arduino (núcleo 1) chama LVGL; tarefas publicam com `Published<T>`.
- Buffer > 4 KB vai para a PSRAM; `malloc` acima de 4096 bytes já vai sozinho (`CONFIG_SPIRAM_USE_MALLOC=y`,
  `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096`), então `std::vector` grande serve.
- Client ID padrão: `5ee610458f974597ba5c5f96eab539b5`.
- Redirect URI: `https://leonardolermen.github.io/esp32-display/callback/`.
- Escopos: `user-read-playback-state user-modify-playback-state user-read-currently-playing`.
- `state` = `<ip>:<porta>.<nonce>`, nonce de 16 bytes em hex minúsculo (32 caracteres); validade 10 min.
- Intervalos: 3 s (assistido e tocando), 15 s (tocando, não assistido), 30 s (sem aparelho ou pausado há mais de
  1 min); desconectado não consulta; `429` espera `Retry-After` (padrão 30 s).
- Volume com debounce de 300 ms; reconciliação 600 ms depois de um comando.
- Capa 200×200; cor escurecida até luminância ≤ 0,15; fallback `0x34322F`; transição de 400 ms.
- Ordem do deslize: **Início → Cripto → Spotify**.
- Testes espelham `src/`; commits em português terminando com
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build no Windows pelo PowerShell com `PLATFORMIO_CORE_DIR=C:\pio`.

## Review Focus

1. **Callback repetido, velho ou de outro login** — um link de 11 min atrás, ou de outro Claudinho, não pode gravar
   token. Testes em `test_auth_flow` (Task 2) e no relay (Task 3).
2. **Renovação sem `refresh_token` novo** — o antigo tem que continuar valendo. Teste em `test_token_response`
   (Task 4); `SpotifyAuth::remember` só grava quando veio um (Task 8).
3. **Anúncio ou podcast tocando** (`item` nulo, ou episódio sem `album`) — a página mostra algo, sem crash. Testes em
   `test_player_parser` (Task 4).
4. **Progresso perto da virada do `millis()` e além da duração** — nunca passa do total nem salta para trás. Teste em
   `test_progress` (Task 5).
5. **Capa maior (640 px) ou menor (64 px) que o esperado** — sempre 200×200 sem estourar buffer. Testes em
   `test_cover_scaler` (Task 6).

---

### Task 1: SHA-256, base64url e PKCE

**Files:**
- Create: `firmware/src/spotify/Sha256.h`, `firmware/src/spotify/Sha256.cpp`
- Create: `firmware/src/spotify/Base64Url.h`, `firmware/src/spotify/Base64Url.cpp`
- Create: `firmware/src/spotify/Pkce.h`, `firmware/src/spotify/Pkce.cpp`
- Test: `firmware/test/spotify/test_sha256/test_main.cpp`
- Test: `firmware/test/spotify/test_pkce/test_main.cpp`

**Interfaces:**
- Produces:
  - `namespace Sha256 { std::array<uint8_t, 32> digest(const std::string& message); }`
  - `namespace Base64Url { std::string encode(const uint8_t* bytes, size_t length); }` — sem `=`.
  - `namespace Pkce { constexpr size_t VERIFIER_LENGTH = 64; std::string verifier(const uint8_t (&random)[VERIFIER_LENGTH]);
    std::string challenge(const std::string& verifier); std::string hex(const uint8_t* bytes, size_t length); }`

- [ ] **Step 1: Testes**

`firmware/test/spotify/test_sha256/test_main.cpp`:

```cpp
#include <unity.h>

#include <cstdio>
#include <string>

#include "spotify/Sha256.h"

namespace {

std::string hexOf(const std::array<uint8_t, 32>& digest)
{
    char buffer[65];
    for (size_t index = 0; index < digest.size(); index++) {
        snprintf(buffer + 2 * index, 3, "%02x", digest[index]);
    }
    return buffer;
}

}

void setUp()
{
}

void tearDown()
{
}

void test_abc_matches_the_nist_vector()
{
    TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        hexOf(Sha256::digest("abc")).c_str());
}

void test_empty_message_matches_the_nist_vector()
{
    TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        hexOf(Sha256::digest("")).c_str());
}

void test_message_that_needs_two_blocks()
{
    TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        hexOf(Sha256::digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_abc_matches_the_nist_vector);
    RUN_TEST(test_empty_message_matches_the_nist_vector);
    RUN_TEST(test_message_that_needs_two_blocks);
    return UNITY_END();
}
```

`firmware/test/spotify/test_pkce/test_main.cpp`:

```cpp
#include <unity.h>

#include <cstring>
#include <string>

#include "spotify/Pkce.h"

namespace {

constexpr const char* ALLOWED = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

}

void setUp()
{
}

void tearDown()
{
}

void test_rfc7636_example_challenge()
{
    std::string challenge = Pkce::challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk");

    TEST_ASSERT_EQUAL_STRING("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM", challenge.c_str());
}

void test_verifier_has_64_characters_from_the_allowed_alphabet()
{
    uint8_t random[Pkce::VERIFIER_LENGTH];
    for (size_t index = 0; index < Pkce::VERIFIER_LENGTH; index++) {
        random[index] = static_cast<uint8_t>(index * 4);
    }

    std::string verifier = Pkce::verifier(random);

    TEST_ASSERT_EQUAL_size_t(64, verifier.size());
    TEST_ASSERT_EQUAL_INT('A', verifier[0]);
    for (char character : verifier) {
        TEST_ASSERT_NOT_NULL(strchr(ALLOWED, character));
    }
}

void test_byte_65_is_the_last_letter_of_the_alphabet()
{
    uint8_t random[Pkce::VERIFIER_LENGTH] = {};
    random[1] = 65;

    TEST_ASSERT_EQUAL_INT('~', Pkce::verifier(random)[1]);
}

void test_hex_is_lowercase_and_padded()
{
    const uint8_t bytes[] = {0x00, 0x9f, 0xff};

    TEST_ASSERT_EQUAL_STRING("009fff", Pkce::hex(bytes, sizeof(bytes)).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_rfc7636_example_challenge);
    RUN_TEST(test_verifier_has_64_characters_from_the_allowed_alphabet);
    RUN_TEST(test_byte_65_is_the_last_letter_of_the_alphabet);
    RUN_TEST(test_hex_is_lowercase_and_padded);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: FAIL — `spotify/Sha256.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`firmware/src/spotify/Sha256.h`:

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

// SHA-256 próprio na lógica pura: o PKCE precisa dele e assim ele é testado no PC contra os vetores da NIST.
namespace Sha256 {

std::array<uint8_t, 32> digest(const std::string& message);

}
```

`firmware/src/spotify/Sha256.cpp`:

```cpp
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
```

`firmware/src/spotify/Base64Url.h`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// base64 com "-" e "_" no lugar de "+" e "/", sem "=": o formato que o PKCE pede para o code_challenge.
namespace Base64Url {

std::string encode(const uint8_t* bytes, size_t length);

}
```

`firmware/src/spotify/Base64Url.cpp`:

```cpp
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
```

`firmware/src/spotify/Pkce.h`:

```cpp
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
```

`firmware/src/spotify/Pkce.cpp`:

```cpp
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
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: `7 test cases: 7 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/spotify firmware/test/spotify
git commit -m "feat(spotify): SHA-256, base64url e PKCE testados contra os vetores oficiais

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Fluxo de login — URL, `state` e callback

**Files:**
- Create: `firmware/src/spotify/AuthFlow.h`, `firmware/src/spotify/AuthFlow.cpp`
- Test: `firmware/test/spotify/test_auth_flow/test_main.cpp`

**Interfaces:**
- Consumes: nada além da biblioteca padrão.
- Produces:
  - `namespace AuthFlow {`
    `constexpr const char* DEFAULT_CLIENT_ID; REDIRECT_URI; SCOPES; constexpr uint32_t VALID_FOR_MILLIS = 600000;`
    `struct Pending { std::string verifier; std::string nonce; uint32_t createdMillis = 0; };`
    `struct HostAndNonce { std::string host; uint16_t port; std::string nonce; };`
    `struct Callback { std::string code; std::string state; };`
    `enum class CallbackCheck { Accepted, Expired, Mismatch };`
    `std::string urlEncode(const std::string&); std::string state(const std::string& ip, uint16_t port, const std::string& nonce);`
    `std::optional<HostAndNonce> parseState(const std::string&);`
    `std::string authorizeUrl(const std::string& clientId, const std::string& challenge, const std::string& state);`
    `CallbackCheck check(const Pending&, const std::string& state, uint32_t nowMillis);`
    `std::optional<Callback> fromPastedUrl(const std::string& url);`
    `std::string tokenRequestBody(const std::string& clientId, const std::string& code, const std::string& verifier);`
    `std::string refreshRequestBody(const std::string& clientId, const std::string& refreshToken); }`

- [ ] **Step 1: Teste**

`firmware/test/spotify/test_auth_flow/test_main.cpp`:

```cpp
#include <unity.h>

#include "spotify/AuthFlow.h"

namespace {

const std::string NONCE = "0123456789abcdef0123456789abcdef";
const std::string STATE = "192.168.1.34:80." + NONCE;

}

void setUp()
{
}

void tearDown()
{
}

void test_url_encode_keeps_unreserved_and_escapes_the_rest()
{
    TEST_ASSERT_EQUAL_STRING("a%20b%2Fc%3A~-._", AuthFlow::urlEncode("a b/c:~-._").c_str());
}

void test_authorize_url_has_every_parameter()
{
    std::string url = AuthFlow::authorizeUrl("CID", "CHALLENGE", STATE);

    std::string expected = "https://accounts.spotify.com/authorize?response_type=code&client_id=CID"
        "&scope=user-read-playback-state%20user-modify-playback-state%20user-read-currently-playing"
        "&redirect_uri=https%3A%2F%2Fleonardolermen.github.io%2Fesp32-display%2Fcallback%2F"
        "&code_challenge_method=S256&code_challenge=CHALLENGE&state=192.168.1.34%3A80." + NONCE;
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), url.c_str());
}

void test_state_round_trip()
{
    std::string state = AuthFlow::state("192.168.1.34", 80, NONCE);
    std::optional<AuthFlow::HostAndNonce> parsed = AuthFlow::parseState(state);

    TEST_ASSERT_EQUAL_STRING(STATE.c_str(), state.c_str());
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL_STRING("192.168.1.34", parsed->host.c_str());
    TEST_ASSERT_EQUAL_UINT16(80, parsed->port);
    TEST_ASSERT_EQUAL_STRING(NONCE.c_str(), parsed->nonce.c_str());
}

void test_malformed_states_are_refused()
{
    TEST_ASSERT_FALSE(AuthFlow::parseState("").has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34").has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34:abc." + NONCE).has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34:80.short").has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState(":80." + NONCE).has_value());
    TEST_ASSERT_FALSE(AuthFlow::parseState("192.168.1.34:99999." + NONCE).has_value());
}

void test_right_nonce_inside_ten_minutes_is_accepted()
{
    AuthFlow::Pending pending{"verifier", NONCE, 1000};

    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Accepted, AuthFlow::check(pending, STATE, 1000 + 599999));
}

void test_old_callback_is_expired()
{
    AuthFlow::Pending pending{"verifier", NONCE, 1000};

    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Expired, AuthFlow::check(pending, STATE, 1000 + 600000));
}

void test_callback_from_another_login_is_a_mismatch()
{
    AuthFlow::Pending other{"verifier", "ffffffffffffffffffffffffffffffff", 1000};
    AuthFlow::Pending none{};

    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Mismatch, AuthFlow::check(other, STATE, 2000));
    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Mismatch, AuthFlow::check(none, STATE, 2000));
    TEST_ASSERT_EQUAL(AuthFlow::CallbackCheck::Mismatch, AuthFlow::check(other, "lixo", 2000));
}

void test_pasted_url_gives_code_and_state()
{
    std::optional<AuthFlow::Callback> callback = AuthFlow::fromPastedUrl(
        "https://leonardolermen.github.io/esp32-display/callback/?code=AQB%2Dx&state=192.168.1.34%3A80." + NONCE);

    TEST_ASSERT_TRUE(callback.has_value());
    TEST_ASSERT_EQUAL_STRING("AQB-x", callback->code.c_str());
    TEST_ASSERT_EQUAL_STRING(STATE.c_str(), callback->state.c_str());
}

void test_pasted_url_without_code_is_nothing()
{
    TEST_ASSERT_FALSE(AuthFlow::fromPastedUrl("https://example.com/callback/?state=x").has_value());
    TEST_ASSERT_FALSE(AuthFlow::fromPastedUrl("sem interrogação").has_value());
}

void test_token_request_body()
{
    std::string body = AuthFlow::tokenRequestBody("CID", "AQB", "VER");

    TEST_ASSERT_EQUAL_STRING("grant_type=authorization_code&code=AQB"
        "&redirect_uri=https%3A%2F%2Fleonardolermen.github.io%2Fesp32-display%2Fcallback%2F"
        "&client_id=CID&code_verifier=VER", body.c_str());
}

void test_refresh_request_body()
{
    TEST_ASSERT_EQUAL_STRING("grant_type=refresh_token&refresh_token=R%2B1&client_id=CID",
        AuthFlow::refreshRequestBody("CID", "R+1").c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_url_encode_keeps_unreserved_and_escapes_the_rest);
    RUN_TEST(test_authorize_url_has_every_parameter);
    RUN_TEST(test_state_round_trip);
    RUN_TEST(test_malformed_states_are_refused);
    RUN_TEST(test_right_nonce_inside_ten_minutes_is_accepted);
    RUN_TEST(test_old_callback_is_expired);
    RUN_TEST(test_callback_from_another_login_is_a_mismatch);
    RUN_TEST(test_pasted_url_gives_code_and_state);
    RUN_TEST(test_pasted_url_without_code_is_nothing);
    RUN_TEST(test_token_request_body);
    RUN_TEST(test_refresh_request_body);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f spotify/test_auth_flow`
Expected: FAIL — `spotify/AuthFlow.h: No such file or directory`.

- [ ] **Step 3: Implementar**

`firmware/src/spotify/AuthFlow.h`:

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>

// O login do Spotify sem client secret: a placa gera o PKCE e o state, o Spotify devolve para o relay no GitHub
// Pages, e o relay manda o navegador de volta para a placa com code e state.
namespace AuthFlow {

constexpr const char* DEFAULT_CLIENT_ID = "5ee610458f974597ba5c5f96eab539b5";
constexpr const char* REDIRECT_URI = "https://leonardolermen.github.io/esp32-display/callback/";
constexpr const char* SCOPES = "user-read-playback-state user-modify-playback-state user-read-currently-playing";
constexpr uint32_t VALID_FOR_MILLIS = 10 * 60 * 1000;

struct Pending {
    std::string verifier;
    std::string nonce;
    uint32_t createdMillis = 0;
};

struct HostAndNonce {
    std::string host;
    uint16_t port;
    std::string nonce;
};

struct Callback {
    std::string code;
    std::string state;
};

enum class CallbackCheck {
    Accepted,
    Expired,
    Mismatch,
};

std::string urlEncode(const std::string& text);
std::string state(const std::string& ip, uint16_t port, const std::string& nonce);
std::optional<HostAndNonce> parseState(const std::string& state);
std::string authorizeUrl(const std::string& clientId, const std::string& challenge, const std::string& state);
CallbackCheck check(const Pending& pending, const std::string& state, uint32_t nowMillis);
std::optional<Callback> fromPastedUrl(const std::string& url);
std::string tokenRequestBody(const std::string& clientId, const std::string& code, const std::string& verifier);
std::string refreshRequestBody(const std::string& clientId, const std::string& refreshToken);

}
```

`firmware/src/spotify/AuthFlow.cpp`:

```cpp
#include "spotify/AuthFlow.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* AUTHORIZE_URL = "https://accounts.spotify.com/authorize";
constexpr size_t NONCE_LENGTH = 32;
constexpr size_t MAX_PORT_DIGITS = 5;
constexpr unsigned long MAX_PORT = 65535;

bool isUnreserved(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) || character == '-' || character == '.'
        || character == '_' || character == '~';
}

bool isLowerHex(char character)
{
    return std::isdigit(static_cast<unsigned char>(character)) || (character >= 'a' && character <= 'f');
}

bool allOf(const std::string& text, bool (*accepts)(char))
{
    return std::all_of(text.begin(), text.end(), accepts);
}

bool isDigit(char character)
{
    return std::isdigit(static_cast<unsigned char>(character));
}

int hexValue(char character)
{
    if (std::isdigit(static_cast<unsigned char>(character))) {
        return character - '0';
    }

    return std::tolower(static_cast<unsigned char>(character)) - 'a' + 10;
}

std::string urlDecode(const std::string& text)
{
    std::string decoded;
    for (size_t index = 0; index < text.size(); index++) {
        bool escaped = text[index] == '%' && index + 2 < text.size();
        if (escaped) {
            decoded += static_cast<char>(hexValue(text[index + 1]) * 16 + hexValue(text[index + 2]));
            index += 2;
            continue;
        }
        decoded += text[index] == '+' ? ' ' : text[index];
    }
    return decoded;
}

std::optional<std::string> queryValue(const std::string& query, const std::string& key)
{
    size_t start = 0;
    while (start <= query.size()) {
        size_t end = query.find('&', start);
        std::string pair = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        size_t equals = pair.find('=');
        if (equals != std::string::npos && pair.substr(0, equals) == key) {
            return urlDecode(pair.substr(equals + 1));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return std::nullopt;
}

}

namespace AuthFlow {

std::string urlEncode(const std::string& text)
{
    std::string encoded;
    char buffer[4];
    for (char character : text) {
        if (isUnreserved(character)) {
            encoded += character;
            continue;
        }
        snprintf(buffer, sizeof(buffer), "%%%02X", static_cast<unsigned char>(character));
        encoded += buffer;
    }
    return encoded;
}

std::string state(const std::string& ip, uint16_t port, const std::string& nonce)
{
    return ip + ":" + std::to_string(port) + "." + nonce;
}

std::optional<HostAndNonce> parseState(const std::string& state)
{
    size_t colon = state.find(':');
    size_t dot = colon == std::string::npos ? std::string::npos : state.find('.', colon);
    if (colon == 0 || dot == std::string::npos) {
        return std::nullopt;
    }

    std::string portText = state.substr(colon + 1, dot - colon - 1);
    std::string nonce = state.substr(dot + 1);
    bool portLooksRight = !portText.empty() && portText.size() <= MAX_PORT_DIGITS && allOf(portText, isDigit);
    unsigned long port = portLooksRight ? std::strtoul(portText.c_str(), nullptr, 10) : 0;
    bool nonceLooksRight = nonce.size() == NONCE_LENGTH && allOf(nonce, isLowerHex);
    if (port == 0 || port > MAX_PORT || !nonceLooksRight) {
        return std::nullopt;
    }

    return HostAndNonce{state.substr(0, colon), static_cast<uint16_t>(port), nonce};
}

std::string authorizeUrl(const std::string& clientId, const std::string& challenge, const std::string& state)
{
    return std::string(AUTHORIZE_URL) + "?response_type=code&client_id=" + urlEncode(clientId)
        + "&scope=" + urlEncode(SCOPES) + "&redirect_uri=" + urlEncode(REDIRECT_URI)
        + "&code_challenge_method=S256&code_challenge=" + challenge + "&state=" + urlEncode(state);
}

CallbackCheck check(const Pending& pending, const std::string& state, uint32_t nowMillis)
{
    std::optional<HostAndNonce> parsed = parseState(state);
    if (!parsed || pending.nonce.empty() || parsed->nonce != pending.nonce) {
        return CallbackCheck::Mismatch;
    }

    bool fresh = nowMillis - pending.createdMillis < VALID_FOR_MILLIS;
    return fresh ? CallbackCheck::Accepted : CallbackCheck::Expired;
}

std::optional<Callback> fromPastedUrl(const std::string& url)
{
    size_t question = url.find('?');
    if (question == std::string::npos) {
        return std::nullopt;
    }

    std::string query = url.substr(question + 1);
    query = query.substr(0, query.find('#'));
    std::optional<std::string> code = queryValue(query, "code");
    std::optional<std::string> state = queryValue(query, "state");
    if (!code || !state) {
        return std::nullopt;
    }

    return Callback{*code, *state};
}

std::string tokenRequestBody(const std::string& clientId, const std::string& code, const std::string& verifier)
{
    return "grant_type=authorization_code&code=" + urlEncode(code) + "&redirect_uri=" + urlEncode(REDIRECT_URI)
        + "&client_id=" + urlEncode(clientId) + "&code_verifier=" + urlEncode(verifier);
}

std::string refreshRequestBody(const std::string& clientId, const std::string& refreshToken)
{
    return "grant_type=refresh_token&refresh_token=" + urlEncode(refreshToken) + "&client_id=" + urlEncode(clientId);
}

}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native -f spotify/test_auth_flow`
Expected: `11 test cases: 11 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add firmware/src/spotify firmware/test/spotify
git commit -m "feat(spotify): URL de autorização, state com nonce e leitura do callback

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Relay no GitHub Pages

**Files:**
- Create: `relay/callback/relay.js`, `relay/callback/index.html`
- Create: `relay/test/relay.test.mjs`
- Create: `.github/workflows/pages.yml`
- Modify: `CLAUDE.md` (comando do teste do relay)

**Interfaces:**
- Produces: `relay.js` exporta `isPrivateIpv4(host) → boolean`, `parseState(state) → {host, port, nonce} | null`,
  `targetUrl(search) → string | null`. O formato do `state` é o mesmo da Task 2.

- [ ] **Step 1: Teste**

`relay/test/relay.test.mjs`:

```js
import test from 'node:test';
import assert from 'node:assert/strict';

import { isPrivateIpv4, parseState, targetUrl } from '../callback/relay.js';

const NONCE = '0123456789abcdef0123456789abcdef';

test('private IPv4 ranges are accepted', () => {
    for (const host of ['10.0.0.5', '172.16.0.1', '172.31.255.254', '192.168.1.34']) {
        assert.ok(isPrivateIpv4(host), host);
    }
});

test('public, loopback and malformed hosts are refused', () => {
    for (const host of ['8.8.8.8', '172.32.0.1', '172.15.0.1', '127.0.0.1', 'localhost', '192.168.1', '192.168.1.256',
        'evil.com']) {
        assert.equal(isPrivateIpv4(host), false, host);
    }
});

test('a state with ip, port and nonce is parsed', () => {
    assert.deepEqual(parseState(`192.168.1.34:80.${NONCE}`), { host: '192.168.1.34', port: 80, nonce: NONCE });
});

test('malformed or public states are refused', () => {
    for (const state of ['', `192.168.1.34:http.${NONCE}`, '192.168.1.34:80.xyz', `8.8.8.8:80.${NONCE}`,
        `192.168.1.34:70000.${NONCE}`, null]) {
        assert.equal(parseState(state), null, String(state));
    }
});

test('the target sends code and state back to the board', () => {
    assert.equal(targetUrl(`?code=AQB-x&state=192.168.1.34%3A80.${NONCE}`),
        `http://192.168.1.34:80/api/spotify/callback?code=AQB-x&state=192.168.1.34%3A80.${NONCE}`);
});

test('no target without code or with a public host', () => {
    assert.equal(targetUrl(`?state=192.168.1.34%3A80.${NONCE}`), null);
    assert.equal(targetUrl(`?code=x&state=8.8.8.8%3A80.${NONCE}`), null);
});
```

- [ ] **Step 2: Rodar e ver falhar**

Run (da raiz do repositório): `node --test "relay/test/*.test.mjs"`
Expected: FAIL — `Cannot find module …relay/callback/relay.js`.

- [ ] **Step 3: Implementar**

`relay/callback/relay.js`:

```js
// O Spotify só devolve o login para um endereço HTTPS registrado; este relay recebe e manda o navegador para a placa
// na rede de casa. Só aceita IP privado para não virar um redirecionador aberto.
const STATE_PATTERN = /^([0-9.]+):([0-9]{1,5})\.([0-9a-f]{32})$/;
const MAX_PORT = 65535;

export function isPrivateIpv4(host) {
    const parts = String(host).split('.');
    const wellFormed = parts.length === 4 && parts.every((part) => /^[0-9]{1,3}$/.test(part) && Number(part) <= 255);
    if (!wellFormed) {
        return false;
    }

    const [first, second] = parts.map(Number);
    return first === 10 || (first === 172 && second >= 16 && second <= 31) || (first === 192 && second === 168);
}

export function parseState(state) {
    const match = STATE_PATTERN.exec(state ?? '');
    if (!match) {
        return null;
    }

    const [, host, portText, nonce] = match;
    const port = Number(portText);
    if (port < 1 || port > MAX_PORT || !isPrivateIpv4(host)) {
        return null;
    }

    return { host, port, nonce };
}

export function targetUrl(search) {
    const params = new URLSearchParams(search);
    const code = params.get('code');
    const state = params.get('state');
    const parsed = parseState(state);
    if (!code || !parsed) {
        return null;
    }

    return `http://${parsed.host}:${parsed.port}/api/spotify/callback?code=${encodeURIComponent(code)}`
        + `&state=${encodeURIComponent(state)}`;
}
```

`relay/callback/index.html`:

```html
<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Claudinho — Spotify</title>
<style>
  body { margin: 0; background: #1F1E1C; color: #F5F1EA; font: 17px/1.5 system-ui, sans-serif; }
  main { max-width: 560px; margin: 0 auto; padding: 32px 24px; }
  h1 { font-size: 22px; font-weight: 500; }
  p { color: #A8A29A; }
  code { display: block; word-break: break-all; background: #34322F; padding: 12px; border-radius: 8px; color: #F5F1EA; }
  button { margin-top: 12px; padding: 10px 16px; border: 0; border-radius: 8px; background: #D97757; color: #2b1209;
    font-weight: 500; }
</style>
</head>
<body>
<main>
  <h1 id="title">Voltando para o Claudinho…</h1>
  <p id="text">Se nada acontecer em alguns segundos, copie o link abaixo e cole no card Spotify em claudinho.local.</p>
  <code id="link"></code>
  <button id="copy" type="button">Copiar link</button>
</main>
<script type="module">
import { targetUrl } from './relay.js';

const title = document.getElementById('title');
const text = document.getElementById('text');
const link = document.getElementById('link');
const copy = document.getElementById('copy');
const params = new URLSearchParams(location.search);

link.textContent = location.href;
copy.addEventListener('click', async () => {
    await navigator.clipboard.writeText(location.href);
    copy.textContent = 'Copiado';
});

if (params.get('error')) {
    title.textContent = 'Login cancelado';
    text.textContent = 'Nada foi alterado. Se quiser, toque em Conectar de novo em claudinho.local.';
    link.hidden = true;
    copy.hidden = true;
} else {
    const target = targetUrl(location.search);
    if (target) {
        location.replace(target);
    } else {
        title.textContent = 'Link inválido';
        text.textContent = 'Este link não veio de um Claudinho na sua rede. Toque em Conectar de novo em claudinho.local.';
    }
}
</script>
</body>
</html>
```

`.github/workflows/pages.yml`:

```yaml
name: relay

on:
  push:
    branches: [main]
    paths: ['relay/**', '.github/workflows/pages.yml']
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: true

jobs:
  deploy:
    runs-on: ubuntu-latest
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    steps:
      - uses: actions/checkout@v4
      - uses: actions/configure-pages@v5
      - uses: actions/upload-pages-artifact@v3
        with:
          path: relay
      - id: deployment
        uses: actions/deploy-pages@v4
```

Em `CLAUDE.md`, na seção "Testes", depois da linha do JS da página:

```markdown
- Relay do Spotify (da raiz): `node --test "relay/test/*.test.mjs"`.
```

- [ ] **Step 4: Rodar e ver passar**

Run: `node --test "relay/test/*.test.mjs"`
Expected: `pass 6`, `fail 0`.

- [ ] **Step 5: Commit**

```bash
git add relay .github/workflows/pages.yml CLAUDE.md
git commit -m "feat(spotify): relay do login no GitHub Pages, só para IP privado

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Estado do player, parsing do `/me/player` e do token

**Files:**
- Create: `firmware/src/spotify/PlayerState.h`
- Create: `firmware/src/spotify/PlayerParser.h`, `firmware/src/spotify/PlayerParser.cpp`
- Create: `firmware/src/spotify/TokenResponse.h`, `firmware/src/spotify/TokenResponse.cpp`
- Test: `firmware/test/spotify/test_player_parser/test_main.cpp`
- Test: `firmware/test/spotify/test_token_response/test_main.cpp`

**Interfaces:**
- Produces:
  - `enum class PlayerStatus { Disconnected, NoDevice, Playing, Paused };`
  - `struct Track { std::string title, artists, album, albumId, coverUrl; uint32_t durationMs = 0; };`
  - `struct PlayerState { PlayerStatus status = PlayerStatus::Disconnected; bool hasItem = false; Track track;
    uint32_t progressMs = 0; uint32_t sampledAtMillis = 0; std::string device; std::optional<int> volume;
    bool volumeAllowed = true; bool stale = false; std::string loginUrl; };`
  - `namespace PlayerParser { std::optional<PlayerState> parse(std::string_view json, uint32_t nowMillis); }`
  - `struct TokenResult { bool ok = false; std::string accessToken; std::optional<std::string> refreshToken;
    uint32_t expiresInSeconds = 0; bool invalidGrant = false; std::string error; };`
    `namespace TokenResponse { TokenResult parse(std::string_view json); }`

- [ ] **Step 1: Testes**

`firmware/test/spotify/test_player_parser/test_main.cpp`:

```cpp
#include <unity.h>

#include "spotify/PlayerParser.h"

namespace {

const char* PLAYING = R"({"device":{"id":"d1","name":"MacBook do Pai","type":"Computer","volume_percent":64},
 "progress_ms":42000,"is_playing":true,"currently_playing_type":"track",
 "item":{"name":"Águas de Março","duration_ms":212000,
   "artists":[{"name":"Elis Regina"},{"name":"Tom Jobim"}],
   "album":{"id":"alb1","name":"Elis & Tom","images":[{"url":"https://i.scdn.co/640","width":640},
     {"url":"https://i.scdn.co/300","width":300},{"url":"https://i.scdn.co/64","width":64}]}}})";

const char* PAUSED_WITHOUT_VOLUME = R"({"device":{"name":"Echo","volume_percent":null},"progress_ms":10,
 "is_playing":false,"item":{"name":"Faixa","duration_ms":1000,"artists":[{"name":"A"}],
 "album":{"id":"alb2","name":"B","images":[]}}})";

const char* ADVERTISEMENT = R"({"device":{"name":"Celular","volume_percent":30},"progress_ms":0,"is_playing":true,
 "currently_playing_type":"ad","item":null})";

const char* EPISODE = R"({"device":{"name":"Celular","volume_percent":30},"progress_ms":5000,"is_playing":true,
 "item":{"id":"ep1","name":"Episódio 12","duration_ms":3600000,"show":{"name":"Podcast X"},
   "images":[{"url":"https://i.scdn.co/ep640","width":640},{"url":"https://i.scdn.co/ep300","width":300}]}})";

}

void setUp()
{
}

void tearDown()
{
}

void test_playing_track_fills_every_field()
{
    std::optional<PlayerState> state = PlayerParser::parse(PLAYING, 1234);

    TEST_ASSERT_TRUE(state.has_value());
    TEST_ASSERT_EQUAL(PlayerStatus::Playing, state->status);
    TEST_ASSERT_TRUE(state->hasItem);
    TEST_ASSERT_EQUAL_STRING("Águas de Março", state->track.title.c_str());
    TEST_ASSERT_EQUAL_STRING("Elis Regina, Tom Jobim", state->track.artists.c_str());
    TEST_ASSERT_EQUAL_STRING("Elis & Tom", state->track.album.c_str());
    TEST_ASSERT_EQUAL_STRING("alb1", state->track.albumId.c_str());
    TEST_ASSERT_EQUAL_STRING("https://i.scdn.co/300", state->track.coverUrl.c_str());
    TEST_ASSERT_EQUAL_UINT32(212000, state->track.durationMs);
    TEST_ASSERT_EQUAL_UINT32(42000, state->progressMs);
    TEST_ASSERT_EQUAL_UINT32(1234, state->sampledAtMillis);
    TEST_ASSERT_EQUAL_STRING("MacBook do Pai", state->device.c_str());
    TEST_ASSERT_EQUAL_INT(64, *state->volume);
}

void test_paused_without_volume()
{
    std::optional<PlayerState> state = PlayerParser::parse(PAUSED_WITHOUT_VOLUME, 0);

    TEST_ASSERT_EQUAL(PlayerStatus::Paused, state->status);
    TEST_ASSERT_FALSE(state->volume.has_value());
    TEST_ASSERT_EQUAL_STRING("", state->track.coverUrl.c_str());
}

void test_advertisement_has_no_item()
{
    std::optional<PlayerState> state = PlayerParser::parse(ADVERTISEMENT, 0);

    TEST_ASSERT_TRUE(state.has_value());
    TEST_ASSERT_EQUAL(PlayerStatus::Playing, state->status);
    TEST_ASSERT_FALSE(state->hasItem);
}

void test_podcast_episode_uses_the_show_and_its_images()
{
    std::optional<PlayerState> state = PlayerParser::parse(EPISODE, 0);

    TEST_ASSERT_TRUE(state->hasItem);
    TEST_ASSERT_EQUAL_STRING("Podcast X", state->track.artists.c_str());
    TEST_ASSERT_EQUAL_STRING("ep1", state->track.albumId.c_str());
    TEST_ASSERT_EQUAL_STRING("https://i.scdn.co/ep300", state->track.coverUrl.c_str());
}

void test_broken_json_is_nothing()
{
    TEST_ASSERT_FALSE(PlayerParser::parse("{oops", 0).has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_playing_track_fills_every_field);
    RUN_TEST(test_paused_without_volume);
    RUN_TEST(test_advertisement_has_no_item);
    RUN_TEST(test_podcast_episode_uses_the_show_and_its_images);
    RUN_TEST(test_broken_json_is_nothing);
    return UNITY_END();
}
```

`firmware/test/spotify/test_token_response/test_main.cpp`:

```cpp
#include <unity.h>

#include "spotify/TokenResponse.h"

void setUp()
{
}

void tearDown()
{
}

void test_first_exchange_brings_both_tokens()
{
    TokenResult result = TokenResponse::parse(
        R"({"access_token":"AT","token_type":"Bearer","expires_in":3600,"refresh_token":"RT","scope":"x"})");

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL_STRING("AT", result.accessToken.c_str());
    TEST_ASSERT_EQUAL_STRING("RT", result.refreshToken->c_str());
    TEST_ASSERT_EQUAL_UINT32(3600, result.expiresInSeconds);
}

void test_refresh_without_a_new_refresh_token_keeps_it_absent()
{
    TokenResult result = TokenResponse::parse(R"({"access_token":"AT2","token_type":"Bearer","expires_in":3600})");

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_FALSE(result.refreshToken.has_value());
}

void test_revoked_refresh_token_is_an_invalid_grant()
{
    TokenResult result = TokenResponse::parse(R"({"error":"invalid_grant","error_description":"Refresh token revoked"})");

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_TRUE(result.invalidGrant);
}

void test_other_errors_are_not_invalid_grant()
{
    TokenResult result = TokenResponse::parse(R"({"error":"invalid_client"})");

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_FALSE(result.invalidGrant);
    TEST_ASSERT_EQUAL_STRING("invalid_client", result.error.c_str());
}

void test_broken_json_is_not_ok()
{
    TEST_ASSERT_FALSE(TokenResponse::parse("<html>").ok);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_exchange_brings_both_tokens);
    RUN_TEST(test_refresh_without_a_new_refresh_token_keeps_it_absent);
    RUN_TEST(test_revoked_refresh_token_is_an_invalid_grant);
    RUN_TEST(test_other_errors_are_not_invalid_grant);
    RUN_TEST(test_broken_json_is_not_ok);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: FAIL nos dois testes novos por include ausente.

- [ ] **Step 3: Implementar**

`firmware/src/spotify/PlayerState.h`:

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>

enum class PlayerStatus {
    Disconnected,
    NoDevice,
    Playing,
    Paused,
};

struct Track {
    std::string title;
    std::string artists;
    std::string album;
    std::string albumId;
    std::string coverUrl;
    uint32_t durationMs = 0;
};

struct PlayerState {
    PlayerStatus status = PlayerStatus::Disconnected;
    bool hasItem = false;
    Track track;
    uint32_t progressMs = 0;
    uint32_t sampledAtMillis = 0;
    std::string device;
    std::optional<int> volume;
    bool volumeAllowed = true;
    bool stale = false;
    std::string loginUrl;
};
```

`firmware/src/spotify/PlayerParser.h`:

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "spotify/PlayerState.h"

namespace PlayerParser {

std::optional<PlayerState> parse(std::string_view json, uint32_t nowMillis);

}
```

`firmware/src/spotify/PlayerParser.cpp`:

```cpp
#include "spotify/PlayerParser.h"

#include <ArduinoJson.h>

#include <climits>
#include <cstdlib>

namespace {

constexpr int PREFERRED_COVER_WIDTH = 300;

// A resposta do player traz o mercado, as URLs externas e a lista de disponibilidade; o filtro guarda só o que a
// tela usa.
JsonDocument playerFilter()
{
    JsonDocument filter;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["device"]["name"] = true;
    filter["device"]["volume_percent"] = true;
    filter["item"]["id"] = true;
    filter["item"]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["item"]["artists"][0]["name"] = true;
    filter["item"]["album"]["id"] = true;
    filter["item"]["album"]["name"] = true;
    filter["item"]["album"]["images"][0]["url"] = true;
    filter["item"]["album"]["images"][0]["width"] = true;
    filter["item"]["show"]["name"] = true;
    filter["item"]["images"][0]["url"] = true;
    filter["item"]["images"][0]["width"] = true;
    return filter;
}

std::string joinArtists(JsonArrayConst artists)
{
    std::string joined;
    for (JsonObjectConst artist : artists) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += artist["name"] | "";
    }
    return joined;
}

std::string closestCover(JsonArrayConst images)
{
    std::string best;
    int bestDistance = INT_MAX;
    for (JsonObjectConst image : images) {
        int distance = std::abs((image["width"] | 0) - PREFERRED_COVER_WIDTH);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = image["url"] | "";
        }
    }
    return best;
}

void fillTrack(Track& track, JsonObjectConst item)
{
    track.title = item["name"] | "";
    track.durationMs = item["duration_ms"] | 0u;

    JsonObjectConst album = item["album"].as<JsonObjectConst>();
    // Episódio de podcast não tem álbum: o "artista" é o programa e a capa vem do próprio episódio.
    if (album.isNull()) {
        track.artists = item["show"]["name"] | "";
        track.albumId = item["id"] | "";
        track.coverUrl = closestCover(item["images"].as<JsonArrayConst>());
        return;
    }

    track.artists = joinArtists(item["artists"].as<JsonArrayConst>());
    track.album = album["name"] | "";
    track.albumId = album["id"] | "";
    track.coverUrl = closestCover(album["images"].as<JsonArrayConst>());
}

}

namespace PlayerParser {

std::optional<PlayerState> parse(std::string_view json, uint32_t nowMillis)
{
    JsonDocument filter = playerFilter();
    JsonDocument document;
    DeserializationError error = deserializeJson(document, json.data(), json.size(),
        DeserializationOption::Filter(filter));
    if (error != DeserializationError::Ok) {
        return std::nullopt;
    }

    PlayerState state;
    state.status = (document["is_playing"] | false) ? PlayerStatus::Playing : PlayerStatus::Paused;
    state.progressMs = document["progress_ms"] | 0u;
    state.sampledAtMillis = nowMillis;
    state.device = document["device"]["name"] | "";

    JsonVariantConst volume = document["device"]["volume_percent"];
    if (volume.is<int>()) {
        state.volume = volume.as<int>();
    }

    JsonObjectConst item = document["item"].as<JsonObjectConst>();
    state.hasItem = !item.isNull();
    if (state.hasItem) {
        fillTrack(state.track, item);
    }
    return state;
}

}
```

`firmware/src/spotify/TokenResponse.h`:

```cpp
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
```

`firmware/src/spotify/TokenResponse.cpp`:

```cpp
#include "spotify/TokenResponse.h"

#include <ArduinoJson.h>

namespace {

constexpr uint32_t DEFAULT_EXPIRES_SECONDS = 3600;
constexpr const char* INVALID_GRANT = "invalid_grant";

}

namespace TokenResponse {

TokenResult parse(std::string_view json)
{
    TokenResult result;
    JsonDocument document;
    if (deserializeJson(document, json.data(), json.size()) != DeserializationError::Ok) {
        result.error = "resposta inválida";
        return result;
    }

    const char* error = document["error"];
    if (error != nullptr) {
        result.error = error;
        result.invalidGrant = result.error == INVALID_GRANT;
        return result;
    }

    const char* access = document["access_token"];
    if (access == nullptr) {
        result.error = "sem access_token";
        return result;
    }

    result.ok = true;
    result.accessToken = access;
    result.expiresInSeconds = document["expires_in"] | DEFAULT_EXPIRES_SECONDS;

    // A renovação nem sempre manda um refresh token novo; ausente aqui quer dizer "continue com o antigo".
    const char* refresh = document["refresh_token"];
    if (refresh != nullptr) {
        result.refreshToken = std::string(refresh);
    }
    return result;
}

}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: todos verdes (`test_player_parser` 5, `test_token_response` 5).

- [ ] **Step 5: Commit**

```bash
git add firmware/src/spotify firmware/test/spotify
git commit -m "feat(spotify): leitura do player e da resposta de token

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Agenda do polling e progresso

**Files:**
- Create: `firmware/src/spotify/PollSchedule.h`, `firmware/src/spotify/PollSchedule.cpp`
- Create: `firmware/src/spotify/Progress.h`, `firmware/src/spotify/Progress.cpp`
- Test: `firmware/test/spotify/test_poll_schedule/test_main.cpp`
- Test: `firmware/test/spotify/test_progress/test_main.cpp`

**Interfaces:**
- Consumes: `PlayerStatus`, `PlayerState` (Task 4).
- Produces:
  - `namespace PollSchedule { constexpr uint32_t WATCHED_MILLIS = 3000, BACKGROUND_MILLIS = 15000, IDLE_MILLIS = 30000,
    IDLE_AFTER_PAUSE_MILLIS = 60000, DEFAULT_RETRY_MILLIS = 30000;
    std::optional<uint32_t> intervalMillis(PlayerStatus status, bool watched, uint32_t pausedForMillis);
    uint32_t retryAfterMillis(const std::string& header); }`
  - `namespace Progress { uint32_t estimate(const PlayerState& state, uint32_t nowMillis); std::string clock(uint32_t millis); }`

- [ ] **Step 1: Testes**

`firmware/test/spotify/test_poll_schedule/test_main.cpp`:

```cpp
#include <unity.h>

#include "spotify/PollSchedule.h"

void setUp()
{
}

void tearDown()
{
}

void test_disconnected_does_not_poll()
{
    TEST_ASSERT_FALSE(PollSchedule::intervalMillis(PlayerStatus::Disconnected, true, 0).has_value());
}

void test_playing_and_watched_is_fast()
{
    TEST_ASSERT_EQUAL_UINT32(3000, *PollSchedule::intervalMillis(PlayerStatus::Playing, true, 0));
}

void test_playing_in_the_background_is_slower()
{
    TEST_ASSERT_EQUAL_UINT32(15000, *PollSchedule::intervalMillis(PlayerStatus::Playing, false, 0));
}

void test_no_device_is_idle_even_when_watched()
{
    TEST_ASSERT_EQUAL_UINT32(30000, *PollSchedule::intervalMillis(PlayerStatus::NoDevice, true, 0));
}

void test_recent_pause_keeps_the_pace_and_long_pause_goes_idle()
{
    TEST_ASSERT_EQUAL_UINT32(3000, *PollSchedule::intervalMillis(PlayerStatus::Paused, true, 10000));
    TEST_ASSERT_EQUAL_UINT32(30000, *PollSchedule::intervalMillis(PlayerStatus::Paused, true, 70000));
}

void test_retry_after_is_in_seconds_with_a_safe_default()
{
    TEST_ASSERT_EQUAL_UINT32(12000, PollSchedule::retryAfterMillis("12"));
    TEST_ASSERT_EQUAL_UINT32(1000, PollSchedule::retryAfterMillis("0"));
    TEST_ASSERT_EQUAL_UINT32(30000, PollSchedule::retryAfterMillis(""));
    TEST_ASSERT_EQUAL_UINT32(30000, PollSchedule::retryAfterMillis("abc"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_disconnected_does_not_poll);
    RUN_TEST(test_playing_and_watched_is_fast);
    RUN_TEST(test_playing_in_the_background_is_slower);
    RUN_TEST(test_no_device_is_idle_even_when_watched);
    RUN_TEST(test_recent_pause_keeps_the_pace_and_long_pause_goes_idle);
    RUN_TEST(test_retry_after_is_in_seconds_with_a_safe_default);
    return UNITY_END();
}
```

`firmware/test/spotify/test_progress/test_main.cpp`:

```cpp
#include <unity.h>

#include "spotify/Progress.h"

namespace {

PlayerState sampled(PlayerStatus status, uint32_t progressMs, uint32_t sampledAt)
{
    PlayerState state;
    state.status = status;
    state.hasItem = true;
    state.progressMs = progressMs;
    state.sampledAtMillis = sampledAt;
    state.track.durationMs = 200000;
    return state;
}

}

void setUp()
{
}

void tearDown()
{
}

void test_playing_advances_with_the_clock()
{
    TEST_ASSERT_EQUAL_UINT32(3500, Progress::estimate(sampled(PlayerStatus::Playing, 1000, 5000), 7500));
}

void test_paused_stays_where_it_was()
{
    TEST_ASSERT_EQUAL_UINT32(1000, Progress::estimate(sampled(PlayerStatus::Paused, 1000, 5000), 99999));
}

void test_never_passes_the_duration()
{
    TEST_ASSERT_EQUAL_UINT32(200000, Progress::estimate(sampled(PlayerStatus::Playing, 199000, 0), 5000));
}

void test_survives_the_millis_wraparound()
{
    PlayerState state = sampled(PlayerStatus::Playing, 1000, 0xFFFFF000u);

    TEST_ASSERT_EQUAL_UINT32(1000 + 0x1800, Progress::estimate(state, 0x00000800u));
}

void test_clock_is_minutes_and_two_digit_seconds()
{
    TEST_ASSERT_EQUAL_STRING("0:00", Progress::clock(0).c_str());
    TEST_ASSERT_EQUAL_STRING("1:01", Progress::clock(61000).c_str());
    TEST_ASSERT_EQUAL_STRING("9:59", Progress::clock(599999).c_str());
    TEST_ASSERT_EQUAL_STRING("60:00", Progress::clock(3600000).c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_playing_advances_with_the_clock);
    RUN_TEST(test_paused_stays_where_it_was);
    RUN_TEST(test_never_passes_the_duration);
    RUN_TEST(test_survives_the_millis_wraparound);
    RUN_TEST(test_clock_is_minutes_and_two_digit_seconds);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: FAIL nos dois testes novos por include ausente.

- [ ] **Step 3: Implementar**

`firmware/src/spotify/PollSchedule.h`:

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "spotify/PlayerState.h"

// A API tem cota por app; consultar rápido só quando alguém está olhando e algo está tocando.
namespace PollSchedule {

constexpr uint32_t WATCHED_MILLIS = 3000;
constexpr uint32_t BACKGROUND_MILLIS = 15000;
constexpr uint32_t IDLE_MILLIS = 30000;
constexpr uint32_t IDLE_AFTER_PAUSE_MILLIS = 60000;
constexpr uint32_t DEFAULT_RETRY_MILLIS = 30000;

std::optional<uint32_t> intervalMillis(PlayerStatus status, bool watched, uint32_t pausedForMillis);
uint32_t retryAfterMillis(const std::string& header);

}
```

`firmware/src/spotify/PollSchedule.cpp`:

```cpp
#include "spotify/PollSchedule.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

constexpr uint32_t MIN_RETRY_MILLIS = 1000;
constexpr uint32_t MILLIS_PER_SECOND = 1000;

}

namespace PollSchedule {

std::optional<uint32_t> intervalMillis(PlayerStatus status, bool watched, uint32_t pausedForMillis)
{
    bool longPause = status == PlayerStatus::Paused && pausedForMillis > IDLE_AFTER_PAUSE_MILLIS;
    bool active = status == PlayerStatus::Playing || (status == PlayerStatus::Paused && !longPause);

    if (status == PlayerStatus::Disconnected) {
        return std::nullopt;
    }
    if (!active) {
        return IDLE_MILLIS;
    }
    return watched ? WATCHED_MILLIS : BACKGROUND_MILLIS;
}

uint32_t retryAfterMillis(const std::string& header)
{
    bool numeric = !header.empty() && std::all_of(header.begin(), header.end(), [](char character) {
        return std::isdigit(static_cast<unsigned char>(character));
    });
    if (!numeric) {
        return DEFAULT_RETRY_MILLIS;
    }

    uint32_t seconds = static_cast<uint32_t>(std::strtoul(header.c_str(), nullptr, 10));
    return std::max(seconds * MILLIS_PER_SECOND, MIN_RETRY_MILLIS);
}

}
```

`firmware/src/spotify/Progress.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>

#include "spotify/PlayerState.h"

namespace Progress {

uint32_t estimate(const PlayerState& state, uint32_t nowMillis);
std::string clock(uint32_t millis);

}
```

`firmware/src/spotify/Progress.cpp`:

```cpp
#include "spotify/Progress.h"

#include <algorithm>
#include <cstdio>

namespace Progress {

// Entre um poll e outro a barra anda sozinha; a subtração sem sinal atravessa a virada do millis() sem saltar.
uint32_t estimate(const PlayerState& state, uint32_t nowMillis)
{
    uint32_t elapsed = state.status == PlayerStatus::Playing ? nowMillis - state.sampledAtMillis : 0;
    uint32_t position = state.progressMs + elapsed;

    return state.track.durationMs > 0 ? std::min(position, state.track.durationMs) : position;
}

std::string clock(uint32_t millis)
{
    uint32_t totalSeconds = millis / 1000;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u:%02u", static_cast<unsigned>(totalSeconds / 60),
        static_cast<unsigned>(totalSeconds % 60));
    return buffer;
}

}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: todos verdes (`test_poll_schedule` 6, `test_progress` 5).

- [ ] **Step 5: Commit**

```bash
git add firmware/src/spotify firmware/test/spotify
git commit -m "feat(spotify): agenda do polling e progresso estimado entre consultas

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: Capa — redução e cor predominante

**Files:**
- Create: `firmware/src/spotify/CoverScaler.h`, `firmware/src/spotify/CoverScaler.cpp`
- Create: `firmware/src/spotify/DominantColor.h`, `firmware/src/spotify/DominantColor.cpp`
- Create: `firmware/src/spotify/CoverArt.h`
- Test: `firmware/test/spotify/test_cover_scaler/test_main.cpp`
- Test: `firmware/test/spotify/test_dominant_color/test_main.cpp`

**Interfaces:**
- Produces:
  - `namespace CoverScaler { void downscale(const uint16_t* source, int sourceWidth, int sourceHeight, uint16_t* target, int size); }`
    — média por blocos em RGB565; também amplia (bloco de 1 pixel) quando a fonte é menor.
  - `namespace DominantColor { constexpr uint32_t FALLBACK = 0x34322F; constexpr double MAX_LUMINANCE = 0.15;
    uint32_t of(const uint16_t* pixels, size_t count); double luminance(uint32_t rgb); }` — cor em `0xRRGGBB`.
  - `constexpr int COVER_SIZE = 200; struct CoverArt { std::string albumId; std::vector<uint16_t> pixels;
    uint32_t color = DominantColor::FALLBACK; };`

- [ ] **Step 1: Testes**

`firmware/test/spotify/test_cover_scaler/test_main.cpp`:

```cpp
#include <unity.h>

#include <vector>

#include "spotify/CoverScaler.h"

namespace {

std::vector<uint16_t> scaled(const std::vector<uint16_t>& source, int width, int height)
{
    std::vector<uint16_t> target(200 * 200, 0xAAAA);
    CoverScaler::downscale(source.data(), width, height, target.data(), 200);
    return target;
}

void assertEverywhere(const std::vector<uint16_t>& target, uint16_t expected)
{
    for (uint16_t pixel : target) {
        TEST_ASSERT_EQUAL_HEX16(expected, pixel);
    }
}

}

void setUp()
{
}

void tearDown()
{
}

void test_uniform_300_keeps_its_color()
{
    assertEverywhere(scaled(std::vector<uint16_t>(300 * 300, 0x1234), 300, 300), 0x1234);
}

void test_alternating_columns_average_to_gray()
{
    std::vector<uint16_t> stripes(400 * 400);
    for (int y = 0; y < 400; y++) {
        for (int x = 0; x < 400; x++) {
            stripes[y * 400 + x] = x % 2 == 0 ? 0x0000 : 0xFFFF;
        }
    }

    assertEverywhere(scaled(stripes, 400, 400), 0x7BEF);
}

void test_large_640_cover_fits()
{
    assertEverywhere(scaled(std::vector<uint16_t>(640 * 640, 0xF800), 640, 640), 0xF800);
}

void test_small_64_cover_is_enlarged()
{
    assertEverywhere(scaled(std::vector<uint16_t>(64 * 64, 0x07E0), 64, 64), 0x07E0);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_uniform_300_keeps_its_color);
    RUN_TEST(test_alternating_columns_average_to_gray);
    RUN_TEST(test_large_640_cover_fits);
    RUN_TEST(test_small_64_cover_is_enlarged);
    return UNITY_END();
}
```

`firmware/test/spotify/test_dominant_color/test_main.cpp`:

```cpp
#include <unity.h>

#include <vector>

#include "spotify/DominantColor.h"

void setUp()
{
}

void tearDown()
{
}

void test_red_cover_gives_a_darker_red()
{
    std::vector<uint16_t> red(100, 0xF800);

    uint32_t color = DominantColor::of(red.data(), red.size());

    TEST_ASSERT_TRUE((color >> 16) > 0);
    TEST_ASSERT_EQUAL_UINT32(0, (color >> 8) & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(0, color & 0xFF);
    TEST_ASSERT_TRUE(DominantColor::luminance(color) <= DominantColor::MAX_LUMINANCE);
}

void test_black_and_white_only_falls_back()
{
    std::vector<uint16_t> pixels(100, 0x0000);
    for (size_t index = 0; index < pixels.size(); index += 2) {
        pixels[index] = 0xFFFF;
    }

    TEST_ASSERT_EQUAL_HEX32(DominantColor::FALLBACK, DominantColor::of(pixels.data(), pixels.size()));
}

void test_the_most_common_color_wins()
{
    std::vector<uint16_t> pixels(100, 0x07E0);
    for (size_t index = 0; index < 70; index++) {
        pixels[index] = 0x001F;
    }

    uint32_t color = DominantColor::of(pixels.data(), pixels.size());

    TEST_ASSERT_TRUE((color & 0xFF) > ((color >> 8) & 0xFF));
}

void test_a_dark_color_is_kept_as_is()
{
    std::vector<uint16_t> pixels(100, 0x2188);

    TEST_ASSERT_EQUAL_HEX32(0x213042, DominantColor::of(pixels.data(), pixels.size()));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_red_cover_gives_a_darker_red);
    RUN_TEST(test_black_and_white_only_falls_back);
    RUN_TEST(test_the_most_common_color_wins);
    RUN_TEST(test_a_dark_color_is_kept_as_is);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: FAIL nos dois testes novos por include ausente.

- [ ] **Step 3: Implementar**

`firmware/src/spotify/CoverScaler.h`:

```cpp
#pragma once

#include <cstdint>

// Reduz uma vez, ao baixar a capa: em full refresh, escalar no desenho custaria em todo quadro.
namespace CoverScaler {

void downscale(const uint16_t* source, int sourceWidth, int sourceHeight, uint16_t* target, int size);

}
```

`firmware/src/spotify/CoverScaler.cpp`:

```cpp
#include "spotify/CoverScaler.h"

#include <algorithm>

namespace CoverScaler {

void downscale(const uint16_t* source, int sourceWidth, int sourceHeight, uint16_t* target, int size)
{
    for (int y = 0; y < size; y++) {
        int top = y * sourceHeight / size;
        int bottom = std::max(top + 1, (y + 1) * sourceHeight / size);

        for (int x = 0; x < size; x++) {
            int left = x * sourceWidth / size;
            int right = std::max(left + 1, (x + 1) * sourceWidth / size);

            uint32_t red = 0;
            uint32_t green = 0;
            uint32_t blue = 0;
            uint32_t count = 0;
            for (int row = top; row < bottom; row++) {
                for (int column = left; column < right; column++) {
                    uint16_t pixel = source[row * sourceWidth + column];
                    red += pixel >> 11;
                    green += (pixel >> 5) & 0x3F;
                    blue += pixel & 0x1F;
                    count++;
                }
            }

            target[y * size + x] = static_cast<uint16_t>(((red / count) << 11) | ((green / count) << 5) | (blue / count));
        }
    }
}

}
```

`firmware/src/spotify/DominantColor.h`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace DominantColor {

constexpr uint32_t FALLBACK = 0x34322F;
// Com o texto #F5F1EA (luminância 0,88), 0,15 dá contraste 4,65 — acima do 4,5 da WCAG para texto comum.
constexpr double MAX_LUMINANCE = 0.15;

uint32_t of(const uint16_t* pixels, size_t count);
double luminance(uint32_t rgb);

}
```

`firmware/src/spotify/DominantColor.cpp`:

```cpp
#include "spotify/DominantColor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int BINS_PER_CHANNEL = 16;
constexpr size_t SAMPLE_STEP = 4;
constexpr int DARKEST_USEFUL = 24;
constexpr int BRIGHTEST_USEFUL = 232;
constexpr double DARKEN_FACTOR = 0.9;

struct Rgb {
    int red;
    int green;
    int blue;
};

struct Bin {
    uint32_t count = 0;
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
};

Rgb expand(uint16_t pixel)
{
    int red5 = pixel >> 11;
    int green6 = (pixel >> 5) & 0x3F;
    int blue5 = pixel & 0x1F;
    return Rgb{(red5 << 3) | (red5 >> 2), (green6 << 2) | (green6 >> 4), (blue5 << 3) | (blue5 >> 2)};
}

uint32_t pack(Rgb color)
{
    return (static_cast<uint32_t>(color.red) << 16) | (static_cast<uint32_t>(color.green) << 8)
        | static_cast<uint32_t>(color.blue);
}

Rgb unpack(uint32_t rgb)
{
    return Rgb{static_cast<int>((rgb >> 16) & 0xFF), static_cast<int>((rgb >> 8) & 0xFF), static_cast<int>(rgb & 0xFF)};
}

double linear(int channel)
{
    double value = channel / 255.0;
    return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

// Quase preto e quase branco são fundo e bordas da capa; a cor do álbum está no resto.
bool isUseful(Rgb color)
{
    int brightest = std::max({color.red, color.green, color.blue});
    int darkest = std::min({color.red, color.green, color.blue});
    return brightest >= DARKEST_USEFUL && darkest <= BRIGHTEST_USEFUL;
}

size_t binOf(Rgb color)
{
    return static_cast<size_t>((color.red >> 4) * BINS_PER_CHANNEL * BINS_PER_CHANNEL + (color.green >> 4) * BINS_PER_CHANNEL
        + (color.blue >> 4));
}

}

namespace DominantColor {

double luminance(uint32_t rgb)
{
    Rgb color = unpack(rgb);
    return 0.2126 * linear(color.red) + 0.7152 * linear(color.green) + 0.0722 * linear(color.blue);
}

uint32_t of(const uint16_t* pixels, size_t count)
{
    std::vector<Bin> bins(BINS_PER_CHANNEL * BINS_PER_CHANNEL * BINS_PER_CHANNEL);
    for (size_t index = 0; index < count; index += SAMPLE_STEP) {
        Rgb color = expand(pixels[index]);
        if (!isUseful(color)) {
            continue;
        }

        Bin& bin = bins[binOf(color)];
        bin.count++;
        bin.red += color.red;
        bin.green += color.green;
        bin.blue += color.blue;
    }

    auto fullest = std::max_element(bins.begin(), bins.end(), [](const Bin& left, const Bin& right) {
        return left.count < right.count;
    });
    if (fullest->count == 0) {
        return FALLBACK;
    }

    Rgb color{static_cast<int>(fullest->red / fullest->count), static_cast<int>(fullest->green / fullest->count),
        static_cast<int>(fullest->blue / fullest->count)};
    while (luminance(pack(color)) > MAX_LUMINANCE) {
        color.red = static_cast<int>(color.red * DARKEN_FACTOR);
        color.green = static_cast<int>(color.green * DARKEN_FACTOR);
        color.blue = static_cast<int>(color.blue * DARKEN_FACTOR);
    }
    return pack(color);
}

}
```

`firmware/src/spotify/CoverArt.h`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "spotify/DominantColor.h"

constexpr int COVER_SIZE = 200;

// Pixels RGB565 já em 200×200. O vector passa de 4 KB, então o malloc do core o coloca na PSRAM.
struct CoverArt {
    std::string albumId;
    std::vector<uint16_t> pixels;
    uint32_t color = DominantColor::FALLBACK;
};
```

- [ ] **Step 4: Rodar e ver passar**

Run: `cd firmware && pio test -e native -f "spotify/*"`
Expected: todos verdes (`test_cover_scaler` 4, `test_dominant_color` 4).

- [ ] **Step 5: Commit**

```bash
git add firmware/src/spotify firmware/test/spotify
git commit -m "feat(spotify): redução da capa e cor predominante com contraste garantido

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Cliente HTTPS comum (extraído do `MarketFeed`)

Refactor sem mudança de comportamento na cripto: o que o `MarketFeed` faz com `NetworkClientSecure` + `HTTPClient` +
corpo na PSRAM vira `support/device/Https`, que o Spotify também usa (com método, token, corpo e `Retry-After`).

**Files:**
- Create: `firmware/src/support/device/Https.h`, `firmware/src/support/device/Https.cpp`
- Modify: `firmware/src/crypto/device/MarketFeed.h`, `firmware/src/crypto/device/MarketFeed.cpp`

**Interfaces:**
- Produces:
  - `class PsramBuffer : public Stream { static std::unique_ptr<PsramBuffer> allocate(size_t capacity);
    std::string_view view() const; … }`
  - `struct HttpsRequest { const char* method = "GET"; std::string url; std::string bearerToken; std::string contentType;
    std::string payload; size_t maxBodyBytes = 64 * 1024; };`
  - `struct HttpsResponse { int status = 0; std::unique_ptr<PsramBuffer> body; std::string retryAfter; };`
  - `namespace Https { HttpsResponse send(const HttpsRequest& request); }` — `status <= 0` é falha de rede/TLS.

- [ ] **Step 1: Implementar `Https`**

`firmware/src/support/device/Https.h`:

```cpp
#pragma once

#include <Stream.h>

#include <memory>
#include <string>
#include <string_view>

// Corpo de resposta na PSRAM: respostas de API passam de vários KB, grandes demais para a heap interna. É um Stream
// para o HTTPClient escrever nele já sem a codificação chunked.
class PsramBuffer : public Stream {
public:
    static std::unique_ptr<PsramBuffer> allocate(size_t capacity);
    ~PsramBuffer() override;

    std::string_view view() const;

    size_t write(uint8_t byte) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    int available() override;
    int read() override;
    int peek() override;

private:
    PsramBuffer(char* data, size_t capacity);

    char* bytes;
    size_t room;
    size_t used = 0;
};

struct HttpsRequest {
    const char* method = "GET";
    std::string url;
    std::string bearerToken;
    std::string contentType;
    std::string payload;
    size_t maxBodyBytes = 64 * 1024;
};

struct HttpsResponse {
    int status = 0;
    std::unique_ptr<PsramBuffer> body;
    std::string retryAfter;
};

namespace Https {

HttpsResponse send(const HttpsRequest& request);

}
```

`firmware/src/support/device/Https.cpp`:

```cpp
#include "support/device/Https.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr uint16_t TIMEOUT_MILLIS = 5000;
const char* COLLECTED_HEADERS[] = {"Retry-After"};

}

std::unique_ptr<PsramBuffer> PsramBuffer::allocate(size_t capacity)
{
    char* bytes = static_cast<char*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (bytes == nullptr) {
        return nullptr;
    }

    return std::unique_ptr<PsramBuffer>(new PsramBuffer(bytes, capacity));
}

PsramBuffer::PsramBuffer(char* data, size_t capacity)
    : bytes(data)
    , room(capacity)
{
}

PsramBuffer::~PsramBuffer()
{
    heap_caps_free(bytes);
}

std::string_view PsramBuffer::view() const
{
    return std::string_view(bytes, used);
}

size_t PsramBuffer::write(uint8_t byte)
{
    return write(&byte, 1);
}

size_t PsramBuffer::write(const uint8_t* buffer, size_t size)
{
    size_t accepted = std::min(size, room - used);
    memcpy(bytes + used, buffer, accepted);
    used += accepted;
    return accepted;
}

int PsramBuffer::available()
{
    return 0;
}

int PsramBuffer::read()
{
    return -1;
}

int PsramBuffer::peek()
{
    return -1;
}

namespace Https {

HttpsResponse send(const HttpsRequest& request)
{
    HttpsResponse response;

    NetworkClientSecure client;
    // Certificados raiz embutidos no core: a conexão é verificada de verdade, sem setInsecure.
    client.useBuiltinCACertBundle();
    client.setTimeout(TIMEOUT_MILLIS);

    HTTPClient http;
    http.setTimeout(TIMEOUT_MILLIS);
    http.setConnectTimeout(TIMEOUT_MILLIS);
    if (!http.begin(client, request.url.c_str())) {
        return response;
    }

    http.collectHeaders(COLLECTED_HEADERS, 1);
    if (!request.bearerToken.empty()) {
        http.addHeader("Authorization", ("Bearer " + request.bearerToken).c_str());
    }
    if (!request.contentType.empty()) {
        http.addHeader("Content-Type", request.contentType.c_str());
    }
    // O HTTPClient só manda Content-Length quando há corpo; o Spotify recusa PUT e POST vazios sem ele (411).
    if (request.payload.empty() && strcmp(request.method, "GET") != 0) {
        http.addHeader("Content-Length", "0");
    }

    uint8_t* payload = reinterpret_cast<uint8_t*>(const_cast<char*>(request.payload.data()));
    response.status = http.sendRequest(request.method, payload, request.payload.size());
    response.retryAfter = http.header("Retry-After").c_str();

    if (response.status > 0) {
        std::unique_ptr<PsramBuffer> body = PsramBuffer::allocate(request.maxBodyBytes);
        if (body && http.writeToStream(body.get()) > 0) {
            response.body = std::move(body);
        }
    }
    http.end();

    return response;
}

}
```

- [ ] **Step 2: `MarketFeed` passa a usar `Https`**

`firmware/src/crypto/device/MarketFeed.h` — substitua a classe interna `Body` por `PsramBuffer`:

```cpp
#pragma once

#include <cstdint>
#include <memory>

#include "crypto/MarketState.h"
#include "support/Published.h"
#include "support/device/Https.h"

// Busca as cotações numa tarefa própria (núcleo 0) e publica para a UI. Uma falha numa API não derruba as outras.
class MarketFeed {
public:
    void begin();
    const Published<MarketState>& state() const;

private:
    static void run(void* self);
    void loop();
    void poll(uint32_t nowMillis, bool wifiConnected);
    std::unique_ptr<PsramBuffer> fetch(const char* url);
    bool refreshCrypto();
    bool refreshDollar();
    void markOffline();

    Published<MarketState> published;
    MarketState current;
    uint32_t lastPollMillis = 0;
    uint32_t intervalMillis = 0;
    bool rateLimited = false;
};
```

Em `firmware/src/crypto/device/MarketFeed.cpp`: apague as definições de `MarketFeed::Body::*` e os includes de
`HTTPClient.h`, `NetworkClientSecure.h`, `esp_heap_caps.h` e `cstring`; apague `MAX_BODY_BYTES` e `TIMEOUT_MILLIS`;
troque `std::unique_ptr<Body>` por `std::unique_ptr<PsramBuffer>` em `refreshCrypto` e `refreshDollar`; e troque o
corpo de `fetch` por:

```cpp
std::unique_ptr<PsramBuffer> MarketFeed::fetch(const char* url)
{
    HttpsRequest request;
    request.url = url;

    HttpsResponse response = Https::send(request);
    if (response.status == TOO_MANY_REQUESTS) {
        rateLimited = true;
    }
    if (response.status != HTTP_OK) {
        return nullptr;
    }

    return std::move(response.body);
}
```

com `constexpr int HTTP_OK = 200;` junto das outras constantes do arquivo.

- [ ] **Step 3: Testes do PC e build da placa**

Run: `cd firmware && pio test -e native` (todos verdes, nenhum teste mudou) e, no PowerShell,
`cd firmware; pio run -e board -t upload --upload-port COM8`
Expected: compila; a página de cripto continua preenchendo em até 1 min (refactor sem mudança de comportamento).

- [ ] **Step 4: Commit**

```bash
git add firmware/src/support firmware/src/crypto
git commit -m "refactor(support): cliente HTTPS comum com corpo na PSRAM, extraído do MarketFeed

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Cofre e login na placa (`SpotifyVault`, `SpotifyAuth`)

**Files:**
- Create: `firmware/src/spotify/storage/SpotifyVault.h`, `firmware/src/spotify/storage/SpotifyVault.cpp`
- Create: `firmware/src/spotify/device/SpotifyAuth.h`, `firmware/src/spotify/device/SpotifyAuth.cpp`

**Interfaces:**
- Consumes: `AuthFlow::*` (Task 2), `Pkce::*` (Task 1), `TokenResponse::parse` (Task 4), `Https::send` (Task 7).
- Produces:
  - `class SpotifyVault { void begin(); std::string clientId() const; void saveClientId(const std::string&);
    std::string refreshToken() const; void saveRefreshToken(const std::string&); void forgetRefreshToken(); }`
  - `class SpotifyAuth { explicit SpotifyAuth(SpotifyVault&); bool connected() const;
    std::string startLogin(const std::string& ipAddress, uint32_t nowMillis); std::string loginUrl(uint32_t nowMillis) const;
    AuthFlow::CallbackCheck acceptCallback(const std::string& code, const std::string& state, uint32_t nowMillis);
    bool exchangeWaitingCode(uint32_t nowMillis); std::optional<std::string> accessToken(uint32_t nowMillis);
    void invalidateAccessToken(); void disconnect(); }`
    — `acceptCallback` roda na tarefa do servidor web e só guarda o código; a troca (HTTPS) roda na tarefa do Spotify.

- [ ] **Step 1: Implementar**

`firmware/src/spotify/storage/SpotifyVault.h`:

```cpp
#pragma once

#include <Preferences.h>

#include <mutex>
#include <string>

// Client ID e refresh token na NVS. O access token nunca vem para cá: vive só na RAM.
class SpotifyVault {
public:
    void begin();
    std::string clientId() const;
    void saveClientId(const std::string& clientId);
    std::string refreshToken() const;
    void saveRefreshToken(const std::string& refreshToken);
    void forgetRefreshToken();

private:
    mutable std::mutex mutex;
    Preferences preferences;
    std::string storedClientId;
    std::string storedRefreshToken;
};
```

`firmware/src/spotify/storage/SpotifyVault.cpp`:

```cpp
#include "spotify/storage/SpotifyVault.h"

#include "spotify/AuthFlow.h"

namespace {

constexpr const char* NAMESPACE = "spotify";
constexpr const char* CLIENT_KEY = "client";
constexpr const char* REFRESH_KEY = "refresh";

}

void SpotifyVault::begin()
{
    std::lock_guard<std::mutex> lock(mutex);

    preferences.begin(NAMESPACE, false);
    storedClientId = preferences.getString(CLIENT_KEY, AuthFlow::DEFAULT_CLIENT_ID).c_str();
    storedRefreshToken = preferences.getString(REFRESH_KEY, "").c_str();
}

std::string SpotifyVault::clientId() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return storedClientId;
}

void SpotifyVault::saveClientId(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex);
    storedClientId = clientId;
    preferences.putString(CLIENT_KEY, clientId.c_str());
}

std::string SpotifyVault::refreshToken() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return storedRefreshToken;
}

void SpotifyVault::saveRefreshToken(const std::string& refreshToken)
{
    std::lock_guard<std::mutex> lock(mutex);
    storedRefreshToken = refreshToken;
    preferences.putString(REFRESH_KEY, refreshToken.c_str());
}

void SpotifyVault::forgetRefreshToken()
{
    std::lock_guard<std::mutex> lock(mutex);
    storedRefreshToken.clear();
    preferences.remove(REFRESH_KEY);
}
```

`firmware/src/spotify/device/SpotifyAuth.h`:

```cpp
#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include "spotify/AuthFlow.h"
#include "spotify/TokenResponse.h"
#include "spotify/storage/SpotifyVault.h"

class SpotifyAuth {
public:
    explicit SpotifyAuth(SpotifyVault& vault);

    bool connected() const;
    std::string startLogin(const std::string& ipAddress, uint32_t nowMillis);
    std::string loginUrl(uint32_t nowMillis) const;
    AuthFlow::CallbackCheck acceptCallback(const std::string& code, const std::string& state, uint32_t nowMillis);
    bool exchangeWaitingCode(uint32_t nowMillis);
    std::optional<std::string> accessToken(uint32_t nowMillis);
    void invalidateAccessToken();
    void disconnect();

private:
    std::optional<TokenResult> requestToken(const std::string& body);
    void remember(const TokenResult& result, uint32_t nowMillis);

    SpotifyVault& vault;
    mutable std::mutex mutex;
    AuthFlow::Pending pending;
    std::string pendingUrl;
    std::string waitingCode;
    std::string waitingVerifier;
    std::string access;
    uint32_t accessExpiresMillis = 0;
};
```

`firmware/src/spotify/device/SpotifyAuth.cpp`:

```cpp
#include "spotify/device/SpotifyAuth.h"

#include <Arduino.h>
#include <esp_random.h>

#include <algorithm>

#include "spotify/Pkce.h"
#include "support/device/Https.h"

namespace {

constexpr const char* TOKEN_URL = "https://accounts.spotify.com/api/token";
constexpr const char* FORM = "application/x-www-form-urlencoded";
constexpr uint16_t HTTP_PORT = 80;
constexpr size_t NONCE_BYTES = 16;
constexpr uint32_t EARLY_REFRESH_SECONDS = 60;
// Gera um link novo antes do antigo vencer, para o QR da tela nunca mostrar um link que o Spotify vai recusar.
constexpr uint32_t LOGIN_URL_LIFETIME_MILLIS = 9 * 60 * 1000;

bool reached(uint32_t nowMillis, uint32_t deadline)
{
    return static_cast<int32_t>(nowMillis - deadline) >= 0;
}

}

SpotifyAuth::SpotifyAuth(SpotifyVault& vault)
    : vault(vault)
{
}

bool SpotifyAuth::connected() const
{
    return !vault.refreshToken().empty();
}

std::string SpotifyAuth::startLogin(const std::string& ipAddress, uint32_t nowMillis)
{
    uint8_t verifierBytes[Pkce::VERIFIER_LENGTH];
    uint8_t nonceBytes[NONCE_BYTES];
    esp_fill_random(verifierBytes, sizeof(verifierBytes));
    esp_fill_random(nonceBytes, sizeof(nonceBytes));

    AuthFlow::Pending fresh;
    fresh.verifier = Pkce::verifier(verifierBytes);
    fresh.nonce = Pkce::hex(nonceBytes, sizeof(nonceBytes));
    fresh.createdMillis = nowMillis;
    std::string state = AuthFlow::state(ipAddress, HTTP_PORT, fresh.nonce);
    std::string url = AuthFlow::authorizeUrl(vault.clientId(), Pkce::challenge(fresh.verifier), state);

    std::lock_guard<std::mutex> lock(mutex);
    pending = fresh;
    pendingUrl = url;
    return url;
}

std::string SpotifyAuth::loginUrl(uint32_t nowMillis) const
{
    std::lock_guard<std::mutex> lock(mutex);
    bool fresh = !pendingUrl.empty() && nowMillis - pending.createdMillis < LOGIN_URL_LIFETIME_MILLIS;
    return fresh ? pendingUrl : "";
}

AuthFlow::CallbackCheck SpotifyAuth::acceptCallback(const std::string& code, const std::string& state,
    uint32_t nowMillis)
{
    std::lock_guard<std::mutex> lock(mutex);

    AuthFlow::CallbackCheck check = AuthFlow::check(pending, state, nowMillis);
    if (check == AuthFlow::CallbackCheck::Accepted) {
        waitingCode = code;
        waitingVerifier = pending.verifier;
        // O mesmo link não serve duas vezes.
        pending = AuthFlow::Pending{};
        pendingUrl.clear();
    }
    return check;
}

bool SpotifyAuth::exchangeWaitingCode(uint32_t nowMillis)
{
    std::string code;
    std::string verifier;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (waitingCode.empty()) {
            return false;
        }
        code.swap(waitingCode);
        verifier.swap(waitingVerifier);
    }

    std::optional<TokenResult> result = requestToken(AuthFlow::tokenRequestBody(vault.clientId(), code, verifier));
    if (!result || !result->ok || !result->refreshToken) {
        log_w("spotify: troca do código falhou: %s", result ? result->error.c_str() : "sem resposta");
        return false;
    }

    remember(*result, nowMillis);
    return true;
}

std::optional<std::string> SpotifyAuth::accessToken(uint32_t nowMillis)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!access.empty() && !reached(nowMillis, accessExpiresMillis)) {
            return access;
        }
    }

    std::string refresh = vault.refreshToken();
    if (refresh.empty()) {
        return std::nullopt;
    }

    std::optional<TokenResult> result = requestToken(AuthFlow::refreshRequestBody(vault.clientId(), refresh));
    if (result && result->invalidGrant) {
        // Revogado ou vencido: não adianta tentar de novo, a tela passa a pedir login.
        disconnect();
        return std::nullopt;
    }
    if (!result || !result->ok) {
        return std::nullopt;
    }

    remember(*result, nowMillis);
    return result->accessToken;
}

void SpotifyAuth::invalidateAccessToken()
{
    std::lock_guard<std::mutex> lock(mutex);
    access.clear();
}

void SpotifyAuth::disconnect()
{
    vault.forgetRefreshToken();

    std::lock_guard<std::mutex> lock(mutex);
    access.clear();
}

std::optional<TokenResult> SpotifyAuth::requestToken(const std::string& body)
{
    HttpsRequest request;
    request.method = "POST";
    request.url = TOKEN_URL;
    request.contentType = FORM;
    request.payload = body;

    HttpsResponse response = Https::send(request);
    if (!response.body) {
        return std::nullopt;
    }
    return TokenResponse::parse(response.body->view());
}

void SpotifyAuth::remember(const TokenResult& result, uint32_t nowMillis)
{
    // Sem refresh token novo na resposta, o antigo continua valendo.
    if (result.refreshToken) {
        vault.saveRefreshToken(*result.refreshToken);
    }

    uint32_t usableSeconds = result.expiresInSeconds - std::min(result.expiresInSeconds, EARLY_REFRESH_SECONDS);

    std::lock_guard<std::mutex> lock(mutex);
    access = result.accessToken;
    accessExpiresMillis = nowMillis + usableSeconds * 1000;
}
```

- [ ] **Step 2: Compilar**

Run: `cd firmware && pio test -e native` (verdes; nada de lógica pura mudou) e, no PowerShell, `pio run -e board`
Expected: compila (nada usa as classes ainda).

- [ ] **Step 3: Commit**

```bash
git add firmware/src/spotify
git commit -m "feat(spotify): cofre na NVS e login com troca e renovação de tokens

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Tarefa do Spotify e download da capa (`SpotifyClient`, `CoverFetcher`)

**Files:**
- Create: `firmware/src/spotify/device/CoverFetcher.h`, `firmware/src/spotify/device/CoverFetcher.cpp`
- Create: `firmware/src/spotify/device/SpotifyClient.h`, `firmware/src/spotify/device/SpotifyClient.cpp`

**Interfaces:**
- Consumes: `SpotifyAuth` (Task 8), `PlayerParser`, `PollSchedule`, `Progress`, `CoverScaler`, `DominantColor`,
  `CoverArt` (Tasks 4–6), `Https` (Task 7), `Published<T>`.
- Produces:
  - `class CoverFetcher { std::optional<CoverArt> fetch(const std::string& url); }`
  - `enum class CommandKind : uint8_t { Play, Pause, Next, Previous, Volume }; struct Command { CommandKind kind; uint8_t volume = 0; };`
  - `class SpotifyClient { explicit SpotifyClient(SpotifyAuth&); void begin(); const Published<PlayerState>& state() const;
    const Published<CoverArt>& cover() const; void send(Command command); void watch(bool watching); }`
    — `send` e `watch` são chamados do núcleo 1 (UI) e não bloqueiam.

- [ ] **Step 1: `CoverFetcher`**

`firmware/src/spotify/device/CoverFetcher.h`:

```cpp
#pragma once

#include <JPEGDEC.h>

#include <memory>
#include <optional>
#include <string>

#include "spotify/CoverArt.h"

// Baixa a capa só quando o álbum muda, decodifica e reduz para 200×200 uma vez.
class CoverFetcher {
public:
    std::optional<CoverArt> fetch(const std::string& url);

private:
    static int copyBlock(JPEGDRAW* block);

    std::unique_ptr<JPEGDEC> decoder;
};
```

`firmware/src/spotify/device/CoverFetcher.cpp`:

```cpp
#include "spotify/device/CoverFetcher.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "spotify/CoverScaler.h"
#include "spotify/DominantColor.h"
#include "support/device/Https.h"

namespace {

constexpr size_t MAX_COVER_BYTES = 256 * 1024;
constexpr int HTTP_OK = 200;
// Capas de 640 px decodificam pela metade direto no JPEGDEC: menos memória e o resultado reduz igual.
constexpr int HALVE_FROM_WIDTH = 600;

struct Target {
    uint16_t* pixels;
    int width;
    int height;
};

}

std::optional<CoverArt> CoverFetcher::fetch(const std::string& url)
{
    HttpsRequest request;
    request.url = url;
    request.maxBodyBytes = MAX_COVER_BYTES;

    HttpsResponse response = Https::send(request);
    if (response.status != HTTP_OK || !response.body) {
        return std::nullopt;
    }

    // O JPEGDEC tem buffers internos de ~17 KB; alocado sob demanda, vai para a PSRAM.
    if (!decoder) {
        decoder = std::make_unique<JPEGDEC>();
    }

    std::string_view jpeg = response.body->view();
    uint8_t* data = reinterpret_cast<uint8_t*>(const_cast<char*>(jpeg.data()));
    if (!decoder->openRAM(data, static_cast<int>(jpeg.size()), copyBlock)) {
        return std::nullopt;
    }

    int scale = decoder->getWidth() >= HALVE_FROM_WIDTH ? JPEG_SCALE_HALF : 0;
    int shift = scale == 0 ? 0 : 1;
    int width = decoder->getWidth() >> shift;
    int height = decoder->getHeight() >> shift;
    std::vector<uint16_t> decoded(static_cast<size_t>(width) * height);
    Target target{decoded.data(), width, height};

    decoder->setUserPointer(&target);
    decoder->setPixelType(RGB565_LITTLE_ENDIAN);
    bool decodedOk = decoder->decode(0, 0, scale) == 1;
    decoder->close();
    if (!decodedOk) {
        return std::nullopt;
    }

    CoverArt art;
    art.pixels.resize(COVER_SIZE * COVER_SIZE);
    CoverScaler::downscale(decoded.data(), width, height, art.pixels.data(), COVER_SIZE);
    art.color = DominantColor::of(art.pixels.data(), art.pixels.size());
    return art;
}

int CoverFetcher::copyBlock(JPEGDRAW* block)
{
    Target* target = static_cast<Target*>(block->pUser);

    for (int row = 0; row < block->iHeight; row++) {
        int y = block->y + row;
        int width = std::min(block->iWidth, target->width - block->x);
        if (y >= target->height || width <= 0) {
            break;
        }

        memcpy(target->pixels + y * target->width + block->x, block->pPixels + row * block->iWidth, width * 2);
    }

    return 1;
}
```

- [ ] **Step 2: `SpotifyClient`**

`firmware/src/spotify/device/SpotifyClient.h`:

```cpp
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "spotify/CoverArt.h"
#include "spotify/PlayerState.h"
#include "spotify/device/CoverFetcher.h"
#include "spotify/device/SpotifyAuth.h"
#include "support/Published.h"
#include "support/device/Https.h"

enum class CommandKind : uint8_t {
    Play,
    Pause,
    Next,
    Previous,
    Volume,
};

struct Command {
    CommandKind kind;
    uint8_t volume = 0;
};

// Tarefa própria no núcleo 0: consulta o player, executa os comandos da tela e baixa a capa. A UI só lê o que é
// publicado e manda comandos pela fila.
class SpotifyClient {
public:
    explicit SpotifyClient(SpotifyAuth& auth);

    void begin();
    const Published<PlayerState>& state() const;
    const Published<CoverArt>& cover() const;
    void send(Command command);
    void watch(bool watching);

private:
    static void run(void* self);
    void loop();
    void showLogin(uint32_t nowMillis);
    void markStale();
    void poll(uint32_t nowMillis);
    uint32_t applyPlayer(const HttpsResponse& response, uint32_t nowMillis);
    uint32_t applyNoDevice(uint32_t nowMillis);
    uint32_t applyFailure(uint32_t nowMillis);
    uint32_t interval(uint32_t nowMillis) const;
    void refreshCover();
    void handle(const Command& command, uint32_t nowMillis);
    int callPlayer(CommandKind kind, const std::string& suffix);
    void flushVolume();
    void waitForCommand(uint32_t waitMillis);
    uint32_t nextWaitMillis(uint32_t nowMillis) const;

    SpotifyAuth& auth;
    CoverFetcher covers;
    QueueHandle_t commands = nullptr;
    std::atomic<int> watchers{0};
    Published<PlayerState> published;
    Published<CoverArt> publishedCover;
    PlayerState current;
    std::string coverAlbumId;
    uint32_t nextPollMillis = 0;
    uint32_t pausedSinceMillis = 0;
    std::optional<uint8_t> pendingVolume;
    uint32_t volumeDueMillis = 0;
};
```

`firmware/src/spotify/device/SpotifyClient.cpp`:

```cpp
#include "spotify/device/SpotifyClient.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>

#include "spotify/PlayerParser.h"
#include "spotify/PollSchedule.h"
#include "spotify/Progress.h"

namespace {

constexpr const char* PLAYER_URL = "https://api.spotify.com/v1/me/player";
constexpr uint32_t TASK_STACK_BYTES = 16384;
constexpr UBaseType_t QUEUE_LENGTH = 8;
constexpr uint32_t IDLE_WAIT_MILLIS = 1000;
constexpr uint32_t RECONCILE_AFTER_MILLIS = 600;
constexpr uint32_t VOLUME_DEBOUNCE_MILLIS = 300;
constexpr uint32_t RETRY_WITHOUT_TOKEN_MILLIS = 5000;
constexpr int HTTP_OK = 200;
constexpr int NO_CONTENT = 204;
constexpr int UNAUTHORIZED = 401;
constexpr int FORBIDDEN = 403;
constexpr int NOT_FOUND = 404;
constexpr int TOO_MANY_REQUESTS = 429;

struct Endpoint {
    const char* method;
    const char* path;
};

// Na ordem de CommandKind.
constexpr Endpoint ENDPOINTS[] = {
    {"PUT", "/play"},
    {"PUT", "/pause"},
    {"POST", "/next"},
    {"POST", "/previous"},
    {"PUT", "/volume?volume_percent="},
};

bool reached(uint32_t nowMillis, uint32_t deadline)
{
    return static_cast<int32_t>(nowMillis - deadline) >= 0;
}

uint32_t remaining(uint32_t nowMillis, uint32_t deadline)
{
    return reached(nowMillis, deadline) ? 0 : deadline - nowMillis;
}

}

SpotifyClient::SpotifyClient(SpotifyAuth& auth)
    : auth(auth)
{
}

void SpotifyClient::begin()
{
    commands = xQueueCreate(QUEUE_LENGTH, sizeof(Command));
    xTaskCreatePinnedToCore(run, "spotify", TASK_STACK_BYTES, this, 1, nullptr, 0);
}

const Published<PlayerState>& SpotifyClient::state() const
{
    return published;
}

const Published<CoverArt>& SpotifyClient::cover() const
{
    return publishedCover;
}

void SpotifyClient::send(Command command)
{
    if (commands != nullptr) {
        xQueueSend(commands, &command, 0);
    }
}

void SpotifyClient::watch(bool watching)
{
    watchers += watching ? 1 : -1;
}

void SpotifyClient::run(void* self)
{
    static_cast<SpotifyClient*>(self)->loop();
}

void SpotifyClient::loop()
{
    for (;;) {
        uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            markStale();
            vTaskDelay(pdMS_TO_TICKS(IDLE_WAIT_MILLIS));
            continue;
        }

        if (auth.exchangeWaitingCode(now)) {
            nextPollMillis = now;
        }
        if (!auth.connected()) {
            showLogin(now);
            waitForCommand(IDLE_WAIT_MILLIS);
            continue;
        }

        if (reached(now, nextPollMillis)) {
            poll(now);
        }
        if (pendingVolume && reached(millis(), volumeDueMillis)) {
            flushVolume();
        }

        waitForCommand(nextWaitMillis(millis()));
    }
}

void SpotifyClient::showLogin(uint32_t nowMillis)
{
    if (current.status != PlayerStatus::Disconnected) {
        current = PlayerState{};
        coverAlbumId.clear();
        publishedCover.publish(CoverArt{});
    }

    std::string url = auth.loginUrl(nowMillis);
    if (url.empty()) {
        url = auth.startLogin(WiFi.localIP().toString().c_str(), nowMillis);
    }
    if (url != current.loginUrl) {
        current.loginUrl = url;
        published.publish(current);
    }
}

void SpotifyClient::markStale()
{
    if (current.stale || current.status == PlayerStatus::Disconnected) {
        return;
    }

    current.stale = true;
    published.publish(current);
}

void SpotifyClient::poll(uint32_t nowMillis)
{
    std::optional<std::string> token = auth.accessToken(nowMillis);
    if (!token) {
        nextPollMillis = nowMillis + RETRY_WITHOUT_TOKEN_MILLIS;
        return;
    }

    HttpsRequest request;
    request.url = PLAYER_URL;
    request.bearerToken = *token;
    HttpsResponse response = Https::send(request);
    uint32_t answeredAt = millis();

    uint32_t wait = 0;
    switch (response.status) {
    case HTTP_OK:
        wait = applyPlayer(response, answeredAt);
        break;
    case NO_CONTENT:
        wait = applyNoDevice(answeredAt);
        break;
    case UNAUTHORIZED:
        auth.invalidateAccessToken();
        wait = IDLE_WAIT_MILLIS;
        break;
    case TOO_MANY_REQUESTS:
        wait = PollSchedule::retryAfterMillis(response.retryAfter);
        break;
    default:
        wait = applyFailure(answeredAt);
        break;
    }
    nextPollMillis = answeredAt + wait;
}

uint32_t SpotifyClient::applyPlayer(const HttpsResponse& response, uint32_t nowMillis)
{
    std::string_view json = response.body ? response.body->view() : std::string_view();
    std::optional<PlayerState> parsed = PlayerParser::parse(json, nowMillis);
    if (!parsed) {
        return applyFailure(nowMillis);
    }

    parsed->volumeAllowed = parsed->device == current.device ? current.volumeAllowed : true;
    // Enquanto o dedo mexe no volume, o slider fica onde ele deixou.
    if (pendingVolume) {
        parsed->volume = *pendingVolume;
    }
    if (parsed->status == PlayerStatus::Paused && current.status != PlayerStatus::Paused) {
        pausedSinceMillis = nowMillis;
    }

    current = *parsed;
    published.publish(current);
    refreshCover();
    return interval(nowMillis);
}

uint32_t SpotifyClient::applyNoDevice(uint32_t nowMillis)
{
    current.status = PlayerStatus::NoDevice;
    current.hasItem = false;
    current.stale = false;
    published.publish(current);
    return interval(nowMillis);
}

uint32_t SpotifyClient::applyFailure(uint32_t nowMillis)
{
    current.stale = true;
    published.publish(current);
    return interval(nowMillis);
}

uint32_t SpotifyClient::interval(uint32_t nowMillis) const
{
    uint32_t pausedFor = current.status == PlayerStatus::Paused ? nowMillis - pausedSinceMillis : 0;
    return PollSchedule::intervalMillis(current.status, watchers.load() > 0, pausedFor).value_or(IDLE_WAIT_MILLIS);
}

void SpotifyClient::refreshCover()
{
    if (!current.hasItem || current.track.albumId == coverAlbumId) {
        return;
    }

    // Marca antes de baixar: uma capa que falha não é tentada de novo a cada poll do mesmo álbum.
    coverAlbumId = current.track.albumId;
    std::optional<CoverArt> art;
    if (!current.track.coverUrl.empty()) {
        art = covers.fetch(current.track.coverUrl);
    }

    CoverArt shown = art ? std::move(*art) : CoverArt{};
    shown.albumId = coverAlbumId;
    publishedCover.publish(shown);
}

void SpotifyClient::handle(const Command& command, uint32_t nowMillis)
{
    switch (command.kind) {
    case CommandKind::Volume:
        pendingVolume = command.volume;
        volumeDueMillis = nowMillis + VOLUME_DEBOUNCE_MILLIS;
        current.volume = command.volume;
        published.publish(current);
        return;
    case CommandKind::Play:
        current.status = PlayerStatus::Playing;
        current.sampledAtMillis = nowMillis;
        break;
    case CommandKind::Pause:
        current.progressMs = Progress::estimate(current, nowMillis);
        current.sampledAtMillis = nowMillis;
        current.status = PlayerStatus::Paused;
        break;
    case CommandKind::Next:
    case CommandKind::Previous:
        current.progressMs = 0;
        current.sampledAtMillis = nowMillis;
        break;
    }

    // Otimista: a tela muda na hora e o poll logo depois confirma ou desfaz.
    published.publish(current);
    callPlayer(command.kind, "");
    nextPollMillis = millis() + RECONCILE_AFTER_MILLIS;
}

int SpotifyClient::callPlayer(CommandKind kind, const std::string& suffix)
{
    std::optional<std::string> token = auth.accessToken(millis());
    if (!token) {
        return 0;
    }

    const Endpoint& endpoint = ENDPOINTS[static_cast<size_t>(kind)];
    HttpsRequest request;
    request.method = endpoint.method;
    request.url = std::string(PLAYER_URL) + endpoint.path + suffix;
    request.bearerToken = *token;

    HttpsResponse response = Https::send(request);
    if (response.status == UNAUTHORIZED) {
        auth.invalidateAccessToken();
    }
    if (response.status == NOT_FOUND) {
        applyNoDevice(millis());
    }
    return response.status;
}

void SpotifyClient::flushVolume()
{
    uint8_t volume = *pendingVolume;
    pendingVolume.reset();

    // 403 no volume: o aparelho não deixa (alguns alto-falantes e TVs); o slider some até o aparelho mudar.
    if (callPlayer(CommandKind::Volume, std::to_string(volume)) == FORBIDDEN) {
        current.volumeAllowed = false;
        published.publish(current);
    }
}

void SpotifyClient::waitForCommand(uint32_t waitMillis)
{
    Command command;
    if (xQueueReceive(commands, &command, pdMS_TO_TICKS(waitMillis)) == pdTRUE) {
        handle(command, millis());
    }
}

uint32_t SpotifyClient::nextWaitMillis(uint32_t nowMillis) const
{
    uint32_t wait = std::min(remaining(nowMillis, nextPollMillis), IDLE_WAIT_MILLIS);
    if (pendingVolume) {
        wait = std::min(wait, remaining(nowMillis, volumeDueMillis));
    }
    return wait;
}
```

- [ ] **Step 3: Compilar**

Run: `cd firmware && pio test -e native` e, no PowerShell, `pio run -e board`
Expected: verdes; compila. Se o compilador apontar `JPEG_SCALE_HALF` ou `getWidth`, confira em
`.pio/libdeps/board/JPEGDEC/src/JPEGDEC.h`.

- [ ] **Step 4: Commit**

```bash
git add firmware/src/spotify
git commit -m "feat(spotify): tarefa que consulta o player, executa comandos e baixa a capa

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Rotas e card na página web

**Files:**
- Create: `firmware/src/spotify/web/SpotifyRoutes.h`, `firmware/src/spotify/web/SpotifyRoutes.cpp`
- Modify: `firmware/web/index.html`, `firmware/web/app.js`, `firmware/web/app.css`

**Interfaces:**
- Consumes: `HttpServer` (`raw()`, `authorize()`), `SpotifyAuth`, `SpotifyVault`, `WifiLink::ip()`, `AuthFlow::*`.
- Produces:
  - `class SpotifyRoutes { SpotifyRoutes(HttpServer&, SpotifyAuth&, SpotifyVault&, const WifiLink&); void begin(); }`
  - Rotas (todas autenticadas, menos o callback):
    `GET /api/spotify` → `{"connected":true,"clientId":"…"}`;
    `POST /api/spotify/connect` `{"clientId":"…"}` → `{"url":"…"}` ou `400 {"field":"clientId","error":"Client ID inválido"}`;
    `POST /api/spotify/paste` `{"url":"…"}` → `202`, `400`/`410 {"error":"…"}`;
    `DELETE /api/spotify` → `204`;
    `GET /api/spotify/callback?code=…&state=…` → página HTML curta.

- [ ] **Step 1: Rotas**

`firmware/src/spotify/web/SpotifyRoutes.h`:

```cpp
#pragma once

#include "setup/device/WifiLink.h"
#include "setup/web/HttpServer.h"
#include "spotify/device/SpotifyAuth.h"
#include "spotify/storage/SpotifyVault.h"

class SpotifyRoutes {
public:
    SpotifyRoutes(HttpServer& http, SpotifyAuth& auth, SpotifyVault& vault, const WifiLink& wifi);

    void begin();

private:
    void callback(AsyncWebServerRequest* request);
    void status(AsyncWebServerRequest* request);
    void connect(AsyncWebServerRequest* request, const uint8_t* data, size_t length);
    void paste(AsyncWebServerRequest* request, const uint8_t* data, size_t length);
    void disconnect(AsyncWebServerRequest* request);

    HttpServer& http;
    SpotifyAuth& auth;
    SpotifyVault& vault;
    const WifiLink& wifi;
};
```

`firmware/src/spotify/web/SpotifyRoutes.cpp`:

```cpp
#include "spotify/web/SpotifyRoutes.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

constexpr size_t MAX_CLIENT_ID_LENGTH = 64;
constexpr size_t PAGE_BYTES = 768;
constexpr const char* PAGE = "<!doctype html><html lang=\"pt-BR\"><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Claudinho</title>"
    "<body style=\"margin:0;background:#1F1E1C;color:#F5F1EA;font:18px/1.5 system-ui,sans-serif\">"
    "<main style=\"max-width:560px;margin:0 auto;padding:32px 24px\"><h1 style=\"font-weight:500\">%s</h1>"
    "<p style=\"color:#A8A29A\">%s</p></main></body></html>";

struct Outcome {
    int status;
    const char* title;
    const char* text;
};

// Na ordem de AuthFlow::CallbackCheck.
constexpr Outcome OUTCOMES[] = {
    {200, "Spotify conectando", "Pode fechar esta aba. Em alguns segundos a música aparece na tela do Claudinho."},
    {410, "Link vencido", "O login demorou mais de 10 minutos. Toque em Conectar de novo em claudinho.local."},
    {400, "Link de outro login", "Este link não foi gerado por este Claudinho. Toque em Conectar de novo em claudinho.local."},
};

void sendPage(AsyncWebServerRequest* request, int status, const char* title, const char* text)
{
    char buffer[PAGE_BYTES];
    snprintf(buffer, sizeof(buffer), PAGE, title, text);
    request->send(status, "text/html; charset=utf-8", buffer);
}

void sendJson(AsyncWebServerRequest* request, int status, const JsonDocument& document)
{
    String body;
    serializeJson(document, body);
    request->send(status, "application/json", body);
}

void sendError(AsyncWebServerRequest* request, int status, const char* field, const char* error)
{
    JsonDocument document;
    if (field != nullptr) {
        document["field"] = field;
    }
    document["error"] = error;
    sendJson(request, status, document);
}

bool looksLikeClientId(const std::string& clientId)
{
    return !clientId.empty() && clientId.size() <= MAX_CLIENT_ID_LENGTH
        && std::all_of(clientId.begin(), clientId.end(), [](char character) {
               return std::isalnum(static_cast<unsigned char>(character));
           });
}

const Outcome& outcomeOf(AuthFlow::CallbackCheck check)
{
    return OUTCOMES[static_cast<size_t>(check)];
}

}

SpotifyRoutes::SpotifyRoutes(HttpServer& http, SpotifyAuth& auth, SpotifyVault& vault, const WifiLink& wifi)
    : http(http)
    , auth(auth)
    , vault(vault)
    , wifi(wifi)
{
}

void SpotifyRoutes::begin()
{
    AsyncWebServer& server = http.raw();

    // O ESPAsyncWebServer casa "/api/spotify" também com "/api/spotify/…": o callback tem que vir antes do status.
    server.on("/api/spotify/callback", HTTP_GET, [this](AsyncWebServerRequest* request) { callback(request); });
    server.on("/api/spotify/connect", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            connect(request, data, length);
        });
    server.on("/api/spotify/paste", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            paste(request, data, length);
        });
    server.on("/api/spotify", HTTP_GET, [this](AsyncWebServerRequest* request) { status(request); });
    server.on("/api/spotify", HTTP_DELETE, [this](AsyncWebServerRequest* request) { disconnect(request); });
}

// Aberto de propósito: chega do celular que fez o login, sem a senha da página. O nonce de 128 bits no state é a
// proteção, e ele vale uma vez só.
void SpotifyRoutes::callback(AsyncWebServerRequest* request)
{
    if (request->hasParam("error")) {
        sendPage(request, 400, "Login cancelado",
            "Nada foi alterado. Se quiser, toque em Conectar de novo em claudinho.local.");
        return;
    }
    if (!request->hasParam("code") || !request->hasParam("state")) {
        sendPage(request, 400, "Link incompleto", "Toque em Conectar de novo em claudinho.local.");
        return;
    }

    std::string code = request->getParam("code")->value().c_str();
    std::string state = request->getParam("state")->value().c_str();
    const Outcome& outcome = outcomeOf(auth.acceptCallback(code, state, millis()));
    sendPage(request, outcome.status, outcome.title, outcome.text);
}

void SpotifyRoutes::status(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument document;
    document["connected"] = auth.connected();
    document["clientId"] = vault.clientId();
    sendJson(request, 200, document);
}

void SpotifyRoutes::connect(AsyncWebServerRequest* request, const uint8_t* data, size_t length)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument body;
    deserializeJson(body, data, length);
    std::string clientId = body["clientId"] | "";
    if (!clientId.empty() && !looksLikeClientId(clientId)) {
        sendError(request, 400, "clientId", "Client ID inválido");
        return;
    }
    if (!clientId.empty()) {
        vault.saveClientId(clientId);
    }

    JsonDocument document;
    document["url"] = auth.startLogin(wifi.ip(), millis());
    sendJson(request, 200, document);
}

void SpotifyRoutes::paste(AsyncWebServerRequest* request, const uint8_t* data, size_t length)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument body;
    deserializeJson(body, data, length);
    std::optional<AuthFlow::Callback> callback = AuthFlow::fromPastedUrl(body["url"] | "");
    if (!callback) {
        sendError(request, 400, "url", "link sem code e state");
        return;
    }

    AuthFlow::CallbackCheck check = auth.acceptCallback(callback->code, callback->state, millis());
    if (check == AuthFlow::CallbackCheck::Accepted) {
        request->send(202);
        return;
    }

    const Outcome& outcome = outcomeOf(check);
    sendError(request, outcome.status, "url", outcome.title);
}

void SpotifyRoutes::disconnect(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    auth.disconnect();
    request->send(204);
}
```

- [ ] **Step 2: Card na página**

Em `firmware/web/index.html`, depois da seção `#birthday`:

```html
  <section id="spotify" class="card hidden">
    <h2>Spotify</h2>
    <p class="hint" id="spotify-status">Carregando…</p>
    <input type="text" id="spotify-client" placeholder="Client ID" autocomplete="off" spellcheck="false">
    <button type="button" id="spotify-connect">Conectar</button>
    <button type="button" id="spotify-disconnect" class="secondary hidden">Desconectar</button>
    <p class="error" id="spotify-message"></p>
    <details>
      <summary>O login não voltou para cá?</summary>
      <p class="hint">Copie o endereço da página onde o login parou e cole aqui.</p>
      <input type="text" id="spotify-paste" placeholder="https://…/callback/?code=…" autocomplete="off">
      <button type="button" id="spotify-paste-send">Usar este link</button>
    </details>
  </section>
```

Em `firmware/web/app.css`, troque o seletor `input[type=password] {` por `input[type=password], input[type=text] {`
e acrescente no fim:

```css
.secondary { background: transparent; color: var(--text); border: 1px solid #4a4845; }
details { margin-top: 8px; }
summary { cursor: pointer; color: var(--muted); font-size: 14px; margin-bottom: 8px; }
```

No fim de `firmware/web/app.js`:

```js
const spotifyStatus = document.getElementById('spotify-status');
const spotifyClient = document.getElementById('spotify-client');
const spotifyConnect = document.getElementById('spotify-connect');
const spotifyDisconnect = document.getElementById('spotify-disconnect');
const spotifyMessage = document.getElementById('spotify-message');
const spotifyPaste = document.getElementById('spotify-paste');

async function refreshSpotify() {
    const response = await fetch('/api/spotify');
    if (!response.ok) {
        return;
    }

    const spotify = await response.json();
    spotifyStatus.textContent = spotify.connected
        ? 'Conectado. A música aparece na tela do Claudinho.'
        : 'Desconectado. Conecte para ver e controlar a música na tela.';
    spotifyClient.value = spotify.clientId;
    spotifyConnect.textContent = spotify.connected ? 'Conectar outra conta' : 'Conectar';
    spotifyDisconnect.classList.toggle('hidden', !spotify.connected);
}

spotifyConnect.addEventListener('click', async () => {
    spotifyMessage.textContent = '';
    const response = await fetch('/api/spotify/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ clientId: spotifyClient.value.trim() }),
    });
    const body = await response.json().catch(() => ({ error: 'não deu certo' }));
    if (!response.ok) {
        spotifyMessage.textContent = body.error;
        return;
    }

    location.href = body.url;
});

spotifyDisconnect.addEventListener('click', async () => {
    if (!confirm('Desconectar o Spotify deste Claudinho?')) {
        return;
    }
    await fetch('/api/spotify', { method: 'DELETE' });
    refreshSpotify();
});

document.getElementById('spotify-paste-send').addEventListener('click', async () => {
    spotifyMessage.textContent = '';
    const response = await fetch('/api/spotify/paste', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url: spotifyPaste.value.trim() }),
    });
    if (!response.ok) {
        const body = await response.json().catch(() => ({ error: 'não deu certo' }));
        spotifyMessage.textContent = body.error;
        return;
    }

    spotifyPaste.value = '';
    spotifyStatus.textContent = 'Link aceito. Em alguns segundos a música aparece na tela.';
    setTimeout(refreshSpotify, 4000);
});

document.getElementById('spotify').classList.toggle('hidden', !status.adminSet);
if (status.adminSet) {
    refreshSpotify();
}
```

- [ ] **Step 3: Compilar**

Run: `cd firmware && node --test "test/web/*.test.mjs"` (verdes) e, no PowerShell, `pio run -e board`
Expected: compila (as rotas entram no `main.cpp` na Task 11).

- [ ] **Step 4: Commit**

```bash
git add firmware/src/spotify firmware/web
git commit -m "feat(spotify): rotas de login e card Spotify na página

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: Página do Spotify, pílula no Início e montagem

**Files:**
- Create: `firmware/src/spotify/screen/SpotifyPage.h`, `firmware/src/spotify/screen/SpotifyPage.cpp`
- Create: `firmware/src/spotify/screen/NowPlayingPill.h`, `firmware/src/spotify/screen/NowPlayingPill.cpp`
- Modify: `firmware/src/photos/screen/HomePage.h`, `firmware/src/photos/screen/HomePage.cpp` (camada de sobreposição)
- Modify: `firmware/src/main.cpp`
- Modify: `docs/roteiro-de-teste.md`, `README.md`

**Interfaces:**
- Consumes: `SpotifyClient` (`state()`, `cover()`, `send()`, `watch()`), `PlayerState`, `CoverArt`, `Progress`,
  `DominantColor::FALLBACK`, `theme`, `fonts`, `Page`.
- Produces:
  - `class SpotifyPage : public Page { explicit SpotifyPage(SpotifyClient&); }`
  - `class NowPlayingPill : public Page { explicit NowPlayingPill(SpotifyClient&); }`
  - `HomePage(PhotoStore& store, std::unique_ptr<Page> overlay)` — `overlay` é construída numa camada que fica sempre
    por cima das fotos e recebe `onShow`/`onHide`/`onTick`.

- [ ] **Step 1: `HomePage` com sobreposição**

Em `firmware/src/photos/screen/HomePage.h`: inclua `<memory>`; troque o construtor por
`HomePage(PhotoStore& store, std::unique_ptr<Page> overlay);`; acrescente `void onHide() override;`; e nos membros
`std::unique_ptr<Page> overlay;` e `lv_obj_t* overlayLayer = nullptr;`.

Em `firmware/src/photos/screen/HomePage.cpp`:

```cpp
HomePage::HomePage(PhotoStore& store, std::unique_ptr<Page> overlay)
    : store(store)
    , view(store)
    , overlay(std::move(overlay))
{
}
```

No fim de `HomePage::build`:

```cpp
    // Camada por cima das fotos para outra página pôr o que quiser (a pílula da música). Não recebe toque: o deslize
    // continua passando para o Navigator.
    overlayLayer = lv_obj_create(parent);
    lv_obj_remove_style_all(overlayLayer);
    lv_obj_set_size(overlayLayer, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(overlayLayer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(overlayLayer, LV_OBJ_FLAG_GESTURE_BUBBLE);
    if (overlay) {
        overlay->build(overlayLayer);
    }
```

Em `HomePage::onShow`, depois de `reloadAlbum();`: `if (overlay) { overlay->onShow(); }`. Acrescente:

```cpp
void HomePage::onHide()
{
    if (overlay) {
        overlay->onHide();
    }
}
```

No fim de `HomePage::onTick`: `if (overlay) { overlay->onTick(nowMillis); }`. Em `HomePage::showNextPhoto`, logo
depois de `lv_obj_move_foreground(band);`: `lv_obj_move_foreground(overlayLayer);`.

- [ ] **Step 2: `NowPlayingPill`**

`firmware/src/spotify/screen/NowPlayingPill.h`:

```cpp
#pragma once

#include "navigation/Page.h"
#include "spotify/device/SpotifyClient.h"

// Cápsula "tocando agora" no canto do Início. Some quando pausa ou não há aparelho.
class NowPlayingPill : public Page {
public:
    explicit NowPlayingPill(SpotifyClient& client);

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;
    void onTick(uint32_t nowMillis) override;

private:
    SpotifyClient& client;
    uint32_t seenState = 0;
    lv_obj_t* pill = nullptr;
    lv_obj_t* text = nullptr;
};
```

`firmware/src/spotify/screen/NowPlayingPill.cpp`:

```cpp
#include "spotify/screen/NowPlayingPill.h"

#include <string>

#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t PILL_HEIGHT = 28;
constexpr int32_t MAX_TEXT_WIDTH = 220;
constexpr int32_t MARGIN = 12;

}

NowPlayingPill::NowPlayingPill(SpotifyClient& client)
    : client(client)
{
}

void NowPlayingPill::build(lv_obj_t* parent)
{
    pill = lv_obj_create(parent);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, PILL_HEIGHT);
    lv_obj_set_style_bg_color(pill, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_50, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, MARGIN, 0);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, -MARGIN, MARGIN);
    lv_obj_add_flag(pill, LV_OBJ_FLAG_HIDDEN);

    text = theme::label(pill, fonts::small, theme::text());
    lv_obj_set_width(text, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(text, MAX_TEXT_WIDTH, 0);
    lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
    lv_obj_align(text, LV_ALIGN_LEFT_MID, 0, 0);
}

void NowPlayingPill::onShow()
{
    client.watch(true);
}

void NowPlayingPill::onHide()
{
    client.watch(false);
}

void NowPlayingPill::onTick(uint32_t)
{
    PlayerState state;
    if (!client.state().readIfChanged(state, seenState)) {
        return;
    }

    bool showing = state.status == PlayerStatus::Playing && state.hasItem && !state.track.title.empty();
    lv_obj_set_flag(pill, LV_OBJ_FLAG_HIDDEN, !showing);
    if (!showing) {
        return;
    }

    std::string line = std::string(LV_SYMBOL_AUDIO) + "  " + state.track.title;
    if (!state.track.artists.empty()) {
        line += " — " + state.track.artists;
    }
    lv_label_set_text(text, line.c_str());
}
```

- [ ] **Step 3: `SpotifyPage`**

`firmware/src/spotify/screen/SpotifyPage.h`:

```cpp
#pragma once

#include <lvgl.h>

#include <string>

#include "navigation/Page.h"
#include "spotify/CoverArt.h"
#include "spotify/PlayerState.h"
#include "spotify/device/SpotifyClient.h"

class SpotifyPage : public Page {
public:
    explicit SpotifyPage(SpotifyClient& client);

    void build(lv_obj_t* parent) override;
    void onShow() override;
    void onHide() override;
    void onTick(uint32_t nowMillis) override;

private:
    void buildCover(lv_obj_t* parent);
    void buildDetails(lv_obj_t* parent);
    void buildControls(lv_obj_t* parent);
    void buildNotice(lv_obj_t* parent);
    void render(const PlayerState& state, uint32_t nowMillis);
    void renderCover(CoverArt art);
    void renderProgress(uint32_t nowMillis);
    void renderNotice(const PlayerState& state);
    void fadeBackgroundTo(uint32_t color);

    static void onPrevious(lv_event_t* event);
    static void onPlayPause(lv_event_t* event);
    static void onNext(lv_event_t* event);
    static void onVolume(lv_event_t* event);
    static void fadeStep(void* page, int32_t mix);

    SpotifyClient& client;
    uint32_t seenState = 0;
    uint32_t seenCover = 0;
    PlayerState shown;
    CoverArt shownCover;
    lv_image_dsc_t coverDescriptor = {};
    uint32_t backgroundFrom = DominantColor::FALLBACK;
    uint32_t backgroundTo = DominantColor::FALLBACK;
    uint32_t currentBackground = DominantColor::FALLBACK;
    uint32_t lastProgressMillis = 0;
    uint32_t staleSinceMillis = 0;
    std::string shownLoginUrl;

    lv_obj_t* root = nullptr;
    lv_obj_t* cover = nullptr;
    lv_obj_t* coverPlaceholder = nullptr;
    lv_obj_t* title = nullptr;
    lv_obj_t* artists = nullptr;
    lv_obj_t* device = nullptr;
    lv_obj_t* progressBar = nullptr;
    lv_obj_t* elapsed = nullptr;
    lv_obj_t* total = nullptr;
    lv_obj_t* playPauseLabel = nullptr;
    lv_obj_t* volumeIcon = nullptr;
    lv_obj_t* volumeSlider = nullptr;
    lv_obj_t* notice = nullptr;
    lv_obj_t* noticeText = nullptr;
    lv_obj_t* qr = nullptr;
    lv_obj_t* staleLabel = nullptr;
};
```

`firmware/src/spotify/screen/SpotifyPage.cpp`:

```cpp
#include "spotify/screen/SpotifyPage.h"

#include "spotify/Progress.h"
#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t COVER_X = 20;
constexpr int32_t COVER_Y = 40;
constexpr int32_t COVER_RADIUS = 12;
constexpr int32_t DETAILS_X = 240;
constexpr int32_t DETAILS_WIDTH = 220;
constexpr int32_t CONTROL_SIZE = 56;
constexpr int32_t CONTROLS_Y = 182;
constexpr int32_t QR_SIZE = 140;
constexpr int32_t PROGRESS_SCALE = 1000;
constexpr uint32_t PROGRESS_EVERY_MILLIS = 250;
constexpr uint32_t FADE_MILLIS = 400;
constexpr uint32_t STALE_NOTICE_AFTER_MILLIS = 60000;

lv_obj_t* makeControl(lv_obj_t* parent, const char* symbol, int32_t x, lv_event_cb_t callback, void* page)
{
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, CONTROL_SIZE, CONTROL_SIZE);
    lv_obj_set_pos(button, x, CONTROLS_Y);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, theme::text(), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, page);

    lv_obj_t* label = theme::label(button, fonts::title, theme::text());
    lv_label_set_text(label, symbol);
    lv_obj_center(label);
    return label;
}

SpotifyPage* pageOf(lv_event_t* event)
{
    return static_cast<SpotifyPage*>(lv_event_get_user_data(event));
}

}

SpotifyPage::SpotifyPage(SpotifyClient& client)
    : client(client)
{
}

void SpotifyPage::build(lv_obj_t* parent)
{
    root = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(DominantColor::FALLBACK), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    buildCover(parent);
    buildDetails(parent);
    buildControls(parent);
    buildNotice(parent);

    staleLabel = theme::label(parent, fonts::small, theme::textSecondary());
    lv_label_set_text(staleLabel, "sem conexão");
    lv_obj_align(staleLabel, LV_ALIGN_TOP_RIGHT, -12, 10);
    lv_obj_add_flag(staleLabel, LV_OBJ_FLAG_HIDDEN);
}

void SpotifyPage::buildCover(lv_obj_t* parent)
{
    coverPlaceholder = lv_obj_create(parent);
    lv_obj_remove_style_all(coverPlaceholder);
    lv_obj_set_size(coverPlaceholder, COVER_SIZE, COVER_SIZE);
    lv_obj_set_pos(coverPlaceholder, COVER_X, COVER_Y);
    lv_obj_set_style_radius(coverPlaceholder, COVER_RADIUS, 0);
    lv_obj_set_style_bg_color(coverPlaceholder, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(coverPlaceholder, LV_OPA_30, 0);
    lv_obj_add_flag(coverPlaceholder, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t* note = theme::label(coverPlaceholder, fonts::title, theme::textSecondary());
    lv_label_set_text(note, LV_SYMBOL_AUDIO);
    lv_obj_center(note);

    cover = lv_image_create(parent);
    lv_obj_set_size(cover, COVER_SIZE, COVER_SIZE);
    lv_obj_set_pos(cover, COVER_X, COVER_Y);
    lv_obj_set_style_radius(cover, COVER_RADIUS, 0);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(cover, LV_OBJ_FLAG_HIDDEN);
}

void SpotifyPage::buildDetails(lv_obj_t* parent)
{
    title = theme::label(parent, fonts::large, theme::text());
    lv_obj_set_size(title, DETAILS_WIDTH, 48);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(title, DETAILS_X, COVER_Y);

    artists = theme::label(parent, fonts::medium, theme::text());
    lv_obj_set_size(artists, DETAILS_WIDTH, 20);
    lv_label_set_long_mode(artists, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(artists, DETAILS_X, COVER_Y + 52);

    device = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_set_size(device, DETAILS_WIDTH, 16);
    lv_label_set_long_mode(device, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(device, DETAILS_X, COVER_Y + 76);

    progressBar = lv_bar_create(parent);
    lv_obj_set_size(progressBar, DETAILS_WIDTH, 4);
    lv_obj_set_pos(progressBar, DETAILS_X, COVER_Y + 106);
    lv_bar_set_range(progressBar, 0, PROGRESS_SCALE);
    lv_obj_set_style_bg_color(progressBar, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(progressBar, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar, theme::text(), LV_PART_INDICATOR);

    elapsed = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_align_to(elapsed, progressBar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    total = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_align_to(total, progressBar, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 6);
}

void SpotifyPage::buildControls(lv_obj_t* parent)
{
    makeControl(parent, LV_SYMBOL_PREV, DETAILS_X + 4, onPrevious, this);
    playPauseLabel = makeControl(parent, LV_SYMBOL_PLAY, DETAILS_X + 82, onPlayPause, this);
    makeControl(parent, LV_SYMBOL_NEXT, DETAILS_X + 160, onNext, this);

    volumeIcon = theme::label(parent, fonts::medium, theme::textSecondary());
    lv_label_set_text(volumeIcon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_pos(volumeIcon, DETAILS_X, CONTROLS_Y + 76);

    volumeSlider = lv_slider_create(parent);
    lv_obj_set_size(volumeSlider, DETAILS_WIDTH - 36, 6);
    lv_obj_set_pos(volumeSlider, DETAILS_X + 30, CONTROLS_Y + 82);
    lv_slider_set_range(volumeSlider, 0, 100);
    lv_obj_set_style_bg_color(volumeSlider, theme::text(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volumeSlider, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(volumeSlider, theme::text(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volumeSlider, theme::text(), LV_PART_KNOB);
    lv_obj_add_event_cb(volumeSlider, onVolume, LV_EVENT_VALUE_CHANGED, this);
}

void SpotifyPage::buildNotice(lv_obj_t* parent)
{
    notice = lv_obj_create(parent);
    lv_obj_remove_style_all(notice);
    lv_obj_set_size(notice, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(notice, lv_color_hex(DominantColor::FALLBACK), 0);
    lv_obj_set_style_bg_opa(notice, LV_OPA_COVER, 0);
    lv_obj_add_flag(notice, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(notice, LV_OBJ_FLAG_SCROLLABLE);

    qr = lv_qrcode_create(notice);
    lv_qrcode_set_size(qr, QR_SIZE);
    lv_qrcode_set_dark_color(qr, theme::background());
    lv_qrcode_set_light_color(qr, theme::text());
    lv_obj_set_style_border_width(qr, 6, 0);
    lv_obj_set_style_border_color(qr, theme::text(), 0);
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 40, -10);

    noticeText = theme::label(notice, fonts::large, theme::text());
    lv_obj_set_width(noticeText, DETAILS_WIDTH);
    lv_label_set_long_mode(noticeText, LV_LABEL_LONG_WRAP);

    lv_obj_add_flag(notice, LV_OBJ_FLAG_HIDDEN);
}

void SpotifyPage::onShow()
{
    client.watch(true);
}

void SpotifyPage::onHide()
{
    client.watch(false);
}

void SpotifyPage::onTick(uint32_t nowMillis)
{
    PlayerState state;
    if (client.state().readIfChanged(state, seenState)) {
        render(state, nowMillis);
    }

    CoverArt art;
    if (client.cover().readIfChanged(art, seenCover)) {
        renderCover(std::move(art));
    }

    if (nowMillis - lastProgressMillis >= PROGRESS_EVERY_MILLIS) {
        lastProgressMillis = nowMillis;
        renderProgress(nowMillis);
    }
}

void SpotifyPage::render(const PlayerState& state, uint32_t nowMillis)
{
    if (state.stale && !shown.stale) {
        staleSinceMillis = nowMillis;
    }
    shown = state;
    renderNotice(state);

    bool somethingPlays = state.status == PlayerStatus::Playing || state.status == PlayerStatus::Paused;
    std::string titleText = state.hasItem ? state.track.title : (somethingPlays ? "Tocando algo" : "");
    lv_label_set_text(title, titleText.c_str());
    lv_label_set_text(artists, state.hasItem ? state.track.artists.c_str() : "");
    lv_label_set_text(device, state.device.c_str());
    lv_label_set_text(total, Progress::clock(state.track.durationMs).c_str());
    lv_label_set_text(playPauseLabel, state.status == PlayerStatus::Playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    bool volumeShown = state.volume.has_value() && state.volumeAllowed;
    lv_obj_set_flag(volumeSlider, LV_OBJ_FLAG_HIDDEN, !volumeShown);
    lv_obj_set_flag(volumeIcon, LV_OBJ_FLAG_HIDDEN, !volumeShown);
    if (volumeShown && !lv_obj_has_state(volumeSlider, LV_STATE_PRESSED)) {
        lv_slider_set_value(volumeSlider, *state.volume, LV_ANIM_OFF);
    }

    renderProgress(nowMillis);
}

void SpotifyPage::renderCover(CoverArt art)
{
    shownCover = std::move(art);
    bool hasPixels = shownCover.pixels.size() == static_cast<size_t>(COVER_SIZE * COVER_SIZE);
    lv_obj_set_flag(cover, LV_OBJ_FLAG_HIDDEN, !hasPixels);
    lv_obj_set_flag(coverPlaceholder, LV_OBJ_FLAG_HIDDEN, hasPixels);

    if (hasPixels) {
        coverDescriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        coverDescriptor.header.cf = LV_COLOR_FORMAT_RGB565;
        coverDescriptor.header.w = COVER_SIZE;
        coverDescriptor.header.h = COVER_SIZE;
        coverDescriptor.header.stride = COVER_SIZE * 2;
        coverDescriptor.data_size = COVER_SIZE * COVER_SIZE * 2;
        coverDescriptor.data = reinterpret_cast<const uint8_t*>(shownCover.pixels.data());
        // O descritor é o mesmo a cada capa; sem tirar do cache o LVGL desenharia a capa anterior.
        lv_image_cache_drop(&coverDescriptor);
        lv_image_set_src(cover, &coverDescriptor);
        lv_obj_invalidate(cover);
    }

    fadeBackgroundTo(hasPixels ? shownCover.color : DominantColor::FALLBACK);
}

void SpotifyPage::renderProgress(uint32_t nowMillis)
{
    uint32_t position = Progress::estimate(shown, nowMillis);
    uint32_t duration = shown.track.durationMs;
    int32_t scaled = duration > 0 ? static_cast<int32_t>(uint64_t(position) * PROGRESS_SCALE / duration) : 0;
    lv_bar_set_value(progressBar, scaled, LV_ANIM_OFF);
    lv_label_set_text(elapsed, Progress::clock(position).c_str());

    bool staleLong = shown.stale && nowMillis - staleSinceMillis > STALE_NOTICE_AFTER_MILLIS;
    lv_obj_set_flag(staleLabel, LV_OBJ_FLAG_HIDDEN, !staleLong);
}

void SpotifyPage::renderNotice(const PlayerState& state)
{
    bool disconnected = state.status == PlayerStatus::Disconnected;
    bool noDevice = state.status == PlayerStatus::NoDevice;
    lv_obj_set_flag(notice, LV_OBJ_FLAG_HIDDEN, !disconnected && !noDevice);
    lv_obj_set_flag(qr, LV_OBJ_FLAG_HIDDEN, !disconnected);
    lv_obj_move_foreground(notice);

    if (disconnected) {
        lv_label_set_text(noticeText, "Escaneie para conectar o Spotify");
        lv_obj_align(noticeText, LV_ALIGN_RIGHT_MID, -20, -10);
        if (!state.loginUrl.empty() && state.loginUrl != shownLoginUrl) {
            shownLoginUrl = state.loginUrl;
            lv_qrcode_update(qr, shownLoginUrl.c_str(), shownLoginUrl.size());
        }
        return;
    }

    lv_label_set_text(noticeText, LV_SYMBOL_AUDIO "  Abra o Spotify em algum aparelho");
    lv_obj_align(noticeText, LV_ALIGN_CENTER, 0, -10);
}

void SpotifyPage::fadeBackgroundTo(uint32_t color)
{
    backgroundFrom = currentBackground;
    backgroundTo = color;

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, this);
    lv_anim_set_values(&fade, 0, 255);
    lv_anim_set_duration(&fade, FADE_MILLIS);
    lv_anim_set_exec_cb(&fade, fadeStep);
    lv_anim_start(&fade);
}

void SpotifyPage::fadeStep(void* page, int32_t mix)
{
    SpotifyPage* self = static_cast<SpotifyPage*>(page);
    lv_color_t color = lv_color_mix(lv_color_hex(self->backgroundTo), lv_color_hex(self->backgroundFrom),
        static_cast<uint8_t>(mix));

    lv_obj_set_style_bg_color(self->root, color, 0);
    self->currentBackground = lv_color_to_u32(color) & 0xFFFFFF;
}

void SpotifyPage::onPrevious(lv_event_t* event)
{
    pageOf(event)->client.send(Command{CommandKind::Previous});
}

void SpotifyPage::onPlayPause(lv_event_t* event)
{
    SpotifyPage* self = pageOf(event);
    bool playing = self->shown.status == PlayerStatus::Playing;
    self->client.send(Command{playing ? CommandKind::Pause : CommandKind::Play});
}

void SpotifyPage::onNext(lv_event_t* event)
{
    pageOf(event)->client.send(Command{CommandKind::Next});
}

void SpotifyPage::onVolume(lv_event_t* event)
{
    lv_obj_t* slider = lv_event_get_target_obj(event);
    uint8_t volume = static_cast<uint8_t>(lv_slider_get_value(slider));
    pageOf(event)->client.send(Command{CommandKind::Volume, volume});
}
```

(Se `lv_image_cache_drop`, `lv_color_to_u32` ou `lv_obj_set_style_max_width` acusarem nome, confira em
`.pio/libdeps/board/lvgl/src/` — os nomes acima são os do LVGL 9.4. Os ícones `LV_SYMBOL_PREV/PLAY/PAUSE/NEXT/
AUDIO/VOLUME_MAX` estão nas fontes Inter geradas no plano 1.)

- [ ] **Step 4: Montar no `main.cpp`**

Inclua `"spotify/device/SpotifyAuth.h"`, `"spotify/device/SpotifyClient.h"`, `"spotify/screen/NowPlayingPill.h"`,
`"spotify/screen/SpotifyPage.h"`, `"spotify/storage/SpotifyVault.h"` e `"spotify/web/SpotifyRoutes.h"`. Globais,
depois de `MarketFeed marketFeed;`:

```cpp
SpotifyVault spotifyVault;
SpotifyAuth spotifyAuth(spotifyVault);
SpotifyClient spotifyClient(spotifyAuth);
SpotifyRoutes spotifyRoutes(http, spotifyAuth, spotifyVault, wifi);
```

Na `networkTask`, junto das outras rotas e antes de `http.begin();`: `spotifyRoutes.begin();`.

Em `setup()`, depois de `adminSecret.begin();`: `spotifyVault.begin();`. No bloco `if (configured)`:

```cpp
        navigator.add(std::make_unique<HomePage>(photoStore, std::make_unique<NowPlayingPill>(spotifyClient)));
        navigator.add(std::make_unique<CryptoPage>(marketFeed.state()));
        navigator.add(std::make_unique<SpotifyPage>(spotifyClient));
        marketFeed.begin();
        spotifyClient.begin();
```

(substituindo a linha antiga da `HomePage`, que passava só `photoStore`).

- [ ] **Step 5: Roteiro e README**

Em `docs/roteiro-de-teste.md`:

```markdown
## Spotify

Antes, uma vez: redirect URI `https://leonardolermen.github.io/esp32-display/callback/` no app do Spotify; Pages com
fonte "GitHub Actions" (o workflow `relay` publica). Sem o Pages ainda, o plano B funciona: o login para numa página
404 do github.io, e o endereço dela colado no card resolve.

- [ ] Card Spotify em claudinho.local → Conectar → login no celular → volta para a placa → "Spotify conectando".
- [ ] Página do Spotify desconectada mostra QR; escanear com o celular também conecta.
- [ ] Plano B: colar o link onde o login parou no campo "O login não voltou para cá?" conecta.
- [ ] Música aparece em até 3 s com capa, título, artista, aparelho e progresso andando.
- [ ] Tocar/pausar, próxima e anterior respondem na hora; volume pelo slider muda no aparelho.
- [ ] Trocar de álbum troca a capa e a cor de fundo com transição.
- [ ] Sem aparelho tocando: "Abra o Spotify em algum aparelho".
- [ ] Pílula no Início com a música; some ao pausar.
- [ ] Reiniciar a placa mantém a conexão; Desconectar na página volta para o QR.
```

Em `README.md`, na tabela de pastas, depois de `crypto/`:

```markdown
| `firmware/src/spotify/` | Spotify: login PKCE, player, capa e controles |
| `relay/` | página estática do GitHub Pages que devolve o login do Spotify para a placa |
```

e, em "Primeira vez", um passo 4:

```markdown
4. Para a música: no card **Spotify** de `claudinho.local`, toque em **Conectar** e faça login no celular.
```

- [ ] **Step 6: Compilar, gravar e conferir**

Run: `cd firmware && pio test -e native`, `node --test "test/web/*.test.mjs"`, da raiz
`node --test "relay/test/*.test.mjs"`, e no PowerShell `cd firmware; pio run -e board -t upload --upload-port COM8`
Expected: tudo verde, compila, grava. Passe a seção "Spotify" do roteiro com o humano: ele precisa fazer os passos
manuais (redirect URI e Pages) — peça na hora, não antes.

- [ ] **Step 7: Commit**

```bash
git add firmware/src docs/roteiro-de-teste.md README.md
git commit -m "feat(spotify): página do Spotify e pílula da música no Início

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

## Self-review

- **Cobertura da spec:** §3 app e passos manuais (T3 workflow, T11 roteiro); §4 login com relay, state, nonce, plano B
  e QR (T1, T2, T3, T8, T10, T11); §5 tokens só refresh na NVS, renovação 60 s antes, `invalid_grant` (T4, T8); §6
  intervalos, `Retry-After`, comandos otimistas, debounce, `403`/`404` (T5, T9); §7 capa e cor (T6, T9, T11); §8 tela,
  estados especiais e pílula (T11); §9 arquivos (todas); §10 erros (T8 troca falha, T9 rede/capa, T10 páginas de
  callback); §11 testes (T1–T6 no PC, T3 no Node, roteiro em T11). §12 fora de escopo: nada aqui.
- **Placeholders:** nenhum.
- **Tipos:** `AuthFlow::CallbackCheck` (T2) em T8 e T10; `PlayerState`/`PlayerStatus` (T4) em T5, T9, T11;
  `TokenResult` (T4) em T8; `CoverArt`/`COVER_SIZE`/`DominantColor::FALLBACK` (T6) em T9 e T11; `HttpsRequest`/
  `HttpsResponse`/`PsramBuffer` (T7) em T8 e T9; `SpotifyAuth` (T8) em T9 e T10; `SpotifyClient`/`Command`/
  `CommandKind` (T9) em T11; `HomePage(PhotoStore&, std::unique_ptr<Page>)` (T11) só no `main.cpp`.
- **Review Focus → testes:** 1 → `test_old_callback_is_expired`, `test_callback_from_another_login_is_a_mismatch`
  (T2) e os testes do relay (T3); 2 → `test_refresh_without_a_new_refresh_token_keeps_it_absent` (T4); 3 →
  `test_advertisement_has_no_item`, `test_podcast_episode_uses_the_show_and_its_images` (T4); 4 →
  `test_never_passes_the_duration`, `test_survives_the_millis_wraparound` (T5); 5 → `test_large_640_cover_fits`,
  `test_small_64_cover_is_enlarged` (T6).
- **Pontos frágeis declarados:** nomes de API do LVGL 9.4 (`lv_image_cache_drop`, `lv_color_to_u32`, max width em
  label `LONG_DOT`) e do JPEGDEC (`JPEG_SCALE_HALF`) — cada passo diz onde conferir. O salto HTTPS → HTTP local pode
  ser barrado pelo navegador; o plano B (colar o link) está na T10 e no roteiro.
