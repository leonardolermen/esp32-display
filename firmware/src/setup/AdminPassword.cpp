#include "setup/AdminPassword.h"

namespace {

std::string trimmed(const std::string& raw)
{
    size_t start = raw.find_first_not_of(' ');
    if (start == std::string::npos) {
        return "";
    }

    size_t end = raw.find_last_not_of(' ');
    return raw.substr(start, end - start + 1);
}

}

std::optional<AdminPassword> AdminPassword::parse(const std::string& raw)
{
    std::string value = trimmed(raw);
    if (value.size() < MIN_LENGTH || value.size() > MAX_LENGTH) {
        return std::nullopt;
    }

    return AdminPassword(value);
}

const std::string& AdminPassword::value() const
{
    return password;
}

AdminPassword::AdminPassword(std::string value)
    : password(std::move(value))
{
}
