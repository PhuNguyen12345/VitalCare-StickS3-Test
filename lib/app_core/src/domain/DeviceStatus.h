#pragma once

#include <stdint.h>

struct DeviceStatus {
    bool senInitialized = false;
    bool maxOk = false;
    bool rtcOk = false;

    uint8_t batteryLevel = 0;

    // BLE và lưu trữ
    bool bleConnected = false;
    bool syncing = false;
    uint16_t pendingSegments = 0;
};
