#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/DeviceStatus.h"
#include "domain/HealthReading.h"
#include "ports/ISensorProvider.h"

// Đọc từng cảm biến theo chu kỳ riêng, giữ số liệu mới nhất
class MeasurementService {
public:
    static constexpr size_t MAX_SOURCES = 4;

    // Cảm biến lỗi liên tiếp thì giãn chu kỳ đọc: mỗi lần đọc lỗi chờ hết timeout I2C,
    // đọc dày đặc sẽ làm chậm cả vòng loop (nút bấm, màn hình)
    static constexpr uint8_t FAILURES_BEFORE_BACKOFF = 3;
    static constexpr uint32_t RETRY_INTERVAL_MS = 10000;

    bool addSource(ISensorProvider& provider, uint32_t intervalMs);

    // Khởi tạo các cảm biến theo thứ tự đã thêm, rồi đọc lần đầu
    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);

    // Chu kỳ đọc hiện tại của nguồn thứ index (để test)
    uint32_t currentIntervalMs(size_t index) const;

    const HealthReading& reading() const { return reading_; }
    const DeviceStatus& status() const { return status_; }

private:
    struct Source {
        ISensorProvider* provider = nullptr;
        uint32_t intervalMs = 0;
        uint32_t lastReadMs = 0;
        uint8_t failures = 0;
        bool ready = false;
    };

    static uint32_t intervalFor(const Source& source);

    void readSource(Source& source, uint32_t nowMs);

    Source sources_[MAX_SOURCES];
    size_t sourceCount_ = 0;

    HealthReading reading_;
    DeviceStatus status_;
};
