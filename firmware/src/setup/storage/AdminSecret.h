#pragma once

#include <Preferences.h>

#include <string>

#include "setup/AdminPassword.h"

class AdminSecret {
public:
    void begin();
    bool isSet() const;
    void save(const AdminPassword& password);
    bool matches(const std::string& candidate) const;

private:
    Preferences preferences;
    std::string stored;
};
