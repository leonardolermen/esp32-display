#pragma once

#include <cstdint>
#include <optional>
#include <string>

// O login do Spotify sem client secret: a placa gera o PKCE e o state, o Spotify devolve para o relay no GitHub
// Pages, e o relay manda o navegador de volta para a placa com code e state.
namespace AuthFlow {

constexpr const char* DEFAULT_CLIENT_ID = "02d1a22249834743b58e6fc356e33b2e";
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

struct Match {
    CallbackCheck check;
    const Pending* accepted;
};

std::string urlEncode(const std::string& text);
std::string state(const std::string& ip, uint16_t port, const std::string& nonce);
std::optional<HostAndNonce> parseState(const std::string& state);
std::string authorizeUrl(const std::string& clientId, const std::string& challenge, const std::string& state);
CallbackCheck check(const Pending& pending, const std::string& state, uint32_t nowMillis);
Match checkEither(const Pending& current, const Pending& previous, const std::string& state, uint32_t nowMillis);
std::string shortLoginUrl(const std::string& ip);
std::optional<Callback> fromPastedUrl(const std::string& url);
std::string tokenRequestBody(const std::string& clientId, const std::string& code, const std::string& verifier);
std::string refreshRequestBody(const std::string& clientId, const std::string& refreshToken);

}
