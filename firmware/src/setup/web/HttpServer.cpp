#include "setup/web/HttpServer.h"

#include <mbedtls/base64.h>

#include <string>
#include <vector>

extern const uint8_t index_html_start[] asm("_binary_web_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_web_index_html_end");
extern const uint8_t app_css_start[] asm("_binary_web_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_web_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_web_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_web_app_js_end");
extern const uint8_t crop_js_start[] asm("_binary_web_crop_js_start");
extern const uint8_t crop_js_end[] asm("_binary_web_crop_js_end");

namespace {

constexpr const char* ADMIN_USER = "admin";

std::string decodeBase64(const String& encoded)
{
    std::vector<unsigned char> decoded(encoded.length() + 1);
    size_t written = 0;

    int status = mbedtls_base64_decode(decoded.data(), decoded.size(), &written,
        reinterpret_cast<const unsigned char*>(encoded.c_str()), encoded.length());
    if (status != 0) {
        return "";
    }

    return std::string(reinterpret_cast<const char*>(decoded.data()), written);
}

// O authenticate() do ESPAsyncWebServer compara com uma senha fixa; a nossa vem da NVS e pode mudar, então o
// header Basic é decodificado aqui e comparado com AdminSecret::matches.
bool carriesAdminCredentials(AsyncWebServerRequest* request, const AdminSecret& secret)
{
    if (!request->hasHeader("Authorization")) {
        return false;
    }

    String header = request->getHeader("Authorization")->value();
    if (!header.startsWith("Basic ")) {
        return false;
    }

    std::string credentials = decodeBase64(header.substring(6));
    size_t colon = credentials.find(':');
    if (colon == std::string::npos) {
        return false;
    }

    return credentials.substr(0, colon) == ADMIN_USER && secret.matches(credentials.substr(colon + 1));
}

}

HttpServer::HttpServer(AdminSecret& secret)
    : secret(secret)
{
}

void HttpServer::begin()
{
    serveEmbedded("/", "text/html", index_html_start, index_html_end);
    serveEmbedded("/app.css", "text/css", app_css_start, app_css_end);
    serveEmbedded("/app.js", "application/javascript", app_js_start, app_js_end);
    serveEmbedded("/crop.js", "application/javascript", crop_js_start, crop_js_end);

    server.begin();
}

AsyncWebServer& HttpServer::raw()
{
    return server;
}

bool HttpServer::authorize(AsyncWebServerRequest* request)
{
    if (carriesAdminCredentials(request, secret)) {
        return true;
    }

    AsyncWebServerResponse* response = request->beginResponse(401);
    response->addHeader("WWW-Authenticate", "Basic realm=\"Claudinho\"");
    request->send(response);
    return false;
}

void HttpServer::serveEmbedded(const char* path, const char* contentType, const uint8_t* start, const uint8_t* end)
{
    server.on(path, HTTP_GET, [=](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, contentType, start, end - start);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });
}
