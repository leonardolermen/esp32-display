#include "spotify/web/SpotifyRoutes.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

constexpr size_t MAX_CLIENT_ID_LENGTH = 64;
constexpr size_t PAGE_BYTES = 768;
constexpr const char* PAGE = "<!doctype html><html lang=\"pt-BR\"><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Claudinho</title>"
    "<body style=\"margin:0;background:#1F1E1C;color:#F5F1EA;font:18px/1.5 system-ui,sans-serif\">"
    "<main style=\"max-width:560px;margin:0 auto;padding:32px 24px\"><h1 style=\"font-weight:500\">%s</h1>"
    "<p style=\"color:#A8A29A\">%s</p></main></body></html>";

struct Outcome {
    int status;
    const char* title;
    const char* text;
};

// Na ordem de AuthFlow::CallbackCheck.
constexpr Outcome OUTCOMES[] = {
    {200, "Spotify conectando", "Pode fechar esta aba. Em alguns segundos a música aparece na tela do Claudinho."},
    {410, "Link vencido", "O login demorou mais de 10 minutos. Toque em Conectar de novo em claudinho.local."},
    {400, "Link de outro login",
        "Este link não foi gerado por este Claudinho. Toque em Conectar de novo em claudinho.local."},
};

void sendPage(AsyncWebServerRequest* request, int status, const char* title, const char* text)
{
    char buffer[PAGE_BYTES];
    snprintf(buffer, sizeof(buffer), PAGE, title, text);
    request->send(status, "text/html; charset=utf-8", buffer);
}

void sendJson(AsyncWebServerRequest* request, int status, const JsonDocument& document)
{
    String body;
    serializeJson(document, body);
    request->send(status, "application/json", body);
}

void sendError(AsyncWebServerRequest* request, int status, const char* field, const char* error)
{
    JsonDocument document;
    if (field != nullptr) {
        document["field"] = field;
    }
    document["error"] = error;
    sendJson(request, status, document);
}

bool looksLikeClientId(const std::string& clientId)
{
    return !clientId.empty() && clientId.size() <= MAX_CLIENT_ID_LENGTH
        && std::all_of(clientId.begin(), clientId.end(), [](char character) {
               return std::isalnum(static_cast<unsigned char>(character));
           });
}

const Outcome& outcomeOf(AuthFlow::CallbackCheck check)
{
    return OUTCOMES[static_cast<size_t>(check)];
}

}

SpotifyRoutes::SpotifyRoutes(HttpServer& http, SpotifyAuth& auth, SpotifyVault& vault, const WifiLink& wifi)
    : http(http)
    , auth(auth)
    , vault(vault)
    , wifi(wifi)
{
}

void SpotifyRoutes::begin()
{
    AsyncWebServer& server = http.raw();

    // O ESPAsyncWebServer casa "/api/spotify" também com "/api/spotify/…": o callback tem que vir antes do status.
    server.on("/api/spotify/callback", HTTP_GET, [this](AsyncWebServerRequest* request) { callback(request); });
    server.on("/api/spotify/connect", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            connect(request, data, length);
        });
    server.on("/api/spotify/paste", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t length, size_t, size_t) {
            paste(request, data, length);
        });
    server.on("/api/spotify", HTTP_GET, [this](AsyncWebServerRequest* request) { status(request); });
    server.on("/api/spotify", HTTP_DELETE, [this](AsyncWebServerRequest* request) { disconnect(request); });
}

// Aberto de propósito: chega do celular que fez o login, sem a senha da página. O nonce de 128 bits no state é a
// proteção, e ele vale uma vez só.
void SpotifyRoutes::callback(AsyncWebServerRequest* request)
{
    if (request->hasParam("error")) {
        sendPage(request, 400, "Login cancelado",
            "Nada foi alterado. Se quiser, toque em Conectar de novo em claudinho.local.");
        return;
    }
    if (!request->hasParam("code") || !request->hasParam("state")) {
        sendPage(request, 400, "Link incompleto", "Toque em Conectar de novo em claudinho.local.");
        return;
    }

    std::string code = request->getParam("code")->value().c_str();
    std::string state = request->getParam("state")->value().c_str();
    const Outcome& outcome = outcomeOf(auth.acceptCallback(code, state, millis()));
    sendPage(request, outcome.status, outcome.title, outcome.text);
}

void SpotifyRoutes::status(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument document;
    document["connected"] = auth.connected();
    document["clientId"] = vault.clientId();
    sendJson(request, 200, document);
}

void SpotifyRoutes::connect(AsyncWebServerRequest* request, const uint8_t* data, size_t length)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument body;
    deserializeJson(body, data, length);
    std::string clientId = body["clientId"] | "";
    if (!clientId.empty() && !looksLikeClientId(clientId)) {
        sendError(request, 400, "clientId", "Client ID inválido");
        return;
    }
    if (!clientId.empty()) {
        vault.saveClientId(clientId);
    }

    JsonDocument document;
    document["url"] = auth.startLogin(wifi.ip(), millis());
    sendJson(request, 200, document);
}

void SpotifyRoutes::paste(AsyncWebServerRequest* request, const uint8_t* data, size_t length)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument body;
    deserializeJson(body, data, length);
    std::optional<AuthFlow::Callback> callback = AuthFlow::fromPastedUrl(body["url"] | "");
    if (!callback) {
        sendError(request, 400, "url", "link sem code e state");
        return;
    }

    AuthFlow::CallbackCheck check = auth.acceptCallback(callback->code, callback->state, millis());
    if (check == AuthFlow::CallbackCheck::Accepted) {
        request->send(202);
        return;
    }

    const Outcome& outcome = outcomeOf(check);
    sendError(request, outcome.status, "url", outcome.title);
}

void SpotifyRoutes::disconnect(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    auth.disconnect();
    request->send(204);
}
