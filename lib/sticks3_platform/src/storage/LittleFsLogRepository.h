#pragma once

#include <LittleFS.h>

#include "ports/ILogRepository.h"

// Mỗi segment là một file CSV /log/NNNNN.csv
class LittleFsLogRepository : public ILogRepository {
public:
    LittleFsLogRepository(uint32_t segmentMaxBytes, uint32_t reserveBytes);

    bool begin() override;
    bool append(const LogRecord& record) override;
    bool rotate() override;
    bool oldestSegment(uint16_t& id, uint32_t& size) override;
    size_t readSegment(uint16_t id, uint32_t offset, uint8_t* buffer, size_t length) override;
    bool deleteSegment(uint16_t id) override;
    bool clear() override;

    LogStats stats() const override { return stats_; }

private:
    static void segmentPath(uint16_t id, char* out, size_t size);

    bool deleteOldestForSpace();
    void closeReadFile();
    void refreshFreeBytes();

    uint32_t segmentMaxBytes_;
    uint32_t reserveBytes_;

    bool ready_ = false;
    uint16_t oldestId_ = 1;
    uint16_t activeId_ = 1;
    uint32_t activeSize_ = 0;

    // Giữ file đang đọc để không mở lại cho từng chunk khi đồng bộ
    File readFile_;
    uint16_t readId_ = 0;

    LogStats stats_;
};
