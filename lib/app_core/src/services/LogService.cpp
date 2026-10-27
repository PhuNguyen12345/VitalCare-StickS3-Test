#include "services/LogService.h"

#include <math.h>

LogService::LogService(ILogRepository& repository, uint32_t sampleIntervalMs, uint32_t windowMs)
    : repository_(repository), sampleIntervalMs_(sampleIntervalMs), windowMs_(windowMs) {}

void LogService::begin(uint32_t nowMs) {
    lastSampleMs_ = nowMs;
    windowStartMs_ = nowMs;
    resetWindow();
}

void LogService::update(uint32_t nowMs, const HealthReading& reading, uint32_t epochUtc, bool timeReliable) {
    if (nowMs - lastSampleMs_ >= sampleIntervalMs_) {
        lastSampleMs_ = nowMs;
        addSample(reading);
    }

    if (nowMs - windowStartMs_ >= windowMs_) {
        windowStartMs_ = nowMs;
        writeRecord(nowMs, epochUtc, timeReliable);
        resetWindow();
    }
}

void LogService::addSample(const HealthReading& reading) {
    if (reading.temperatureValid) {
        temperatureSum_ += reading.temperatureC;
        temperatureCount_++;
    }

    if (isBioSampleValid(reading)) {
        heartRateSum_ += reading.heartRate;
        spo2Sum_ += reading.spo2;
        confidenceSum_ += reading.confidence;
        bioCount_++;
    }
}

void LogService::writeRecord(uint32_t nowMs, uint32_t epochUtc, bool timeReliable) {
    LogRecord record;

    // Chưa có giờ thì dùng thời gian chạy (giây) và đánh dấu TIME_INVALID
    record.epoch = epochUtc != 0 ? epochUtc : nowMs / 1000;

    if (!timeReliable) {
        record.flags |= LogFlags::TIME_INVALID;
    }

    if (temperatureCount_ > 0) {
        record.temperatureX100 = (int16_t)lroundf(temperatureSum_ / temperatureCount_ * 100.0f);
    } else {
        record.flags |= LogFlags::TEMP_INVALID;
    }

    if (bioCount_ > 0) {
        record.heartRate = (uint16_t)((heartRateSum_ + bioCount_ / 2) / bioCount_);
        record.spo2 = (uint8_t)((spo2Sum_ + bioCount_ / 2) / bioCount_);
        record.confidence = (uint8_t)((confidenceSum_ + bioCount_ / 2) / bioCount_);
    } else {
        record.flags |= LogFlags::BIO_INVALID;
    }

    if (repository_.append(record)) {
        recordsWritten_++;
    }
}

void LogService::resetWindow() {
    temperatureSum_ = 0;
    temperatureCount_ = 0;
    heartRateSum_ = 0;
    spo2Sum_ = 0;
    confidenceSum_ = 0;
    bioCount_ = 0;
}
