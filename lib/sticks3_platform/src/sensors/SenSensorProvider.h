#pragma once

#include <SparkFun_Bio_Sensor_Hub_Library.h>

#include "board/I2cBusManager.h"
#include "ports/ISensorProvider.h"

class SenSensorProvider : public ISensorProvider {
public:
    SenSensorProvider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t resetPin, uint8_t mfioPin);

    bool begin() override;
    bool read(HealthReading& out) override;
    void reportStatus(DeviceStatus& status) const override;

private:
    I2cBusManager& bus_;
    uint8_t sda_;
    uint8_t scl_;
    uint8_t resetPin_;
    uint8_t mfioPin_;

    SparkFun_Bio_Sensor_Hub bioHub_;
    bool initialized_ = false;
};
