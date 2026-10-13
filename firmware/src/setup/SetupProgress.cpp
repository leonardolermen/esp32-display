#include "setup/SetupProgress.h"

namespace {

// QR no formato que iPhone e Android reconhecem como "entrar nesta rede".
constexpr const char* HOTSPOT_QR = "WIFI:S:Claudinho;T:nopass;;";
constexpr const char* PAGE_URL = "http://claudinho.local";

}

SetupState SetupProgress::compute(bool wifiConnected, const std::string& ip, bool adminPasswordSet)
{
    SetupState state;

    if (!wifiConnected) {
        state.step = SetupStep::WaitingWifi;
        state.qrText = HOTSPOT_QR;
        return state;
    }

    state.ip = ip;
    state.qrText = PAGE_URL;
    state.step = adminPasswordSet ? SetupStep::Done : SetupStep::WaitingAdmin;
    return state;
}
