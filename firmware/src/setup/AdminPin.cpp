#include "setup/AdminPin.h"

#include <algorithm>
#include <cctype>

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

bool onlyDigits(const std::string& text)
{
    return std::all_of(text.begin(), text.end(), [](char character) {
        return std::isdigit(static_cast<unsigned char>(character));
    });
}

}

std::optional<AdminPin> AdminPin::parse(const std::string& raw)
{
    std::string value = trimmed(raw);
    if (value.size() < MIN_DIGITS || value.size() > MAX_DIGITS || !onlyDigits(value)) {
        return std::nullopt;
    }

    return AdminPin(value);
}

const std::string& AdminPin::value() const
{
    return digits;
}

AdminPin::AdminPin(std::string digits)
    : digits(std::move(digits))
{
}
