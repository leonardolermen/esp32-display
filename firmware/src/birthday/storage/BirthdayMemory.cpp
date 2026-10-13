#include "birthday/storage/BirthdayMemory.h"

namespace {

constexpr const char* NAMESPACE = "birthday";
constexpr const char* SHOWN_KEY = "shown";
constexpr const char* YEAR_KEY = "year";

}

void BirthdayMemory::begin()
{
    preferences.begin(NAMESPACE, false);
    shown = preferences.getBool(SHOWN_KEY, false);
    year = preferences.getInt(YEAR_KEY, 0);
}

bool BirthdayMemory::neverShown() const
{
    return !shown;
}

std::optional<int> BirthdayMemory::lastShownYear() const
{
    return year == 0 ? std::nullopt : std::optional<int>(year);
}

void BirthdayMemory::markShown(std::optional<int> shownYear)
{
    shown = true;
    preferences.putBool(SHOWN_KEY, true);

    if (shownYear) {
        year = *shownYear;
        preferences.putInt(YEAR_KEY, year);
    }
}
