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
