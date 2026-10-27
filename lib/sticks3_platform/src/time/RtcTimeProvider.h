#pragma once

#include "board/I2cBusManager.h"
#include "ports/ITimeProvider.h"

// RTC HYM8563 qua cổng Grove
class RtcTimeProvider : public ITimeProvider {
public:
    RtcTimeProvider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t address);

    bool begin() override { return true; }
    bool read(TimeInfo& out) override;

private:
    I2cBusManager& bus_;
    uint8_t sda_;
    uint8_t scl_;
    uint8_t address_;
};
