#include "spotify/AuthFlow.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr const char* AUTHORIZE_URL = "https://accounts.spotify.com/authorize";
constexpr size_t NONCE_LENGTH = 32;
constexpr size_t MAX_PORT_DIGITS = 5;
constexpr unsigned long MAX_PORT = 65535;

bool isUnreserved(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) || character == '-' || character == '.'
        || character == '_' || character == '~';
}

bool isLowerHex(char character)
{
    return std::isdigit(static_cast<unsigned char>(character)) || (character >= 'a' && character <= 'f');
}

bool isDigit(char character)
{
    return std::isdigit(static_cast<unsigned char>(character));
}

bool allOf(const std::string& text, bool (*accepts)(char))
{
    return std::all_of(text.begin(), text.end(), accepts);
}

int hexValue(char character)
{
    if (std::isdigit(static_cast<unsigned char>(character))) {
        return character - '0';
    }

    return std::tolower(static_cast<unsigned char>(character)) - 'a' + 10;
}

std::string urlDecode(const std::string& text)
{
    std::string decoded;
    for (size_t index = 0; index < text.size(); index++) {
        bool escaped = text[index] == '%' && index + 2 < text.size();
        if (escaped) {
            decoded += static_cast<char>(hexValue(text[index + 1]) * 16 + hexValue(text[index + 2]));
            index += 2;
            continue;
        }
        decoded += text[index] == '+' ? ' ' : text[index];
    }
    return decoded;
}

std::optional<std::string> queryValue(const std::string& query, const std::string& key)
{
    size_t start = 0;
    while (start <= query.size()) {
        size_t end = query.find('&', start);
        std::string pair = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        size_t equals = pair.find('=');
        if (equals != std::string::npos && pair.substr(0, equals) == key) {
            return urlDecode(pair.substr(equals + 1));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return std::nullopt;
}

}

namespace AuthFlow {

std::string urlEncode(const std::string& text)
{
    std::string encoded;
    char buffer[4];
    for (char character : text) {
        if (isUnreserved(character)) {
            encoded += character;
            continue;
        }
        snprintf(buffer, sizeof(buffer), "%%%02X", static_cast<unsigned char>(character));
        encoded += buffer;
    }
    return encoded;
}

std::string state(const std::string& ip, uint16_t port, const std::string& nonce)
{
    return ip + ":" + std::to_string(port) + "." + nonce;
}

std::optional<HostAndNonce> parseState(const std::string& state)
{
    size_t colon = state.find(':');
    size_t dot = colon == std::string::npos ? std::string::npos : state.find('.', colon);
    if (colon == 0 || dot == std::string::npos) {
        return std::nullopt;
    }

    std::string portText = state.substr(colon + 1, dot - colon - 1);
    std::string nonce = state.substr(dot + 1);
    bool portLooksRight = !portText.empty() && portText.size() <= MAX_PORT_DIGITS && allOf(portText, isDigit);
    unsigned long port = portLooksRight ? std::strtoul(portText.c_str(), nullptr, 10) : 0;
    bool nonceLooksRight = nonce.size() == NONCE_LENGTH && allOf(nonce, isLowerHex);
    if (port == 0 || port > MAX_PORT || !nonceLooksRight) {
        return std::nullopt;
    }

    return HostAndNonce{state.substr(0, colon), static_cast<uint16_t>(port), nonce};
}

std::string authorizeUrl(const std::string& clientId, const std::string& challenge, const std::string& state)
{
    return std::string(AUTHORIZE_URL) + "?response_type=code&client_id=" + urlEncode(clientId)
        + "&scope=" + urlEncode(SCOPES) + "&redirect_uri=" + urlEncode(REDIRECT_URI)
        + "&code_challenge_method=S256&code_challenge=" + challenge + "&state=" + urlEncode(state);
}

CallbackCheck check(const Pending& pending, const std::string& state, uint32_t nowMillis)
{
    std::optional<HostAndNonce> parsed = parseState(state);
    if (!parsed || pending.nonce.empty() || parsed->nonce != pending.nonce) {
        return CallbackCheck::Mismatch;
    }

    bool fresh = nowMillis - pending.createdMillis < VALID_FOR_MILLIS;
    return fresh ? CallbackCheck::Accepted : CallbackCheck::Expired;
}

std::optional<Callback> fromPastedUrl(const std::string& url)
{
    size_t question = url.find('?');
    if (question == std::string::npos) {
        return std::nullopt;
    }

    std::string query = url.substr(question + 1);
    query = query.substr(0, query.find('#'));
    std::optional<std::string> code = queryValue(query, "code");
    std::optional<std::string> state = queryValue(query, "state");
    if (!code || !state) {
        return std::nullopt;
    }

    return Callback{*code, *state};
}

std::string tokenRequestBody(const std::string& clientId, const std::string& code, const std::string& verifier)
{
    return "grant_type=authorization_code&code=" + urlEncode(code) + "&redirect_uri=" + urlEncode(REDIRECT_URI)
        + "&client_id=" + urlEncode(clientId) + "&code_verifier=" + urlEncode(verifier);
}

std::string refreshRequestBody(const std::string& clientId, const std::string& refreshToken)
{
    return "grant_type=refresh_token&refresh_token=" + urlEncode(refreshToken) + "&client_id=" + urlEncode(clientId);
}

}
