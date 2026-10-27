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

        if (source.ready && nowMs - source.lastReadMs >= source.intervalMs) {
            readSource(source, nowMs);
        }
    }
}

void MeasurementService::readSource(Source& source, uint32_t nowMs) {
    source.lastReadMs = nowMs;
    source.provider->read(reading_);
    source.provider->reportStatus(status_);
}
