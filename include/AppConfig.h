#pragma once

#include <stdint.h>

// ======================================================
// Cấu hình chung của ứng dụng
// ======================================================

namespace AppConfig {

constexpr uint32_t SERIAL_BAUD = 115200;

// I2C
constexpr uint32_t I2C_FREQUENCY = 100000;
constexpr uint16_t I2C_TIMEOUT_MS = 100;

// Chu kỳ đọc / vẽ (ms)
constexpr uint32_t RTC_INTERVAL = 1000;
constexpr uint32_t TEMPERATURE_INTERVAL = 1000;
constexpr uint32_t BIO_INTERVAL = 250;
constexpr uint32_t DISPLAY_INTERVAL = 200;
constexpr uint32_t SERIAL_INTERVAL = 1000;

// Nút bấm
constexpr uint32_t BUTTON_HOLD_MS = 800;

// Màn hình 240 x 135 (rotation 1)
constexpr int16_t CANVAS_WIDTH = 240;
constexpr int16_t CANVAS_HEIGHT = 135;
constexpr uint8_t DISPLAY_ROTATION = 1;
constexpr uint8_t DISPLAY_BRIGHTNESS = 100;

}  // namespace AppConfig
