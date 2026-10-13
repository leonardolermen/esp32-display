#include "board/device/Touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "board/AxsTouchReport.h"
#include "board/BoardPins.h"

namespace {

constexpr uint32_t I2C_CLOCK_HZ = 400000;
constexpr uint32_t RESET_PULSE_MILLIS = 200;

// Pedido de leitura do AXS15231B: cabeçalho B5 AB A5 5A e o tamanho da resposta (1 ponto × 6 bytes + 2).
constexpr uint8_t READ_ONE_POINT[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, AxsTouchReport::LENGTH, 0x00, 0x00, 0x00};

}

Touch::Touch(const ScreenRotation& rotation)
    : rotation(rotation)
{
}

lv_indev_t* Touch::begin()
{
    pinMode(pins::TOUCH_RESET, OUTPUT);
    digitalWrite(pins::TOUCH_RESET, LOW);
    delay(RESET_PULSE_MILLIS);
    digitalWrite(pins::TOUCH_RESET, HIGH);
    delay(RESET_PULSE_MILLIS);

    Wire.begin(pins::TOUCH_SDA, pins::TOUCH_SCL);
    Wire.setClock(I2C_CLOCK_HZ);

    lv_indev_t* device = lv_indev_create();
    lv_indev_set_type(device, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(device, read);
    lv_indev_set_user_data(device, this);

    return device;
}

std::optional<PanelPoint> Touch::readPanel()
{
    Wire.beginTransmission(pins::TOUCH_ADDRESS);
    Wire.write(READ_ONE_POINT, sizeof(READ_ONE_POINT));
    if (Wire.endTransmission() != 0) {
        return std::nullopt;
    }

    size_t received = Wire.requestFrom(
        static_cast<uint16_t>(pins::TOUCH_ADDRESS), static_cast<size_t>(AxsTouchReport::LENGTH), true);
    if (received != AxsTouchReport::LENGTH) {
        return std::nullopt;
    }

    uint8_t report[AxsTouchReport::LENGTH] = {};
    Wire.readBytes(report, AxsTouchReport::LENGTH);

    return AxsTouchReport::parse(report);
}

void Touch::read(lv_indev_t* device, lv_indev_data_t* data)
{
    Touch* self = static_cast<Touch*>(lv_indev_get_user_data(device));

    std::optional<PanelPoint> touched = self->readPanel();
    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    ScreenPoint point = self->rotation.toScreen(*touched);
    data->point.x = point.x;
    data->point.y = point.y;
    data->state = LV_INDEV_STATE_PRESSED;
}
