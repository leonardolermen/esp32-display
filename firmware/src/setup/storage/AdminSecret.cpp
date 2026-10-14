#include "setup/storage/AdminSecret.h"

namespace {

constexpr const char* NAMESPACE = "admin";
// A chave continua "password" na NVS: trocar o nome apagaria o PIN de quem já configurou.
constexpr const char* KEY = "password";

}

void AdminSecret::begin()
{
    preferences.begin(NAMESPACE, false);
    stored = preferences.getString(KEY, "").c_str();
}

bool AdminSecret::isSet() const
{
    return !stored.empty();
}

void AdminSecret::save(const AdminPin& pin)
{
    stored = pin.value();
    preferences.putString(KEY, stored.c_str());
}

bool AdminSecret::matches(const std::string& candidate) const
{
    return isSet() && candidate == stored;
}
