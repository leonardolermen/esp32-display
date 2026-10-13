#pragma once

#include <string>

enum class SetupStep {
    WaitingWifi,
    WaitingAdmin,
    Done,
};

struct SetupState {
    SetupStep step = SetupStep::WaitingWifi;
    std::string qrText;
    std::string ip;
};

class SetupProgress {
public:
    static SetupState compute(bool wifiConnected, const std::string& ip, bool adminPasswordSet);
};
