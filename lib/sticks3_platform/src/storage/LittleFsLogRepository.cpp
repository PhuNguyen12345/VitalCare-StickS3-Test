#include "storage/LittleFsLogRepository.h"

#include <stdio.h>
#include <stdlib.h>

namespace {

constexpr const char* LOG_DIR = "/log";

}  // namespace

LittleFsLogRepository::LittleFsLogRepository(uint32_t segmentMaxBytes, uint32_t reserveBytes)
    : segmentMaxBytes_(segmentMaxBytes), reserveBytes_(reserveBytes) {}

void LittleFsLogRepository::segmentPath(uint16_t id, char* out, size_t size) {
    snprintf(out, size, "%s/%05u.csv", LOG_DIR, id);
}

bool LittleFsLogRepository::begin() {
    // Partition "spiffs" (1.5MB); format nếu chưa có filesystem
    ready_ = LittleFS.begin(true);

    if (!ready_) {
        Serial.println("LittleFS mount FAILED");
        return false;
    }

    if (!LittleFS.exists(LOG_DIR)) {
        LittleFS.mkdir(LOG_DIR);
    }

    uint16_t minId = UINT16_MAX;
    uint16_t maxId = 0;

    stats_ = LogStats();

    File dir = LittleFS.open(LOG_DIR);
    File file = dir.openNextFile();

    while (file) {
        uint16_t id = (uint16_t)atoi(file.name());

        if (id > 0) {
            stats_.segmentCount++;
            stats_.totalBytes += file.size();

            if (id < minId) {
                minId = id;
            }

            if (id > maxId) {
                maxId = id;
            }
        }

        file.close();
        file = dir.openNextFile();
    }

    dir.close();

    if (stats_.segmentCount == 0) {
        oldestId_ = 1;
        activeId_ = 1;
        activeSize_ = 0;
    } else {
        // Mỗi lần khởi động ghi sang segment mới: không ghi tiếp vào file của firmware cũ
        // (có thể khác định dạng), segment cũ được coi là đã đóng và sẵn sàng đồng bộ
        oldestId_ = minId;
        activeId_ = maxId + 1;
        activeSize_ = 0;
    }

    refreshFreeBytes();

    Serial.printf(
        "LittleFS: %u segment, %lu bytes, free %lu bytes\n",
        stats_.segmentCount,
        (unsigned long)stats_.totalBytes,
        (unsigned long)stats_.freeBytes
    );

    return true;
}

bool LittleFsLogRepository::append(const LogRecord& record) {
    if (!ready_) {
        return false;
    }

    if (activeSize_ >= segmentMaxBytes_) {
        rotate();
    }

    // Gần đầy: xóa segment cũ nhất (dữ liệu chưa đồng bộ sẽ mất)
    while (stats_.freeBytes < reserveBytes_ && deleteOldestForSpace()) {
        stats_.overflow = true;
    }

    char line[48];
    size_t lineLength = formatCsvLine(record, line, sizeof(line));

    char path[24];
    segmentPath(activeId_, path, sizeof(path));

    File file = LittleFS.open(path, FILE_APPEND);

    if (!file) {
        return false;
    }

    size_t written = 0;

    if (activeSize_ == 0) {
        written += file.print(LOG_CSV_HEADER);
        stats_.segmentCount++;
    }

    written += file.write((const uint8_t*)line, lineLength);
    file.close();

    activeSize_ += written;
    stats_.totalBytes += written;
    refreshFreeBytes();

    return written > 0;
}

bool LittleFsLogRepository::rotate() {
    if (activeSize_ > 0) {
        activeId_++;
        activeSize_ = 0;
    }

    return true;
}

bool LittleFsLogRepository::oldestSegment(uint16_t& id, uint32_t& size) {
    if (!ready_) {
        return false;
    }

    char path[24];

    for (uint16_t candidate = oldestId_; candidate < activeId_; candidate++) {
        segmentPath(candidate, path, sizeof(path));

        if (!LittleFS.exists(path)) {
            continue;
        }

        File file = LittleFS.open(path, FILE_READ);

        if (!file) {
            continue;
        }

        oldestId_ = candidate;
        id = candidate;
        size = file.size();
        file.close();

        return true;
    }

    oldestId_ = activeId_;
    return false;
}

size_t LittleFsLogRepository::readSegment(uint16_t id, uint32_t offset, uint8_t* buffer, size_t length) {
    if (!ready_) {
        return 0;
    }

    if (!readFile_ || readId_ != id) {
        closeReadFile();

        char path[24];
        segmentPath(id, path, sizeof(path));

        readFile_ = LittleFS.open(path, FILE_READ);
        readId_ = id;

        if (!readFile_) {
            return 0;
        }
    }

    if (!readFile_.seek(offset)) {
        return 0;
    }

    return readFile_.read(buffer, length);
}

bool LittleFsLogRepository::deleteSegment(uint16_t id) {
    if (!ready_) {
        return false;
    }

    if (readId_ == id) {
        closeReadFile();
    }

    char path[24];
    segmentPath(id, path, sizeof(path));

    File file = LittleFS.open(path, FILE_READ);

    if (!file) {
        return false;
    }

    uint32_t size = file.size();
    file.close();

    if (!LittleFS.remove(path)) {
        return false;
    }

    stats_.segmentCount--;
    stats_.totalBytes -= min(size, stats_.totalBytes);

    if (id == activeId_) {
        activeSize_ = 0;
    }

    // Đã đồng bộ hết thì xóa cờ tràn bộ nhớ
    if (stats_.segmentCount == 0) {
        stats_.overflow = false;
    }

    refreshFreeBytes();

    return true;
}

bool LittleFsLogRepository::clear() {
    if (!ready_) {
        return false;
    }

    closeReadFile();

    File dir = LittleFS.open(LOG_DIR);
    File file = dir.openNextFile();
    char path[40];

    while (file) {
        snprintf(path, sizeof(path), "%s/%s", LOG_DIR, file.name());
        file.close();
        LittleFS.remove(path);
        file = dir.openNextFile();
    }

    dir.close();

    stats_ = LogStats();
    oldestId_ = activeId_ + 1;
    activeId_ = oldestId_;
    activeSize_ = 0;
    refreshFreeBytes();

    return true;
}

bool LittleFsLogRepository::deleteOldestForSpace() {
    uint16_t id;
    uint32_t size;

    return oldestSegment(id, size) && deleteSegment(id);
}

void LittleFsLogRepository::closeReadFile() {
    if (readFile_) {
        readFile_.close();
    }

    readId_ = 0;
}

void LittleFsLogRepository::refreshFreeBytes() {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();

    stats_.freeBytes = total > used ? total - used : 0;
}
