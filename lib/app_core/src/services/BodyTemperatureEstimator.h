#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/BodyTemperature.h"
#include "domain/HealthReading.h"
#include "ports/IConfigRepository.h"

// Ước tính nhiệt độ cơ thể = trung bình da 1 phút + độ lệch,
// chỉ khi đang đeo và đã cân bằng nhiệt (xem TemperatureThresholds)
class BodyTemperatureEstimator {
public:
    // config có thể là nullptr (dùng khi test): độ lệch không được lưu bền
    explicit BodyTemperatureEstimator(IConfigRepository* config);

    // Đọc độ lệch đã hiệu chuẩn (nếu có)
    void begin(uint32_t nowMs);
    void update(uint32_t nowMs, const HealthReading& reading);

    const BodyTemperature& current() const { return current_; }

    // Hiệu chuẩn bằng nhiệt độ tham chiếu (nhiệt kế đo miệng).
    // Chỉ thành công khi đã cân bằng nhiệt và độ lệch hợp lý.
    bool calibrate(float referenceC);

    bool setOffset(float offsetC);
    void resetOffset();

private:
    struct Sample {
        uint32_t ms;
        float skinC;
    };

    static constexpr size_t MAX_SAMPLES = 32;

    void addSample(uint32_t nowMs, float skinC);
    void evaluate();
    void resetWindow();

    IConfigRepository* config_;
    BodyTemperature current_;

    Sample samples_[MAX_SAMPLES];
    size_t sampleCount_ = 0;

    uint32_t lastSampleMs_ = 0;
    bool sampledOnce_ = false;

    // Trung bình da 1 phút gần nhất (dùng cho cả ước tính và hiệu chuẩn)
    float skinAverageC_ = NAN;
};
