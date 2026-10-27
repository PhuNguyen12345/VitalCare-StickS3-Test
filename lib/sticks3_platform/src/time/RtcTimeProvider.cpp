#include "time/RtcTimeProvider.h"

namespace {

// Thanh ghi 0x02..0x08: giây, phút, giờ, ngày, thứ, tháng, năm (BCD)
constexpr uint8_t TIME_REGISTER = 0x02;
constexpr uint8_t TIME_REGISTER_COUNT = 7;

// Bit 7 thanh ghi giây: RTC từng mất nguồn, giờ không còn đúng
constexpr uint8_t VOLTAGE_LOW_BIT = 0x80;

uint8_t bcdToDecimal(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t decimalToBcd(uint8_t value) {
    return ((value / 10) << 4) | (value % 10);
}

}  // namespace

RtcTimeProvider::RtcTimeProvider(I2cBusManager& bus, uint8_t sda, uint8_t scl, uint8_t address)
    : bus_(bus), sda_(sda), scl_(scl), address_(address) {}

bool RtcTimeProvider::read(TimeInfo& out) {
    out.valid = false;
    out.reliable = false;

    if (!bus_.select(sda_, scl_)) {
        return false;
    }

    TwoWire& wire = bus_.wire();

    wire.beginTransmission(address_);
    wire.write(TIME_REGISTER);

    if (wire.endTransmission(false) != 0) {
        return false;
    }

    if (wire.requestFrom(address_, TIME_REGISTER_COUNT) != TIME_REGISTER_COUNT) {
        return false;
    }

    uint8_t secondRaw = wire.read();

    out.second = bcdToDecimal(secondRaw & 0x7F);
    out.minute = bcdToDecimal(wire.read() & 0x7F);
    out.hour = bcdToDecimal(wire.read() & 0x3F);
    out.day = bcdToDecimal(wire.read() & 0x3F);
    wire.read();  // Thứ trong tuần: tính lại từ ngày khi cần
    out.month = bcdToDecimal(wire.read() & 0x1F);
    out.year = 2000 + bcdToDecimal(wire.read());

    out.valid = true;
    out.reliable = (secondRaw & VOLTAGE_LOW_BIT) == 0 && out.year >= MIN_RELIABLE_YEAR;

    return true;
}

bool RtcTimeProvider::write(const TimeInfo& time) {
    if (!bus_.select(sda_, scl_) || time.year < 2000 || time.year > 2099) {
        return false;
    }

    TwoWire& wire = bus_.wire();

    wire.beginTransmission(address_);
    wire.write(TIME_REGISTER);
    wire.write(decimalToBcd(time.second));  // Ghi giây cũng xóa bit VL
    wire.write(decimalToBcd(time.minute));
    wire.write(decimalToBcd(time.hour));
    wire.write(decimalToBcd(time.day));
    wire.write(weekdayFromEpoch(toEpoch(time)));
    wire.write(decimalToBcd(time.month));
    wire.write(decimalToBcd((uint8_t)(time.year - 2000)));

    return wire.endTransmission() == 0;
}
