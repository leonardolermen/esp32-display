#include "spotify/PlayerApi.h"

namespace {

constexpr const char* PLAYER_URL = "https://api.spotify.com/v1/me/player";
// Sem additional_types o Spotify devolve item nulo para podcast, e a tela mostraria só "Tocando algo".
constexpr const char* POLL_QUERY = "?additional_types=track,episode";

struct Endpoint {
    const char* method;
    const char* path;
};

// Na ordem de CommandKind.
constexpr Endpoint ENDPOINTS[] = {
    {"PUT", "/play"},
    {"PUT", "/pause"},
    {"POST", "/next"},
    {"POST", "/previous"},
    {"PUT", "/volume?volume_percent="},
};

const Endpoint& endpointOf(CommandKind kind)
{
    return ENDPOINTS[static_cast<size_t>(kind)];
}

}

namespace PlayerApi {

std::string pollUrl()
{
    return std::string(PLAYER_URL) + POLL_QUERY;
}

const char* method(CommandKind kind)
{
    return endpointOf(kind).method;
}

std::string commandUrl(CommandKind kind, uint8_t volume)
{
    std::string url = std::string(PLAYER_URL) + endpointOf(kind).path;
    if (kind == CommandKind::Volume) {
        url += std::to_string(volume);
    }
    return url;
}

}
