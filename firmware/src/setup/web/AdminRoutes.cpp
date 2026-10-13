#include "setup/web/AdminRoutes.h"

#include <ArduinoJson.h>

#include "setup/AdminPassword.h"
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

    http.raw().on("/api/admin/password", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            // Trocar uma senha existente exige estar autenticado com a atual.
            if (secret.isSet() && !http.authorize(request)) {
                return;
            }

            JsonDocument document;
            if (deserializeJson(document, data, length) != DeserializationError::Ok) {
                request->send(400, "application/json", "{\"field\":\"password\",\"error\":\"JSON inválido\"}");
                return;
            }

            std::optional<AdminPassword> password = AdminPassword::parse(document["password"] | "");
            if (!password) {
                request->send(400, "application/json", "{\"field\":\"password\",\"error\":\"8 a 64 caracteres\"}");
                return;
            }

            secret.save(*password);
            request->send(204);
        });
}
