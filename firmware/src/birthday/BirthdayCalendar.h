#pragma once

#include <optional>

#include "support/LocalTime.h"

class BirthdayCalendar {
public:
    static bool shouldShow(bool neverShown, std::optional<LocalTime> now, std::optional<int> lastShownYear);
};
