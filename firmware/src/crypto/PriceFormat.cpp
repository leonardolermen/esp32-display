#include "crypto/PriceFormat.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr double CENTS_THRESHOLD = 1000.0;
// Hífen comum, não o sinal de menos tipográfico (U+2212): as fontes Inter geradas cobrem só Latin-1.
constexpr const char* MINUS_SIGN = "-";

std::string groupThousands(long long value)
{
    std::string digits = std::to_string(value);
    std::string grouped;

    for (size_t position = 0; position < digits.size(); position++) {
        size_t remaining = digits.size() - position;
        if (position > 0 && remaining % 3 == 0) {
            grouped += '.';
        }
        grouped += digits[position];
    }

    return grouped;
}

std::string oneDecimalWithComma(double value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f", value);

    std::string text = buffer;
    text.replace(text.find('.'), 1, ",");
    return text;
}

}

namespace PriceFormat {

std::string reais(double value)
{
    // Arredonda nos centavos primeiro: 999,995 vira 1.000,00 e cai na regra de milhar.
    double cents = std::round(value * 100.0);
    if (cents >= CENTS_THRESHOLD * 100.0) {
        return "R$ " + groupThousands(static_cast<long long>(std::round(cents / 100.0)));
    }

    long long whole = static_cast<long long>(cents / 100.0);
    long long fraction = static_cast<long long>(cents) % 100;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "R$ %lld,%02lld", whole, fraction);
    return buffer;
}

std::string percent(double value)
{
    std::string magnitude = oneDecimalWithComma(std::fabs(value));
    bool negative = magnitude != "0,0" && value < 0;

    return (negative ? MINUS_SIGN : "+") + magnitude + " %";
}

}
