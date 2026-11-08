#pragma once

#include <M5GFX.h>

// Chuẩn hóa cỡ chữ cho màn hình 240 x 135
namespace Theme {

constexpr float FONT_SMALL = 1.5f;
constexpr float FONT_MESSAGE = 2.0f;
constexpr float FONT_VALUE = 3.0f;

constexpr uint16_t BACKGROUND = TFT_BLACK;
constexpr uint16_t FOOTER = TFT_DARKGREY;

// Header: pin và Bluetooth
constexpr uint8_t BATTERY_LOW = 20;
constexpr uint8_t BATTERY_CRITICAL = 10;
constexpr uint16_t BLE_CONNECTED = 0x4D9F;
constexpr uint16_t BLE_SYNCING = TFT_ORANGE;

}  // namespace Theme
