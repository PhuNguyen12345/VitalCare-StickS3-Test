#pragma once

#include <stdint.h>

// ======================================================
// Chân và địa chỉ I2C của M5StickS3 + ngoại vi
// ======================================================

namespace BoardPins {

// RTC HYM8563 — Grove
constexpr uint8_t RTC_SDA = 9;
constexpr uint8_t RTC_SCL = 10;
constexpr uint8_t RTC_ADDRESS = 0x51;

// Bus Hat2 dùng chung cho MAX30205 và SEN-15219
constexpr uint8_t HAT_SDA = 7;
constexpr uint8_t HAT_SCL = 6;

// MAX30205
constexpr uint8_t MAX_ADDRESS = 0x48;

// SEN-15219 (MAX32664)
constexpr uint8_t SEN_ADDRESS = 0x55;
constexpr uint8_t SEN_MFIO = 8;
constexpr uint8_t SEN_RESET = 1;

}  // namespace BoardPins
