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
    std::string lastError() const;
    AuthFlow::CallbackCheck acceptCallback(const std::string& code, const std::string& state, uint32_t nowMillis);
    bool exchangeWaitingCode(uint32_t nowMillis);
    std::optional<std::string> accessToken(uint32_t nowMillis);
    void invalidateAccessToken();
    void disconnect();

private:
    std::optional<TokenResult> requestToken(const std::string& body);
    void remember(const TokenResult& result, uint32_t nowMillis, uint32_t generationAtRequest);
    uint32_t currentGeneration() const;

    SpotifyVault& vault;
    mutable std::mutex mutex;
    AuthFlow::Pending pending;
    AuthFlow::Pending previousPending;
    std::string lastFailure;
    std::string waitingCode;
    std::string waitingVerifier;
    std::string access;
    uint32_t accessExpiresMillis = 0;
    // Muda a cada Desconectar: uma resposta de token que chega depois não reconecta a placa sozinha.
    uint32_t generation = 0;
};
