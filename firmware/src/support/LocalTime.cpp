#include "support/LocalTime.h"

#include <ctime>

namespace {

// Antes do NTP o relógio do ESP32 começa em 1970; qualquer coisa antes de 2023 é "ainda não sincronizou".
constexpr time_t FIRST_PLAUSIBLE_EPOCH = 1700000000;

constexpr const char* WEEKDAYS[] = {"domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado"};
constexpr const char* MONTHS[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto",
    "setembro", "outubro", "novembro", "dezembro"};

// O MinGW do PC de desenvolvimento não tem localtime_r; a placa e o Mac têm.
void toLocal(time_t epoch, tm& local)
{
#ifdef _WIN32
    localtime_s(&local, &epoch);
#else
    localtime_r(&epoch, &local);
#endif
}

std::string twoDigits(int value)
{
    std::string text = std::to_string(value);
    return value < 10 ? "0" + text : text;
}

}

std::optional<LocalTime> LocalTime::now()
{
    time_t epoch = time(nullptr);
    if (epoch < FIRST_PLAUSIBLE_EPOCH) {
        return std::nullopt;
    }

    tm local{};
    toLocal(epoch, local);

    return LocalTime{local.tm_hour, local.tm_min, local.tm_mday, local.tm_mon + 1, local.tm_year + 1900, local.tm_wday};
}

std::string LocalTime::hourMinute() const
{
    return twoDigits(hour) + ":" + twoDigits(minute);
}

std::string LocalTime::longDate() const
{
    return std::string(WEEKDAYS[weekday]) + ", " + std::to_string(day) + " de " + MONTHS[month - 1];
}
