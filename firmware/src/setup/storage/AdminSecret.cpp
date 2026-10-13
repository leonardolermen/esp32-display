#include "setup/storage/AdminSecret.h"

namespace {

constexpr const char* NAMESPACE = "admin";
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

void AdminSecret::save(const AdminPassword& password)
{
    stored = password.value();
    preferences.putString(KEY, stored.c_str());
}

bool AdminSecret::matches(const std::string& candidate) const
{
    return isSet() && candidate == stored;
}
