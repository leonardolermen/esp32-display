#include "photos/web/PhotoRoutes.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "photos/UploadCheck.h"

namespace {

constexpr const char* TOO_BIG = "foto maior que 200 KB";
constexpr const char* STORAGE_FULL = "memória cheia";

std::optional<PhotoName> nameFromPath(AsyncWebServerRequest* request)
{
    String url = request->url();
    return PhotoName::parse(url.substring(url.lastIndexOf('/') + 1).c_str());
}

void sendError(AsyncWebServerRequest* request, int status, const std::string& message)
{
    JsonDocument document;
    document["error"] = message;

    String body;
    serializeJson(document, body);
    request->send(status, "application/json", body);
}

int statusFor(const std::string& refusal)
{
    if (refusal == STORAGE_FULL) {
        return 507;
    }

    if (refusal == TOO_BIG) {
        return 413;
    }

    return 400;
}

}

PhotoRoutes::PhotoRoutes(HttpServer& http, PhotoStore& store)
    : http(http)
    , store(store)
{
}

void PhotoRoutes::begin()
{
    AsyncWebServer& server = http.raw();

    server.on("/api/photos", HTTP_GET, [this](AsyncWebServerRequest* request) { list(request); });
    server.on("^/api/photos/(p[0-9]{4}\\.jpg)$", HTTP_DELETE, [this](AsyncWebServerRequest* request) { remove(request); });
    server.on("^/photos/(p[0-9]{4}\\.jpg)$", HTTP_GET, [this](AsyncWebServerRequest* request) { serve(request); });

    server.on("/api/photos", HTTP_POST,
        [this](AsyncWebServerRequest* request) { finish(request); },
        [this](AsyncWebServerRequest* request, const String&, size_t index, uint8_t* data, size_t length, bool) {
            receive(request, data, length, index);
        });
}

void PhotoRoutes::receive(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index)
{
    if (index == 0) {
        refusal.reset();
        if (!http.authorize(request)) {
            refusal = "não autorizado";
            return;
        }

        refusal = UploadCheck::refuse(request->contentLength(), store.freeBytes(), store.list().size());
        if (refusal) {
            return;
        }

        if (!store.beginUpload()) {
            refusal = "não consegui abrir o arquivo";
            return;
        }
    }

    if (refusal) {
        return;
    }

    if (!store.writeChunk(data, length)) {
        store.abortUpload();
        refusal = STORAGE_FULL;
    }
}

void PhotoRoutes::finish(AsyncWebServerRequest* request)
{
    if (refusal) {
        sendError(request, statusFor(*refusal), *refusal);
        return;
    }

    PhotoName name = store.nextName();
    if (!store.finishUpload(name)) {
        sendError(request, 500, "não consegui gravar a foto");
        return;
    }

    JsonDocument document;
    document["name"] = name.fileName();

    String body;
    serializeJson(document, body);
    request->send(201, "application/json", body);
}

void PhotoRoutes::list(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    JsonDocument document;
    JsonArray photos = document.to<JsonArray>();
    for (const PhotoName& name : store.list()) {
        JsonObject photo = photos.add<JsonObject>();
        photo["name"] = name.fileName();
        photo["bytes"] = store.open(name).size();
    }

    String body;
    serializeJson(document, body);
    request->send(200, "application/json", body);
}

void PhotoRoutes::serve(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    std::optional<PhotoName> name = nameFromPath(request);
    if (!name || !LittleFS.exists(name->path().c_str())) {
        request->send(404);
        return;
    }

    request->send(LittleFS, name->path().c_str(), "image/jpeg");
}

void PhotoRoutes::remove(AsyncWebServerRequest* request)
{
    if (!http.authorize(request)) {
        return;
    }

    std::optional<PhotoName> name = nameFromPath(request);
    if (!name || !store.remove(*name)) {
        request->send(404);
        return;
    }

    request->send(204);
}
