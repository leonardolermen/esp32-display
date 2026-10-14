#pragma once

#include "setup/web/HttpServer.h"
#include "support/Published.h"

// Pedidos de "mostrar a mensagem de novo" vindos da página. A UI (núcleo 1) lê o contador e abre a camada.
class BirthdayRoutes {
public:
    explicit BirthdayRoutes(HttpServer& http);

    void begin();
    const Published<uint32_t>& replayRequests() const;

private:
    HttpServer& http;
    Published<uint32_t> replays;
    uint32_t replayCount = 0;
};
