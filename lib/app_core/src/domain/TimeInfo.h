#pragma once

#include <stdint.h>

struct TimeInfo {
    uint16_t year = 2000;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;

    // Đọc được RTC
    bool valid = false;

    // Giờ đáng tin để đóng dấu bản ghi (RTC không mất nguồn, đã được chỉnh giờ)
    bool reliable = false;
};

// ======================================================
// Chuyển đổi epoch (giây từ 1970-01-01 00:00:00)
// Thuật toán days_from_civil / civil_from_days của Howard Hinnant
// ======================================================

inline int32_t daysFromCivil(int32_t year, uint32_t month, uint32_t day) {
    year -= month <= 2;
    const int32_t era = (year >= 0 ? year : year - 399) / 400;
    const uint32_t yearOfEra = (uint32_t)(year - era * 400);
    const uint32_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + (int32_t)dayOfEra - 719468;
}

inline uint32_t toEpoch(const TimeInfo& time) {
    int32_t days = daysFromCivil(time.year, time.month, time.day);
    return (uint32_t)days * 86400u + time.hour * 3600u + time.minute * 60u + time.second;
}

inline TimeInfo fromEpoch(uint32_t epoch) {
    TimeInfo time;

    uint32_t secondsOfDay = epoch % 86400u;
    int32_t days = (int32_t)(epoch / 86400u) + 719468;

    const int32_t era = days / 146097;
    const uint32_t dayOfEra = (uint32_t)(days - era * 146097);
    const uint32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const uint32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const uint32_t monthIndex = (5 * dayOfYear + 2) / 153;
    const uint32_t month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;

    time.year = (uint16_t)((int32_t)yearOfEra + era * 400 + (month <= 2));
    time.month = (uint8_t)month;
    time.day = (uint8_t)(dayOfYear - (153 * monthIndex + 2) / 5 + 1);
    time.hour = (uint8_t)(secondsOfDay / 3600);
    time.minute = (uint8_t)(secondsOfDay % 3600 / 60);
    time.second = (uint8_t)(secondsOfDay % 60);
    time.valid = true;
    time.reliable = true;

    return time;
}

// 0 = Chủ nhật ... 6 = Thứ bảy (1970-01-01 là thứ năm)
inline uint8_t weekdayFromEpoch(uint32_t epoch) {
    return (uint8_t)((epoch / 86400u + 4) % 7);
}
