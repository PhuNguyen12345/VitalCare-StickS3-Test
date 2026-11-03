#pragma once

#include <math.h>
#include <stdint.h>

#include "domain/MeasurementLog.h"

// ======================================================
// Nhiệt độ cơ thể ước tính từ nhiệt độ da cổ tay (MAX30205)
// Mốc tham chiếu: tương đương nhiệt độ đo ở miệng.
// Số liệu hiệu chuẩn: docs/temperature_calibration.csv
// ======================================================

namespace TemperatureThresholds {

// Da thấp hơn mức này: coi như không đeo / cảm biến không áp trên da
constexpr float MIN_WORN_SKIN_C = 30.0f;

// Lấy mẫu nhiệt độ da mỗi 10 giây
constexpr uint32_t SAMPLE_INTERVAL_MS = 10000;

// Đã cân bằng nhiệt: độ dốc trong 3 phút gần nhất không quá 0.05 °C/phút
constexpr uint32_t STABLE_WINDOW_MS = 180000;
constexpr float MAX_STABLE_RATE_C_PER_MIN = 0.05f;

// Nhiệt độ da dùng để ước tính: trung bình 1 phút gần nhất
constexpr uint32_t AVERAGE_WINDOW_MS = 60000;

// Độ lệch (miệng - cổ tay) khi chưa hiệu chuẩn, đo ngày 2026-11-03
constexpr float DEFAULT_OFFSET_C = 2.2f;

// Độ lệch hợp lý khi hiệu chuẩn
constexpr float MIN_OFFSET_C = 0.0f;
constexpr float MAX_OFFSET_C = 8.0f;

}  // namespace TemperatureThresholds

enum class BodyTemperatureState : uint8_t {
    Unavailable,  // Không đọc được MAX30205
    NotWorn,      // Da quá lạnh: không đeo hoặc đeo lỏng
    Stabilizing,  // Đang đeo, chưa cân bằng nhiệt
    Stable        // Đã cân bằng: có nhiệt độ cơ thể ước tính
};

inline const char* bodyTemperatureStateName(BodyTemperatureState state) {
    switch (state) {
        case BodyTemperatureState::Unavailable:
            return "unavailable";

        case BodyTemperatureState::NotWorn:
            return "not_worn";

        case BodyTemperatureState::Stabilizing:
            return "stabilizing";

        case BodyTemperatureState::Stable:
            return "stable";
    }

    return "unknown";
}

struct BodyTemperature {
    BodyTemperatureState state = BodyTemperatureState::Unavailable;

    float skinC = NAN;   // Nhiệt độ da mới nhất
    float bodyC = NAN;   // Chỉ có giá trị khi Stable

    float offsetC = TemperatureThresholds::DEFAULT_OFFSET_C;
    bool calibrated = false;  // Độ lệch đã được hiệu chuẩn (không phải giá trị mặc định)

    bool stable() const { return state == BodyTemperatureState::Stable; }
};

// Cờ nhiệt độ dùng chung cho gói live và log CSV
inline uint8_t temperatureFlags(bool skinValid, const BodyTemperature& body) {
    uint8_t flags = 0;

    if (!skinValid) {
        flags |= LogFlags::TEMP_INVALID;
    }

    if (!body.stable()) {
        flags |= LogFlags::TEMP_UNSTABLE;
    }

    if (body.state == BodyTemperatureState::NotWorn) {
        flags |= LogFlags::LOOSE;
    }

    if (!body.calibrated) {
        flags |= LogFlags::TEMP_UNCALIBRATED;
    }

    return flags;
}
