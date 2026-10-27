#pragma once

#include <stdint.h>

#include "domain/HealthReading.h"
#include "domain/MeasurementLog.h"
#include "ports/ILogRepository.h"

// Lấy mẫu mỗi sampleIntervalMs, mỗi windowMs ghi một bản ghi trung bình
class LogService {
public:
    LogService(ILogRepository& repository, uint32_t sampleIntervalMs, uint32_t windowMs);

    void begin(uint32_t nowMs);

    // epochUtc = 0 nếu chưa đọc được giờ; timeReliable = giờ đã được chỉnh
    void update(uint32_t nowMs, const HealthReading& reading, uint32_t epochUtc, bool timeReliable);

    uint32_t recordsWritten() const { return recordsWritten_; }

private:
    void addSample(const HealthReading& reading);
    void writeRecord(uint32_t nowMs, uint32_t epochUtc, bool timeReliable);
    void resetWindow();

    ILogRepository& repository_;
    uint32_t sampleIntervalMs_;
    uint32_t windowMs_;

    uint32_t lastSampleMs_ = 0;
    uint32_t windowStartMs_ = 0;
    uint32_t recordsWritten_ = 0;

    float temperatureSum_ = 0;
    uint16_t temperatureCount_ = 0;

    uint32_t heartRateSum_ = 0;
    uint32_t spo2Sum_ = 0;
    uint32_t confidenceSum_ = 0;
    uint16_t bioCount_ = 0;
};
