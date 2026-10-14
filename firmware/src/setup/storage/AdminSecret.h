#pragma once

#include <Preferences.h>

#include <string>

#include "setup/AdminPin.h"

class AdminSecret {
public:
    void begin();
    bool isSet() const;
    void save(const AdminPin& pin);
    bool matches(const std::string& candidate) const;

private:
    Preferences preferences;
    std::string stored;
};
