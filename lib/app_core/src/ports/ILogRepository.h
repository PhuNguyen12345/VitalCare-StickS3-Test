#pragma once

#include <stddef.h>
#include <stdint.h>

#include "domain/MeasurementLog.h"

struct LogStats {
    uint16_t segmentCount = 0;  // Kể cả segment đang ghi
    uint32_t totalBytes = 0;
    uint32_t freeBytes = 0;
    bool overflow = false;      // Đã phải xóa dữ liệu cũ chưa đồng bộ vì đầy bộ nhớ
};

// Lưu bản ghi theo segment (file). Segment có id tăng dần;
// segment đang ghi là segment mới nhất, các segment cũ hơn đã đóng.
class ILogRepository {
public:
    virtual ~ILogRepository() = default;

    virtual bool begin() = 0;
    virtual bool append(const LogRecord& record) = 0;

    // Đóng segment đang ghi, bản ghi kế tiếp sẽ vào segment mới
    virtual bool rotate() = 0;

    // Segment đã đóng cũ nhất (không bao giờ trả về segment đang ghi)
    virtual bool oldestSegment(uint16_t& id, uint32_t& size) = 0;

    // Trả về số byte đọc được
    virtual size_t readSegment(uint16_t id, uint32_t offset, uint8_t* buffer, size_t length) = 0;

    virtual bool deleteSegment(uint16_t id) = 0;
    virtual bool clear() = 0;

    virtual LogStats stats() const = 0;
};
