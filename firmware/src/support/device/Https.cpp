#include "support/device/Https.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

constexpr uint16_t TIMEOUT_MILLIS = 5000;
constexpr int NO_CONTENT = 204;
const char* COLLECTED_HEADERS[] = {"Retry-After"};

constexpr uint32_t BODY_DEADLINE_MILLIS = 10000;
constexpr size_t READ_CHUNK_BYTES = 512;

// Lê o corpo direto do socket, com prazo e cedendo o processador quando não há dados. O leitor do HTTPClient espera
// girando sem ceder e, no modo chunked, ficava preso para sempre numa resposta do CoinGecko até o watchdog
// reiniciar a placa.
size_t readBody(HTTPClient& http, PsramBuffer& body, int declaredBytes)
{
    NetworkClient* stream = http.getStreamPtr();
    uint8_t chunk[READ_CHUNK_BYTES];
    size_t received = 0;
    uint32_t deadline = millis() + BODY_DEADLINE_MILLIS;

    while (static_cast<int32_t>(millis() - deadline) < 0) {
        bool complete = declaredBytes >= 0 && received >= static_cast<size_t>(declaredBytes);
        if (complete || stream == nullptr) {
            break;
        }

        int waiting = stream->available();
        if (waiting > 0) {
            int got = stream->read(chunk, std::min(sizeof(chunk), static_cast<size_t>(waiting)));
            if (got > 0) {
                body.write(chunk, static_cast<size_t>(got));
                received += static_cast<size_t>(got);
                continue;
            }
        }
        if (!stream->connected()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return received;
}

// Uma conexão TLS por vez. Neste core o mbedTLS só usa RAM interna (~40 KB por conexão), a mesma que o driver da
// tela usa para o DMA de cada quadro; com Spotify, capa e cotações abrindo conexões juntas, a RAM acabava e o driver
// da tela abortava, reiniciando a placa. Em fila, uma requisição espera alguns segundos no pior caso.
std::mutex oneConnectionAtATime;

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
    std::lock_guard<std::mutex> lock(oneConnectionAtATime);
    HttpsResponse response;

    NetworkClientSecure client;
    // Certificados raiz embutidos no core: a conexão é verificada de verdade, sem setInsecure.
    client.useBuiltinCACertBundle();
    client.setTimeout(TIMEOUT_MILLIS);

    HTTPClient http;
    // HTTP/1.0 não tem chunked: o corpo vem inteiro e o servidor fecha a conexão no fim, o que o readBody entende.
    http.useHTTP10(true);
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

    // 204 não tem corpo: ler esperaria o servidor fechar a conexão. Não usar setReuse(false) para isso — pedindo
    // "Connection: close", o CoinGecko responde sem tamanho e a leitura gira até o fechamento, travando a tarefa.
    bool hasBody = response.status >= 200 && response.status != NO_CONTENT;
    if (hasBody) {
        // Com Content-Length conhecido, reserva só o tamanho da resposta; chunked (-1) usa o teto.
        int declared = http.getSize();
        size_t capacity = declared > 0 ? std::min(static_cast<size_t>(declared), request.maxBodyBytes)
                                       : request.maxBodyBytes;
        std::unique_ptr<PsramBuffer> body = PsramBuffer::allocate(capacity);
        if (body && readBody(http, *body, declared) > 0) {
            response.body = std::move(body);
        }
    }
    http.end();

    return response;
}

}
