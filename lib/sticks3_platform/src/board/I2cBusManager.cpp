#include "board/I2cBusManager.h"

I2cBusManager::I2cBusManager(TwoWire& wire, uint32_t frequency, uint16_t timeoutMs)
    : wire_(wire), frequency_(frequency), timeoutMs_(timeoutMs) {}

// Điện trở kéo lên nằm trên board cảm biến (nối 3V3 của cảm biến).
// Kéo xuống nội ~45k yếu hơn kéo lên ngoài ~4.7k: đọc HIGH nghĩa là kéo lên còn hoạt động
static bool hasPullUp(uint8_t pin) {
    pinMode(pin, INPUT_PULLDOWN);
    delay(2);
    bool high = digitalRead(pin) == HIGH;
    pinMode(pin, INPUT);
    return high;
}

I2cBusManager::BusCheck I2cBusManager::check(
    const char* name,
    uint8_t sda,
    uint8_t scl,
    const uint8_t* addresses,
    size_t count
) {
    BusCheck result;

    if (currentSDA_ != NO_PIN) {
        wire_.end();
        currentSDA_ = NO_PIN;
        currentSCL_ = NO_PIN;
    }

    result.sdaPulledUp = hasPullUp(sda);
    result.sclPulledUp = hasPullUp(scl);

    Serial.printf(
        "I2C check %s: SDA G%d pull-up %s, SCL G%d pull-up %s\n",
        name,
        sda,
        result.sdaPulledUp ? "OK" : "MISSING",
        scl,
        result.sclPulledUp ? "OK" : "MISSING"
    );

    if (!select(sda, scl)) {
        return result;
    }

    for (size_t i = 0; i < count; i++) {
        wire_.beginTransmission(addresses[i]);
        bool ack = wire_.endTransmission() == 0;

        if (ack) {
            result.found++;
        }

        Serial.printf(
            "I2C check %s: 0x%02X %s\n",
            name,
            addresses[i],
            ack ? "found" : "NOT FOUND"
        );
    }

    return result;
}

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
