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

// ======================================================
// Đánh giá mẫu SEN — quy tắc chung cho màn hình, gói live và log CSV
// ======================================================

namespace SampleThresholds {

// confidence của MAX32664 (MODE_ONE) là độ tin cậy của nhịp tim
constexpr uint8_t MIN_CONFIDENCE = 50;

}  // namespace SampleThresholds

struct BioSampleQuality {
    bool fingerDetected = false;  // Đã đọc SEN và phát hiện ngón tay (status 3)
    bool heartRateValid = false;  // Có ngón tay, hr > 0, confidence đạt ngưỡng
    bool spo2Valid = false;       // Nhịp tim hợp lệ và 0 < spo2 <= 100
};

inline BioSampleQuality evaluateBioSample(const HealthReading& reading) {
    BioSampleQuality quality;

    quality.fingerDetected = reading.bioValid && reading.fingerStatus == 3;

    quality.heartRateValid =
        quality.fingerDetected &&
        reading.heartRate > 0 &&
        reading.confidence >= SampleThresholds::MIN_CONFIDENCE;

    // SpO2 tính từ cùng tín hiệu với nhịp tim, và MAX32664 trả 0 khi chưa tính xong
    quality.spo2Valid =
        quality.heartRateValid &&
        reading.spo2 > 0 &&
        reading.spo2 <= 100;

    return quality;
}
