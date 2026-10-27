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

    bool addSource(ISensorProvider& provider, uint32_t intervalMs);

    // Khởi tạo các cảm biến theo thứ tự đã thêm, rồi đọc lần đầu
    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);

    const HealthReading& reading() const { return reading_; }
    const DeviceStatus& status() const { return status_; }

private:
    struct Source {
        ISensorProvider* provider = nullptr;
        uint32_t intervalMs = 0;
        uint32_t lastReadMs = 0;
        bool ready = false;
    };

    void readSource(Source& source, uint32_t nowMs);

    Source sources_[MAX_SOURCES];
    size_t sourceCount_ = 0;

    HealthReading reading_;
    DeviceStatus status_;
};
