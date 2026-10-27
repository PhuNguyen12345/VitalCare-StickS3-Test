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

// Lưu trữ LittleFS: mẫu mỗi giây, mỗi 10 giây ghi một bản ghi trung bình
constexpr uint32_t LOG_SAMPLE_INTERVAL = 1000;
constexpr uint32_t LOG_WINDOW = 10000;
constexpr uint32_t LOG_SEGMENT_MAX_BYTES = 16 * 1024;
constexpr uint32_t LOG_RESERVE_BYTES = 64 * 1024;

// BLE
constexpr uint32_t BLE_LIVE_INTERVAL = 1000;
constexpr uint32_t BLE_STATUS_INTERVAL = 5000;
constexpr uint32_t BLE_SYNC_ACK_TIMEOUT = 10000;

// Pin
constexpr uint32_t BATTERY_INTERVAL = 5000;

// Múi giờ mặc định (phút so với UTC): Việt Nam UTC+7
constexpr int16_t DEFAULT_TZ_MINUTES = 420;

// Nút bấm
constexpr uint32_t BUTTON_HOLD_MS = 800;

// Màn hình 240 x 135 (rotation 1)
constexpr int16_t CANVAS_WIDTH = 240;
constexpr int16_t CANVAS_HEIGHT = 135;
constexpr uint8_t DISPLAY_ROTATION = 1;
constexpr uint8_t DISPLAY_BRIGHTNESS = 100;

}  // namespace AppConfig
