#include "spotify/device/SpotifyClient.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>

#include "spotify/AuthFlow.h"
#include "spotify/PlayerApi.h"
#include "spotify/PlayerParser.h"
#include "spotify/PollSchedule.h"
#include "spotify/Progress.h"

namespace {

constexpr uint32_t TASK_STACK_BYTES = 16384;
constexpr UBaseType_t QUEUE_LENGTH = 8;
constexpr uint32_t IDLE_WAIT_MILLIS = 1000;
constexpr uint32_t RECONCILE_AFTER_MILLIS = 600;
constexpr uint32_t VOLUME_DEBOUNCE_MILLIS = 300;
constexpr uint32_t RETRY_WITHOUT_TOKEN_MILLIS = 5000;
constexpr int HTTP_OK = 200;
constexpr int NO_CONTENT = 204;
constexpr int UNAUTHORIZED = 401;
constexpr int FORBIDDEN = 403;
constexpr int NOT_FOUND = 404;
constexpr int TOO_MANY_REQUESTS = 429;

bool reached(uint32_t nowMillis, uint32_t deadline)
{
    return static_cast<int32_t>(nowMillis - deadline) >= 0;
}

uint32_t remaining(uint32_t nowMillis, uint32_t deadline)
{
    return reached(nowMillis, deadline) ? 0 : deadline - nowMillis;
}

}

SpotifyClient::SpotifyClient(SpotifyAuth& auth)
    : auth(auth)
{
}

void SpotifyClient::begin()
{
    commands = xQueueCreate(QUEUE_LENGTH, sizeof(Command));
    xTaskCreatePinnedToCore(run, "spotify", TASK_STACK_BYTES, this, 1, nullptr, 0);
}

const Published<PlayerState>& SpotifyClient::state() const
{
    return published;
}

const Published<std::shared_ptr<const CoverArt>>& SpotifyClient::cover() const
{
    return publishedCover;
}

void SpotifyClient::send(Command command)
{
    if (commands != nullptr) {
        xQueueSend(commands, &command, 0);
    }
}

void SpotifyClient::requestVolume(uint8_t volume)
{
    requestedVolume = volume;
    // Acorda a tarefa; se a fila estiver cheia, o loop pega o valor no próximo giro.
    send(Command{CommandKind::Volume});
}

void SpotifyClient::watch(bool watching)
{
    watchers += watching ? 1 : -1;
}

void SpotifyClient::run(void* self)
{
    static_cast<SpotifyClient*>(self)->loop();
}

void SpotifyClient::loop()
{
    for (;;) {
        uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            markStale();
            vTaskDelay(pdMS_TO_TICKS(IDLE_WAIT_MILLIS));
            continue;
        }

        if (auth.exchangeWaitingCode(now)) {
            nextPollMillis = now;
        }
        if (!auth.connected()) {
            showLogin(now);
            waitForCommand(IDLE_WAIT_MILLIS);
            continue;
        }

        if (reached(now, nextPollMillis)) {
            poll(now);
        }
        if (requestedVolume.load() >= 0) {
            handle(Command{CommandKind::Volume}, millis());
        }
        if (pendingVolume && reached(millis(), volumeDueMillis)) {
            flushVolume();
        }

        waitForCommand(nextWaitMillis(millis()));
    }
}

void SpotifyClient::showLogin(uint32_t)
{
    if (current.status != PlayerStatus::Disconnected) {
        current = PlayerState{};
        coverAlbumId.clear();
        publishedCover.publish(std::make_shared<const CoverArt>());
    }

    std::string url = AuthFlow::shortLoginUrl(WiFi.localIP().toString().c_str());
    if (url != current.loginUrl) {
        current.loginUrl = url;
        published.publish(current);
    }
}

void SpotifyClient::markStale()
{
    if (current.stale || current.status == PlayerStatus::Disconnected) {
        return;
    }

    current.stale = true;
    published.publish(current);
}

void SpotifyClient::poll(uint32_t nowMillis)
{
    std::optional<std::string> token = auth.accessToken(nowMillis);
    if (!token) {
        nextPollMillis = nowMillis + RETRY_WITHOUT_TOKEN_MILLIS;
        return;
    }

    HttpsRequest request;
    request.url = PlayerApi::pollUrl();
    request.bearerToken = *token;
    HttpsResponse response = Https::send(request);
    uint32_t answeredAt = millis();

    uint32_t wait = 0;
    if (response.status != UNAUTHORIZED) {
        consecutiveUnauthorized = 0;
    }
    switch (response.status) {
    case HTTP_OK:
        wait = applyPlayer(response, answeredAt);
        break;
    case NO_CONTENT:
        wait = applyNoDevice(answeredAt);
        break;
    case UNAUTHORIZED:
        auth.invalidateAccessToken();
        consecutiveUnauthorized++;
        wait = PollSchedule::unauthorizedBackoffMillis(consecutiveUnauthorized);
        break;
    case TOO_MANY_REQUESTS:
        wait = PollSchedule::retryAfterMillis(response.retryAfter);
        break;
    default:
        wait = applyFailure(answeredAt);
        break;
    }
    nextPollMillis = answeredAt + wait;
}

uint32_t SpotifyClient::applyPlayer(const HttpsResponse& response, uint32_t nowMillis)
{
    std::string_view json = response.body ? response.body->view() : std::string_view();
    std::optional<PlayerState> parsed = PlayerParser::parse(json, nowMillis);
    if (!parsed) {
        return applyFailure(nowMillis);
    }

    parsed->volumeAllowed = parsed->device == current.device ? current.volumeAllowed : true;
    // Enquanto o dedo mexe no volume, o slider fica onde ele deixou.
    if (pendingVolume) {
        parsed->volume = *pendingVolume;
    }
    if (parsed->status == PlayerStatus::Paused && current.status != PlayerStatus::Paused) {
        pausedSinceMillis = nowMillis;
    }

    current = *parsed;
    published.publish(current);
    refreshCover();
    return interval(nowMillis);
}

uint32_t SpotifyClient::applyNoDevice(uint32_t nowMillis)
{
    current.status = PlayerStatus::NoDevice;
    current.hasItem = false;
    current.stale = false;
    published.publish(current);
    return interval(nowMillis);
}

uint32_t SpotifyClient::applyFailure(uint32_t nowMillis)
{
    current.stale = true;
    published.publish(current);
    return interval(nowMillis);
}

uint32_t SpotifyClient::interval(uint32_t nowMillis) const
{
    uint32_t pausedFor = current.status == PlayerStatus::Paused ? nowMillis - pausedSinceMillis : 0;
    return PollSchedule::intervalMillis(current.status, watchers.load() > 0, pausedFor).value_or(IDLE_WAIT_MILLIS);
}

void SpotifyClient::refreshCover()
{
    if (!current.hasItem || current.track.albumId == coverAlbumId) {
        return;
    }

    // Marca antes de baixar: uma capa que falha não é tentada de novo a cada poll do mesmo álbum.
    coverAlbumId = current.track.albumId;
    std::optional<CoverArt> art;
    if (!current.track.coverUrl.empty()) {
        art = covers.fetch(current.track.coverUrl);
    }

    CoverArt shown = art ? std::move(*art) : CoverArt{};
    shown.albumId = coverAlbumId;
    publishedCover.publish(std::make_shared<const CoverArt>(std::move(shown)));
}

void SpotifyClient::handle(const Command& command, uint32_t nowMillis)
{
    switch (command.kind) {
    case CommandKind::Volume: {
        int requested = requestedVolume.exchange(-1);
        if (requested < 0) {
            return;
        }
        pendingVolume = static_cast<uint8_t>(requested);
        volumeDueMillis = nowMillis + VOLUME_DEBOUNCE_MILLIS;
        current.volume = requested;
        published.publish(current);
        return;
    }
    case CommandKind::Play:
        current.status = PlayerStatus::Playing;
        current.sampledAtMillis = nowMillis;
        break;
    case CommandKind::Pause:
        current.progressMs = Progress::estimate(current, nowMillis);
        current.sampledAtMillis = nowMillis;
        current.status = PlayerStatus::Paused;
        break;
    case CommandKind::Next:
    case CommandKind::Previous:
        current.progressMs = 0;
        current.sampledAtMillis = nowMillis;
        break;
    }

    // Otimista: a tela muda na hora e o poll logo depois confirma ou desfaz.
    published.publish(current);
    callPlayer(command.kind, 0);
    nextPollMillis = millis() + RECONCILE_AFTER_MILLIS;
}

int SpotifyClient::callPlayer(CommandKind kind, uint8_t volume)
{
    std::optional<std::string> token = auth.accessToken(millis());
    if (!token) {
        return 0;
    }

    HttpsRequest request;
    request.method = PlayerApi::method(kind);
    request.url = PlayerApi::commandUrl(kind, volume);
    request.bearerToken = *token;

    HttpsResponse response = Https::send(request);
    if (response.status == UNAUTHORIZED) {
        auth.invalidateAccessToken();
    }
    if (response.status == NOT_FOUND) {
        applyNoDevice(millis());
    }
    return response.status;
}

void SpotifyClient::flushVolume()
{
    uint8_t volume = *pendingVolume;
    pendingVolume.reset();

    // 403 no volume: o aparelho não deixa (alguns alto-falantes e TVs); o slider some até o aparelho mudar.
    if (callPlayer(CommandKind::Volume, volume) == FORBIDDEN) {
        current.volumeAllowed = false;
        published.publish(current);
    }
}

void SpotifyClient::waitForCommand(uint32_t waitMillis)
{
    Command command;
    if (xQueueReceive(commands, &command, pdMS_TO_TICKS(waitMillis)) == pdTRUE) {
        handle(command, millis());
    }
}

uint32_t SpotifyClient::nextWaitMillis(uint32_t nowMillis) const
{
    uint32_t wait = std::min(remaining(nowMillis, nextPollMillis), IDLE_WAIT_MILLIS);
    if (pendingVolume) {
        wait = std::min(wait, remaining(nowMillis, volumeDueMillis));
    }
    return wait;
}
