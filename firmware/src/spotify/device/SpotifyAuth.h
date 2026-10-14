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
