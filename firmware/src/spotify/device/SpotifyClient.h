#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include "spotify/Command.h"
#include "spotify/CoverArt.h"
#include "spotify/PlayerState.h"
#include "spotify/device/CoverFetcher.h"
#include "spotify/device/SpotifyAuth.h"
#include "support/Published.h"
#include "support/device/Https.h"

// Tarefa própria no núcleo 0: consulta o player, executa os comandos da tela e baixa a capa. A UI só lê o que é
// publicado e manda comandos pela fila.
class SpotifyClient {
public:
    explicit SpotifyClient(SpotifyAuth& auth);

    void begin();
    const Published<PlayerState>& state() const;
    const Published<CoverArt>& cover() const;
    void send(Command command);
    void requestVolume(uint8_t volume);
    void watch(bool watching);

private:
    static void run(void* self);
    void loop();
    void showLogin(uint32_t nowMillis);
    void markStale();
    void poll(uint32_t nowMillis);
    uint32_t applyPlayer(const HttpsResponse& response, uint32_t nowMillis);
    uint32_t applyNoDevice(uint32_t nowMillis);
    uint32_t applyFailure(uint32_t nowMillis);
    uint32_t interval(uint32_t nowMillis) const;
    void refreshCover();
    void handle(const Command& command, uint32_t nowMillis);
    int callPlayer(CommandKind kind, uint8_t volume);
    void flushVolume();
    void waitForCommand(uint32_t waitMillis);
    uint32_t nextWaitMillis(uint32_t nowMillis) const;

    SpotifyAuth& auth;
    CoverFetcher covers;
    QueueHandle_t commands = nullptr;
    std::atomic<int> watchers{0};
    // Só o último valor do slider importa: a fila perderia o final de um arrasto rápido.
    std::atomic<int> requestedVolume{-1};
    Published<PlayerState> published;
    Published<CoverArt> publishedCover;
    PlayerState current;
    std::string coverAlbumId;
    uint32_t nextPollMillis = 0;
    uint32_t pausedSinceMillis = 0;
    std::optional<uint8_t> pendingVolume;
    uint32_t volumeDueMillis = 0;
};
