#pragma once

#include <stdint.h>

struct DeviceStatus {
    // Không đọc được pin. Cùng là giá trị byte `battery` trong gói BLE status
    static constexpr uint8_t BATTERY_UNKNOWN = 0xFF;

    bool senInitialized = false;
    bool maxOk = false;
    bool rtcOk = false;

    uint8_t batteryLevel = BATTERY_UNKNOWN;
    bool charging = false;

    // BLE và lưu trữ
    bool bleConnected = false;
    bool syncing = false;
    uint16_t pendingSegments = 0;
};
