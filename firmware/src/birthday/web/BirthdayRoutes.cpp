#include "birthday/web/BirthdayRoutes.h"

BirthdayRoutes::BirthdayRoutes(HttpServer& http)
    : http(http)
{
}

void BirthdayRoutes::begin()
{
    http.raw().on("/api/birthday/replay", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!http.authorize(request)) {
            return;
        }

        replayCount++;
        replays.publish(replayCount);
        request->send(204);
    });
}

const Published<uint32_t>& BirthdayRoutes::replayRequests() const
{
    return replays;
}
