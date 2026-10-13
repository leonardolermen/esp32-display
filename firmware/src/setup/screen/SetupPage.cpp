#include "setup/screen/SetupPage.h"

#include "theme/Fonts.h"
#include "theme/Theme.h"

namespace {

constexpr int32_t QR_SIZE = 120;
constexpr const char* STEP_TEXTS[] = {
    "1  escaneie pra conectar no Wi-Fi",
    "2  abra claudinho.local",
    "3  crie a senha de administrador",
};

}

SetupPage::SetupPage(const Published<SetupState>& state)
    : state(state)
{
}

void SetupPage::build(lv_obj_t* parent)
{
    qr = lv_qrcode_create(parent);
    lv_qrcode_set_size(qr, QR_SIZE);
    lv_qrcode_set_dark_color(qr, theme::background());
    lv_qrcode_set_light_color(qr, theme::text());
    lv_obj_set_style_border_width(qr, 6, 0);
    lv_obj_set_style_border_color(qr, theme::text(), 0);
    lv_obj_align(qr, LV_ALIGN_LEFT_MID, 18, -12);

    ipLabel = theme::label(parent, fonts::small, theme::textSecondary());
    lv_obj_align_to(ipLabel, qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t* title = theme::label(parent, fonts::large, theme::text());
    lv_label_set_text(title, "Vamos começar");
    lv_obj_set_pos(title, 156, 60);

    for (size_t position = 0; position < 3; position++) {
        steps[position] = theme::label(parent, fonts::medium, theme::textSecondary());
        lv_label_set_text(steps[position], STEP_TEXTS[position]);
        lv_obj_set_pos(steps[position], 156, 100 + static_cast<int32_t>(position) * 26);
    }
}

void SetupPage::onTick(uint32_t)
{
    SetupState current;
    if (state.readIfChanged(current, seenVersion)) {
        render(current);
    }
}

void SetupPage::render(const SetupState& current)
{
    lv_qrcode_update(qr, current.qrText.c_str(), current.qrText.size());
    lv_label_set_text(ipLabel, current.ip.c_str());

    size_t activeStep = current.step == SetupStep::WaitingWifi ? 0 : 1;
    for (size_t position = 0; position < 3; position++) {
        bool active = position == activeStep;
        lv_obj_set_style_text_color(steps[position], active ? theme::text() : theme::textSecondary(), 0);
    }
}
