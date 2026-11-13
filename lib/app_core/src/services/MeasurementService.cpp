#include "services/MeasurementService.h"

bool MeasurementService::addSource(ISensorProvider& provider, uint32_t intervalMs) {
    if (sourceCount_ >= MAX_SOURCES) {
        return false;
    }

    Source& source = sources_[sourceCount_++];
    source.provider = &provider;
    source.intervalMs = intervalMs;

    return true;
}

void MeasurementService::begin(uint32_t nowMs) {
    for (size_t i = 0; i < sourceCount_; i++) {
        sources_[i].ready = sources_[i].provider->begin();
        sources_[i].provider->reportStatus(status_);
    }

    for (size_t i = 0; i < sourceCount_; i++) {
        if (sources_[i].ready) {
            readSource(sources_[i], nowMs);
        }
    }
}

void MeasurementService::update(uint32_t nowMs) {
    for (size_t i = 0; i < sourceCount_; i++) {
        Source& source = sources_[i];

        if (source.ready && nowMs - source.lastReadMs >= intervalFor(source)) {
            readSource(source, nowMs);
        }
    }
}

uint32_t MeasurementService::currentIntervalMs(size_t index) const {
    return index < sourceCount_ ? intervalFor(sources_[index]) : 0;
}

uint32_t MeasurementService::intervalFor(const Source& source) {
    if (source.failures >= FAILURES_BEFORE_BACKOFF && source.intervalMs < RETRY_INTERVAL_MS) {
        return RETRY_INTERVAL_MS;
    }

    return source.intervalMs;
}

void MeasurementService::readSource(Source& source, uint32_t nowMs) {
    source.lastReadMs = nowMs;

    if (source.provider->read(reading_)) {
        source.failures = 0;
    } else if (source.failures < UINT8_MAX) {
        source.failures++;
    }

    source.provider->reportStatus(status_);
}
