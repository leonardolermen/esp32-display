#pragma once

#include <Stream.h>

#include <memory>
#include <string>
#include <string_view>

// Corpo de resposta na PSRAM: respostas de API passam de vários KB, grandes demais para a heap interna. É um Stream
// para o HTTPClient escrever nele já sem a codificação chunked.
class PsramBuffer : public Stream {
public:
    static std::unique_ptr<PsramBuffer> allocate(size_t capacity);
    ~PsramBuffer() override;

    std::string_view view() const;

    size_t write(uint8_t byte) override;
    size_t write(const uint8_t* buffer, size_t size) override;
    int available() override;
    int read() override;
    int peek() override;

private:
    PsramBuffer(char* data, size_t capacity);

    char* bytes;
    size_t room;
    size_t used = 0;
};

struct HttpsRequest {
    const char* method = "GET";
    std::string url;
    std::string bearerToken;
    std::string contentType;
    std::string payload;
    size_t maxBodyBytes = 64 * 1024;
};

struct HttpsResponse {
    int status = 0;
    std::unique_ptr<PsramBuffer> body;
    std::string retryAfter;
};

namespace Https {

HttpsResponse send(const HttpsRequest& request);

}
