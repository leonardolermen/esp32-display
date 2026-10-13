#pragma once

#include <optional>
#include <string>

struct LocalTime {
    int hour;
    int minute;
    int day;
    int month;
    int year;
    int weekday;

    static std::optional<LocalTime> now();

    std::string hourMinute() const;
    std::string longDate() const;
};
