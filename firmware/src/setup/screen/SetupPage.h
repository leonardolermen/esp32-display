#pragma once

#include "navigation/Page.h"
#include "setup/SetupProgress.h"
#include "support/Published.h"

class SetupPage : public Page {
public:
    explicit SetupPage(const Published<SetupState>& state);

    void build(lv_obj_t* parent) override;
    void onTick(uint32_t nowMillis) override;

private:
    void render(const SetupState& state);

    const Published<SetupState>& state;
    uint32_t seenVersion = 0;
    lv_obj_t* qr = nullptr;
    lv_obj_t* ipLabel = nullptr;
    lv_obj_t* steps[3] = {};
};
