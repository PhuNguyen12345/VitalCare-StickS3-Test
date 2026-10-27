#include "board/I2cBusManager.h"

I2cBusManager::I2cBusManager(TwoWire& wire, uint32_t frequency, uint16_t timeoutMs)
    : wire_(wire), frequency_(frequency), timeoutMs_(timeoutMs) {}

bool I2cBusManager::select(uint8_t sda, uint8_t scl) {
    if (currentSDA_ == sda && currentSCL_ == scl) {
        return true;
    }

    if (currentSDA_ != NO_PIN) {
        wire_.end();
        delay(5);
    }

    bool started = wire_.begin(sda, scl, frequency_);

    if (!started) {
        currentSDA_ = NO_PIN;
        currentSCL_ = NO_PIN;

        Serial.printf(
            "Failed to start I2C: SDA G%d, SCL G%d\n",
            sda,
            scl
        );

        return false;
    }

    wire_.setTimeOut(timeoutMs_);

    currentSDA_ = sda;
    currentSCL_ = scl;

    return true;
}
