#pragma once

#include <stddef.h>
#include <stdint.h>

// Bit trạng thái của bản ghi log và gói live
namespace LogFlags {

constexpr uint8_t TIME_INVALID = 0x01;
constexpr uint8_t TEMP_INVALID = 0x02;
constexpr uint8_t BIO_INVALID = 0x04;  // Không có ngón tay / không có mẫu hợp lệ
constexpr uint8_t LOOSE = 0x08;        // Dành cho cảnh báo đeo lỏng
constexpr uint8_t ALERT = 0x10;        // Dành cho cảnh báo vượt ngưỡng

}  // namespace LogFlags

// Một bản ghi trong LittleFS (trung bình của một cửa sổ đo)
struct LogRecord {
    uint32_t epoch = 0;
    int16_t temperatureX100 = 0;
    uint16_t heartRate = 0;
    uint8_t spo2 = 0;
    uint8_t confidence = 0;
    uint8_t flags = 0;
};

// Dòng đầu của mỗi file segment
constexpr const char* LOG_CSV_HEADER = "#v1,ts,t100,hr,spo2,conf,flags\n";

// Ghi "ts,t100,hr,spo2,conf,flags\n" vào buffer, trả về số byte (0 nếu buffer quá nhỏ)
size_t formatCsvLine(const LogRecord& record, char* buffer, size_t size);

// Đọc một dòng CSV (bỏ qua dòng header bắt đầu bằng '#')
bool parseCsvLine(const char* line, LogRecord& out);
