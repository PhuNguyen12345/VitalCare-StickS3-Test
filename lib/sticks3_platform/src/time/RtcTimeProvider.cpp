#include "time/RtcTimeProvider.h"

namespace {

uint8_t bcdToDecimal(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
}

}  // namespace

RtcTimeProvider::RtcTimeProvider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t address)
    : bus_(bus), sda_(sda), scl_(scl), address_(address) {}

bool RtcTimeProvider::read(TimeInfo& out) {
    out.valid = false;

    if (!bus_.select(sda_, scl_)) {
        return false;
    }

    TwoWire& wire = bus_.wire();

    // Thanh ghi thời gian bắt đầu từ 0x02
    wire.beginTransmission(address_);
    wire.write(0x02);

    if (wire.endTransmission(false) != 0) {
        return false;
    }

    if (wire.requestFrom(address_, (uint8_t)3) != 3) {
        return false;
    }

    out.second = bcdToDecimal(wire.read() & 0x7F);
    out.minute = bcdToDecimal(wire.read() & 0x7F);
    out.hour = bcdToDecimal(wire.read() & 0x3F);
    out.valid = true;

    return true;
}
