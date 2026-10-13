#pragma once

#include <optional>
#include <string>

class AdminPassword {
public:
    static constexpr size_t MIN_LENGTH = 8;
    static constexpr size_t MAX_LENGTH = 64;

    static std::optional<AdminPassword> parse(const std::string& raw);

    const std::string& value() const;

private:
    explicit AdminPassword(std::string value);

    std::string password;
};
