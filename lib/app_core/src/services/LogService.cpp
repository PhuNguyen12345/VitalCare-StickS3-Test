#include "services/LogService.h"

#include <math.h>

LogService::LogService(ILogRepository& repository, uint32_t sampleIntervalMs, uint32_t windowMs)
    : repository_(repository), sampleIntervalMs_(sampleIntervalMs), windowMs_(windowMs) {}

void LogService::begin(uint32_t nowMs) {
    lastSampleMs_ = nowMs;
    windowStartMs_ = nowMs;
    resetWindow();
}

void LogService::update(
    uint32_t nowMs,
    const HealthReading& reading,
    const BodyTemperature& body,
    uint32_t epochUtc,
    bool timeReliable
) {
    if (nowMs - lastSampleMs_ >= sampleIntervalMs_) {
        lastSampleMs_ = nowMs;
        addSample(reading);
    }

    if (nowMs - windowStartMs_ >= windowMs_) {
        windowStartMs_ = nowMs;
        writeRecord(nowMs, body, epochUtc, timeReliable);
        resetWindow();
    }
}

void LogService::addSample(const HealthReading& reading) {
    if (reading.temperatureValid) {
        temperatureSum_ += reading.temperatureC;
        temperatureCount_++;
    }

    BioSampleQuality quality = evaluateBioSample(reading);

    if (quality.heartRateValid) {
        heartRateSum_ += reading.heartRate;
        confidenceSum_ += reading.confidence;
        heartRateCount_++;
    }

    if (quality.spo2Valid) {
        spo2Sum_ += reading.spo2;
        spo2Count_++;
    }
}

void LogService::writeRecord(uint32_t nowMs, const BodyTemperature& body, uint32_t epochUtc, bool timeReliable) {
    LogRecord record;

    // Chưa có giờ thì dùng thời gian chạy (giây) và đánh dấu TIME_INVALID
    record.epoch = epochUtc != 0 ? epochUtc : nowMs / 1000;

    if (!timeReliable) {
        record.flags |= LogFlags::TIME_INVALID;
    }

    // Nhiệt độ da: trung bình cửa sổ; nhiệt độ cơ thể: ước tính lúc ghi
    if (temperatureCount_ > 0) {
        record.temperatureX100 = (int16_t)lroundf(temperatureSum_ / temperatureCount_ * 100.0f);
    }

    if (body.hasEstimate()) {
        record.bodyTemperatureX100 = (int16_t)lroundf(body.bodyC * 100.0f);
    }

    record.flags |= temperatureFlags(temperatureCount_ > 0, body);

    if (heartRateCount_ > 0) {
        record.heartRate = (uint16_t)((heartRateSum_ + heartRateCount_ / 2) / heartRateCount_);
        record.confidence = (uint8_t)((confidenceSum_ + heartRateCount_ / 2) / heartRateCount_);
    } else {
        record.flags |= LogFlags::BIO_INVALID;
    }

    if (spo2Count_ > 0) {
        record.spo2 = (uint8_t)((spo2Sum_ + spo2Count_ / 2) / spo2Count_);
    } else {
        record.flags |= LogFlags::SPO2_INVALID;
    }

    if (repository_.append(record)) {
        recordsWritten_++;
    }
}

void LogService::resetWindow() {
    temperatureSum_ = 0;
    temperatureCount_ = 0;
    heartRateSum_ = 0;
    confidenceSum_ = 0;
    heartRateCount_ = 0;
    spo2Sum_ = 0;
    spo2Count_ = 0;
}
