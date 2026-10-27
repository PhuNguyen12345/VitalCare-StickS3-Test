#pragma once

#include "board/I2cBusManager.h"
#include "ports/ITimeProvider.h"

// RTC HYM8563 qua cổng Grove, lưu giờ UTC
class RtcTimeProvider : public ITimeProvider {
public:
    RtcTimeProvider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t address);

    bool begin() override { return true; }
    bool read(TimeInfo& out) override;
    bool write(const TimeInfo& time) override;

private:
    // Giờ trước năm này coi như chưa được chỉnh
    static constexpr uint16_t MIN_RELIABLE_YEAR = 2024;

    I2cBusManager& bus_;
    uint8_t sda_;
    uint8_t scl_;
    uint8_t address_;
};
