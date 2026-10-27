#include "services/TimeService.h"

TimeService::TimeService(
    ITimeProvider& provider,
    IConfigRepository* config,
    uint32_t intervalMs,
    int16_t defaultTzMinutes
)
    : provider_(provider),
      config_(config),
      intervalMs_(intervalMs),
      tzMinutes_(defaultTzMinutes) {}

void TimeService::begin(uint32_t nowMs) {
    if (config_ != nullptr) {
        tzMinutes_ = config_->tzMinutes(tzMinutes_);
    }

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

TimeInfo TimeService::localTime() const {
    if (!time_.valid) {
        return time_;
    }

    TimeInfo local = fromEpoch(toEpoch(time_) + (int32_t)tzMinutes_ * 60);
    local.valid = time_.valid;
    local.reliable = time_.reliable;

    return local;
}

uint32_t TimeService::epochUtc() const {
    return time_.valid ? toEpoch(time_) : 0;
}

bool TimeService::setTime(uint32_t epochUtc, int16_t tzMinutes) {
    tzMinutes_ = tzMinutes;

    if (config_ != nullptr) {
        config_->setTzMinutes(tzMinutes);
    }

    bool written = provider_.write(fromEpoch(epochUtc));
    provider_.read(time_);

    return written;
}
