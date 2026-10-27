#pragma once

#include "board/I2cBusManager.h"
#include "ports/ISensorProvider.h"

class Max30205Provider : public ISensorProvider {
public:
    Max30205Provider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t address);

    bool begin() override { return true; }
    bool read(HealthReading& out) override;
    void reportStatus(DeviceStatus& status) const override;

private:
    I2cBusManager& bus_;
    uint8_t sda_;
    uint8_t scl_;
    uint8_t address_;
    bool lastReadOk_ = false;
};
