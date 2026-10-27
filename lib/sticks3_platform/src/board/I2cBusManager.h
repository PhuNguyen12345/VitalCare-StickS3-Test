#pragma once

#include <Arduino.h>
#include <Wire.h>

// Chuyển bus Wire giữa các cặp chân (Grove RTC <-> Hat2)
class I2cBusManager {
public:
    I2cBusManager(TwoWire& wire, uint32_t frequency, uint16_t timeoutMs);

    // Không khởi tạo lại nếu đã ở đúng bus
    bool select(uint8_t sda, uint8_t scl);

    TwoWire& wire() { return wire_; }

private:
    static constexpr uint8_t NO_PIN = 255;

    TwoWire& wire_;
    uint32_t frequency_;
    uint16_t timeoutMs_;

    uint8_t currentSDA_ = NO_PIN;
    uint8_t currentSCL_ = NO_PIN;
};
