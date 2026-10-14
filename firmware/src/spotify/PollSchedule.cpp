#include "spotify/PollSchedule.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

constexpr uint32_t MIN_RETRY_MILLIS = 1000;
constexpr uint32_t MILLIS_PER_SECOND = 1000;
constexpr uint32_t MAX_UNAUTHORIZED_BACKOFF_MILLIS = 60000;
constexpr uint32_t MAX_BACKOFF_SHIFT = 6;

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

// 401 seguido mesmo com token renovado (app revogado, escopo errado): dobra a espera até 1 min para não martelar o
// endpoint de token.
uint32_t unauthorizedBackoffMillis(uint32_t consecutive)
{
    uint32_t shift = std::min(consecutive > 0 ? consecutive - 1 : 0, MAX_BACKOFF_SHIFT);
    return std::min(MIN_RETRY_MILLIS << shift, MAX_UNAUTHORIZED_BACKOFF_MILLIS);
}

}
