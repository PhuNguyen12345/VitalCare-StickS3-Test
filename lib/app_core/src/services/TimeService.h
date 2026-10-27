#pragma once

#include <stdint.h>

#include "domain/TimeInfo.h"
#include "ports/ITimeProvider.h"

class TimeService {
public:
    TimeService(ITimeProvider& provider, uint32_t intervalMs);

    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);

    const TimeInfo& time() const { return time_; }

private:
    ITimeProvider& provider_;
    uint32_t intervalMs_;
    uint32_t lastReadMs_ = 0;
    TimeInfo time_;
};
