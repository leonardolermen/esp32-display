#include "setup/web/AdminRoutes.h"

#include <ArduinoJson.h>

#include "setup/AdminPin.h"
#include "support/LocalTime.h"

AdminRoutes::AdminRoutes(HttpServer& http, AdminSecret& secret, const WifiLink& wifi)
    : http(http)
    , secret(secret)
    , wifi(wifi)
{
}

void AdminRoutes::begin()
{
    http.raw().on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument document;
        document["name"] = "claudinho";
        document["ip"] = wifi.ip();
        document["adminSet"] = secret.isSet();
        std::optional<LocalTime> now = LocalTime::now();
        document["time"] = now ? now->hourMinute() : "";

        String body;
        serializeJson(document, body);
        request->send(200, "application/json", body);
    });

    http.raw().on("/api/admin/pin", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            // Trocar um PIN existente exige estar autenticado com o atual.
            if (secret.isSet() && !http.authorize(request)) {
                return;
            }

            JsonDocument document;
            if (deserializeJson(document, data, length) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"field\":\"pin\",\"error\":\"JSON inválido\"}");
                return;
            }

            std::optional<AdminPin> pin = AdminPin::parse(document["pin"] | "");
            if (!pin) {
                request->send(400, "application/json", "{\"field\":\"pin\",\"error\":\"4 a 6 dígitos\"}");
                return;
            }

            secret.save(*pin);
            request->send(204);
        });
}
