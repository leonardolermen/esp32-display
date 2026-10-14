#include "spotify/Progress.h"

#include <algorithm>
#include <cstdio>

namespace Progress {

// Entre um poll e outro a barra anda sozinha; a subtração sem sinal atravessa a virada do millis() sem saltar.
uint32_t estimate(const PlayerState& state, uint32_t nowMillis)
{
    uint32_t elapsed = state.status == PlayerStatus::Playing ? nowMillis - state.sampledAtMillis : 0;
    uint32_t position = state.progressMs + elapsed;

    return state.track.durationMs > 0 ? std::min(position, state.track.durationMs) : position;
}

std::string clock(uint32_t millis)
{
    uint32_t totalSeconds = millis / 1000;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u:%02u", static_cast<unsigned>(totalSeconds / 60),
        static_cast<unsigned>(totalSeconds % 60));
    return buffer;
}

}
