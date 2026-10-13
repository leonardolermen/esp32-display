#include "photos/web/PhotoRoutes.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <cstdlib>
#include <cstring>

#include "photos/UploadCheck.h"

namespace {

constexpr const char* TOO_BIG = "foto maior que 200 KB";
constexpr const char* STORAGE_FULL = "memória cheia";
constexpr const char* ANOTHER_UPLOAD_RUNNING = "outro envio em andamento";
constexpr const char* NO_FILE = "nenhum arquivo enviado";

std::optional<PhotoName> nameFromPath(AsyncWebServerRequest* request)
{
    String url = request->url();
    return PhotoName::parse(url.substring(url.lastIndexOf('/') + 1).c_str());
}

void sendError(AsyncWebServerRequest* request, int status, const char* message)
{
    JsonDocument document;
    document["error"] = message;

    String body;
    serializeJson(document, body);
    request->send(status, "application/json", body);
}

int statusFor(const char* refusal)
{
    if (strcmp(refusal, STORAGE_FULL) == 0) {
        return 507;
    }

    if (strcmp(refusal, TOO_BIG) == 0) {
        return 413;
    }

    if (strcmp(refusal, ANOTHER_UPLOAD_RUNNING) == 0) {
        return 409;
    }

    return 400;
}

// Mensagens vêm de constantes com vida estática: o struct é liberado com free() e não pode ter std::string.
const char* keep(const std::optional<std::string>& refusal)
{
    if (!refusal) {
        return nullptr;
    }

    if (*refusal == STORAGE_FULL) {
        return STORAGE_FULL;
    }

    if (*refusal == TOO_BIG) {
        return TOO_BIG;
    }

    return "limite de 200 fotos";
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
    server.on("^/api/photos/(p[0-9]{4}\\.jpg)$", HTTP_DELETE,
        [this](AsyncWebServerRequest* request) { remove(request); });
    server.on("^/photos/(p[0-9]{4}\\.jpg)$", HTTP_GET, [this](AsyncWebServerRequest* request) { serve(request); });

    server.on("/api/photos", HTTP_POST,
        [this](AsyncWebServerRequest* request) { finish(request); },
        [this](AsyncWebServerRequest* request, const String&, size_t index, uint8_t* data, size_t length, bool) {
            receive(request, data, length, index);
        });
}

PhotoRoutes::Upload* PhotoRoutes::startUpload(AsyncWebServerRequest* request)
{
    Upload* upload = static_cast<Upload*>(calloc(1, sizeof(Upload)));
    request->_tempObject = upload;
    // Se o cliente sumir no meio, o servidor descarta o request; o temporário aberto é fechado aqui.
    request->onDisconnect([this, request, upload]() { releaseUpload(request, upload); });

    upload->authorized = http.isAuthorized(request);
    if (!upload->authorized) {
        return upload;
    }

    if (uploading != nullptr) {
        upload->refusal = ANOTHER_UPLOAD_RUNNING;
        return upload;
    }

    upload->refusal = keep(UploadCheck::refuse(request->contentLength(), store.freeBytes(), store.list().size()));
    if (upload->refusal != nullptr) {
        return upload;
    }

    if (!store.beginUpload()) {
        upload->refusal = "não consegui abrir o arquivo";
        return upload;
    }

    uploading = request;
    upload->owner = true;
    return upload;
}

void PhotoRoutes::releaseUpload(AsyncWebServerRequest* request, Upload* upload)
{
    if (upload->owner && uploading == request) {
        store.abortUpload();
        uploading = nullptr;
        upload->owner = false;
    }
}

void PhotoRoutes::receive(AsyncWebServerRequest* request, const uint8_t* data, size_t length, size_t index)
{
    Upload* upload = index == 0 ? startUpload(request) : static_cast<Upload*>(request->_tempObject);
    if (!upload->owner) {
        return;
    }

    upload->received += length;
    upload->refusal = keep(UploadCheck::refuseReceived(upload->received));
    if (upload->refusal == nullptr && !store.writeChunk(data, length)) {
        upload->refusal = STORAGE_FULL;
    }

    if (upload->refusal != nullptr) {
        releaseUpload(request, upload);
    }
}

void PhotoRoutes::finish(AsyncWebServerRequest* request)
{
    Upload* upload = static_cast<Upload*>(request->_tempObject);
    if (upload == nullptr) {
        if (http.authorize(request)) {
            sendError(request, 400, NO_FILE);
        }
        return;
    }

    if (!upload->authorized) {
        http.authorize(request);
        return;
    }

    if (upload->refusal != nullptr) {
        sendError(request, statusFor(upload->refusal), upload->refusal);
        return;
    }

    PhotoName name = store.nextName();
    bool saved = store.finishUpload(name);
    uploading = nullptr;
    upload->owner = false;
    if (!saved) {
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

    // O número pode ser reaproveitado depois de apagar a última foto; sem cache o navegador não mostra a antiga.
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, name->path().c_str(), "image/jpeg");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
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
