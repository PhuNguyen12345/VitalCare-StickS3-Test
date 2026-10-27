#pragma once

#include <stdint.h>

#include "domain/TimeInfo.h"
#include "ports/IConfigRepository.h"
#include "ports/ITimeProvider.h"

// RTC lưu giờ UTC, UI hiển thị giờ địa phương (UTC + múi giờ)
class TimeService {
public:
    // config có thể là nullptr (dùng khi test)
    TimeService(
        ITimeProvider& provider,
        IConfigRepository* config,
        uint32_t intervalMs,
        int16_t defaultTzMinutes
    );

    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);

    // Giờ UTC đọc từ RTC
    const TimeInfo& time() const { return time_; }

    // Giờ địa phương để hiển thị
    TimeInfo localTime() const;

    // 0 nếu chưa đọc được RTC
    uint32_t epochUtc() const;

    int16_t tzMinutes() const { return tzMinutes_; }

    // Chỉnh giờ (từ điện thoại): ghi RTC, lưu múi giờ, đọc lại
    bool setTime(uint32_t epochUtc, int16_t tzMinutes);

private:
    ITimeProvider& provider_;
    IConfigRepository* config_;
    uint32_t intervalMs_;
    int16_t tzMinutes_;
    uint32_t lastReadMs_ = 0;
    TimeInfo time_;
};
