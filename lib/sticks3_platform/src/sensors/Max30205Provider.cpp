#include "sensors/Max30205Provider.h"

Max30205Provider::Max30205Provider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t address)
    : bus_(bus), sda_(sda), scl_(scl), address_(address) {}

bool Max30205Provider::read(HealthReading& out) {
    lastReadOk_ = false;
    out.temperatureC = NAN;
    out.temperatureValid = false;

    if (!bus_.select(sda_, scl_)) {
        return false;
    }

    TwoWire& wire = bus_.wire();

    // Thanh ghi nhiệt độ 0x00
    wire.beginTransmission(address_);
    wire.write(0x00);

    if (wire.endTransmission(false) != 0) {
        return false;
    }

    if (wire.requestFrom(address_, (uint8_t)2) != 2) {
        return false;
    }

    uint8_t msb = wire.read();
    uint8_t lsb = wire.read();

    int16_t raw = (int16_t)((msb << 8) | lsb);

    out.temperatureC = raw / 256.0f;
    out.temperatureValid = true;
    lastReadOk_ = true;

    return true;
}

void Max30205Provider::reportStatus(DeviceStatus& status) const {
    status.maxOk = lastReadOk_;
}
