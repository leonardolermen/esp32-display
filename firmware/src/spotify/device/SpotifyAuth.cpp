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
