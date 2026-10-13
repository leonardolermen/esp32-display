#include "birthday/BirthdayCalendar.h"

#include "birthday/BirthdayContent.h"

bool BirthdayCalendar::shouldShow(bool neverShown, std::optional<LocalTime> now, std::optional<int> lastShownYear)
{
    // O presente é aberto antes de qualquer Wi-Fi: na primeira vez a mensagem aparece sem precisar de relógio.
    if (neverShown) {
        return true;
    }

    if (!now || now->day != birthday::DAY || now->month != birthday::MONTH) {
        return false;
    }

    return !lastShownYear || *lastShownYear != now->year;
}
