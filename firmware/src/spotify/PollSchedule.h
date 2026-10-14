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
