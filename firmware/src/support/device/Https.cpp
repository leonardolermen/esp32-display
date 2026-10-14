#include "support/device/Https.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr uint16_t TIMEOUT_MILLIS = 5000;
constexpr int NO_CONTENT = 204;
const char* COLLECTED_HEADERS[] = {"Retry-After"};

}

std::unique_ptr<PsramBuffer> PsramBuffer::allocate(size_t capacity)
{
    char* bytes = static_cast<char*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (bytes == nullptr) {
        return nullptr;
    }

    return std::unique_ptr<PsramBuffer>(new PsramBuffer(bytes, capacity));
}

PsramBuffer::PsramBuffer(char* data, size_t capacity)
    : bytes(data)
    , room(capacity)
{
}

PsramBuffer::~PsramBuffer()
{
    heap_caps_free(bytes);
}

std::string_view PsramBuffer::view() const
{
    return std::string_view(bytes, used);
}

size_t PsramBuffer::write(uint8_t byte)
{
    return write(&byte, 1);
}

size_t PsramBuffer::write(const uint8_t* buffer, size_t size)
{
    size_t accepted = std::min(size, room - used);
    memcpy(bytes + used, buffer, accepted);
    used += accepted;
    return accepted;
}

int PsramBuffer::available()
{
    return 0;
}

int PsramBuffer::read()
{
    return -1;
}

int PsramBuffer::peek()
{
    return -1;
}

namespace Https {

HttpsResponse send(const HttpsRequest& request)
{
    HttpsResponse response;

    NetworkClientSecure client;
    // Certificados raiz embutidos no core: a conexão é verificada de verdade, sem setInsecure.
    client.useBuiltinCACertBundle();
    client.setTimeout(TIMEOUT_MILLIS);

    HTTPClient http;
    // Sem keep-alive o servidor fecha depois da resposta: um 204 sem Content-Length não deixa a leitura esperando o
    // socket fechar.
    http.setReuse(false);
    http.setTimeout(TIMEOUT_MILLIS);
    http.setConnectTimeout(TIMEOUT_MILLIS);
    if (!http.begin(client, request.url.c_str())) {
        return response;
    }

    http.collectHeaders(COLLECTED_HEADERS, 1);
    if (!request.bearerToken.empty()) {
        http.addHeader("Authorization", ("Bearer " + request.bearerToken).c_str());
    }
    if (!request.contentType.empty()) {
        http.addHeader("Content-Type", request.contentType.c_str());
    }
    // O HTTPClient só manda Content-Length quando há corpo; o Spotify recusa PUT e POST vazios sem ele (411).
    if (request.payload.empty() && strcmp(request.method, "GET") != 0) {
        http.addHeader("Content-Length", "0");
    }

    uint8_t* payload = reinterpret_cast<uint8_t*>(const_cast<char*>(request.payload.data()));
    response.status = http.sendRequest(request.method, payload, request.payload.size());
    response.retryAfter = http.header("Retry-After").c_str();

    bool hasBody = response.status >= 200 && response.status != NO_CONTENT;
    if (hasBody) {
        // Com Content-Length conhecido, reserva só o tamanho da resposta; chunked (-1) usa o teto.
        int declared = http.getSize();
        size_t capacity = declared > 0 ? std::min(static_cast<size_t>(declared), request.maxBodyBytes)
                                       : request.maxBodyBytes;
        std::unique_ptr<PsramBuffer> body = PsramBuffer::allocate(capacity);
        if (body && http.writeToStream(body.get()) > 0) {
            response.body = std::move(body);
        }
    }
    http.end();

    return response;
}

}
