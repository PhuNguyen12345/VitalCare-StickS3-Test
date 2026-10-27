#include "services/TimeService.h"

TimeService::TimeService(ITimeProvider& provider, uint32_t intervalMs)
    : provider_(provider), intervalMs_(intervalMs) {}

void TimeService::begin(uint32_t nowMs) {
    provider_.begin();
    provider_.read(time_);
    lastReadMs_ = nowMs;
}

void TimeService::update(uint32_t nowMs) {
    if (nowMs - lastReadMs_ < intervalMs_) {
        return;
    }

    lastReadMs_ = nowMs;
    provider_.read(time_);
}
