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
