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
