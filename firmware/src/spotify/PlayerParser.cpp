#include "spotify/PlayerParser.h"

#include <ArduinoJson.h>

#include <climits>
#include <cstdlib>

namespace {

constexpr int PREFERRED_COVER_WIDTH = 300;

// A resposta do player traz o mercado, as URLs externas e a lista de disponibilidade; o filtro guarda só o que a
// tela usa.
JsonDocument playerFilter()
{
    JsonDocument filter;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["device"]["name"] = true;
    filter["device"]["volume_percent"] = true;
    filter["item"]["id"] = true;
    filter["item"]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["item"]["artists"][0]["name"] = true;
    filter["item"]["album"]["id"] = true;
    filter["item"]["album"]["name"] = true;
    filter["item"]["album"]["images"][0]["url"] = true;
    filter["item"]["album"]["images"][0]["width"] = true;
    filter["item"]["show"]["name"] = true;
    filter["item"]["images"][0]["url"] = true;
    filter["item"]["images"][0]["width"] = true;
    return filter;
}

std::string joinArtists(JsonArrayConst artists)
{
    std::string joined;
    for (JsonObjectConst artist : artists) {
        if (!joined.empty()) {
            joined += ", ";
        }
        joined += artist["name"] | "";
    }
    return joined;
}

std::string closestCover(JsonArrayConst images)
{
    std::string best;
    int bestDistance = INT_MAX;
    for (JsonObjectConst image : images) {
        int distance = std::abs((image["width"] | 0) - PREFERRED_COVER_WIDTH);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = image["url"] | "";
        }
    }
    return best;
}

void fillTrack(Track& track, JsonObjectConst item)
{
    track.title = item["name"] | "";
    track.durationMs = item["duration_ms"] | 0u;

    JsonObjectConst album = item["album"].as<JsonObjectConst>();
    // Episódio de podcast não tem álbum: o "artista" é o programa e a capa vem do próprio episódio.
    if (album.isNull()) {
        track.artists = item["show"]["name"] | "";
        track.albumId = item["id"] | "";
        track.coverUrl = closestCover(item["images"].as<JsonArrayConst>());
        return;
    }

    track.artists = joinArtists(item["artists"].as<JsonArrayConst>());
    track.album = album["name"] | "";
    track.albumId = album["id"] | "";
    track.coverUrl = closestCover(album["images"].as<JsonArrayConst>());
}

}

namespace PlayerParser {

std::optional<PlayerState> parse(std::string_view json, uint32_t nowMillis)
{
    JsonDocument filter = playerFilter();
    JsonDocument document;
    DeserializationError error = deserializeJson(document, json.data(), json.size(),
        DeserializationOption::Filter(filter));
    if (error != DeserializationError::Ok) {
        return std::nullopt;
    }

    PlayerState state;
    state.status = (document["is_playing"] | false) ? PlayerStatus::Playing : PlayerStatus::Paused;
    state.progressMs = document["progress_ms"] | 0u;
    state.sampledAtMillis = nowMillis;
    state.device = document["device"]["name"] | "";

    JsonVariantConst volume = document["device"]["volume_percent"];
    if (volume.is<int>()) {
        state.volume = volume.as<int>();
    }

    JsonObjectConst item = document["item"].as<JsonObjectConst>();
    state.hasItem = !item.isNull();
    if (state.hasItem) {
        fillTrack(state.track, item);
    }
    return state;
}

}
