#pragma once

#include <Preferences.h>

#include <optional>

class BirthdayMemory {
public:
    void begin();
    bool neverShown() const;
    std::optional<int> lastShownYear() const;
    void markShown(std::optional<int> year);

private:
    Preferences preferences;
    bool shown = false;
    int year = 0;
};
