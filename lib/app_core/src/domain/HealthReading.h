#pragma once

#include <math.h>
#include <stdint.h>

// Số liệu mới nhất từ các cảm biến.
// Mỗi provider chỉ ghi vào phần dữ liệu của mình.
struct HealthReading {
    // MAX30205
    float temperatureC = NAN;
    bool temperatureValid = false;

    // SEN-15219
    uint16_t heartRate = 0;
    uint16_t spo2 = 0;
    uint8_t confidence = 0;
    uint8_t fingerStatus = 0;
    bool bioValid = false;
};

// Mẫu SEN dùng được: đã đọc, phát hiện ngón tay (status 3), có nhịp tim
inline bool isBioSampleValid(const HealthReading& reading) {
    return reading.bioValid && reading.fingerStatus == 3 && reading.heartRate > 0;
}
