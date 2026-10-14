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
