#pragma once

#include <optional>
#include <string>

// PIN como o de celular: só dígitos, de 4 a 6. É o que protege a página de administração na rede de casa.
class AdminPin {
public:
    static constexpr size_t MIN_DIGITS = 4;
    static constexpr size_t MAX_DIGITS = 6;

    static std::optional<AdminPin> parse(const std::string& raw);

    const std::string& value() const;

private:
    explicit AdminPin(std::string digits);

    std::string digits;
};
