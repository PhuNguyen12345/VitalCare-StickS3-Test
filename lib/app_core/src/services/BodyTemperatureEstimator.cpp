#include "services/BodyTemperatureEstimator.h"

#include <math.h>

using namespace TemperatureThresholds;

BodyTemperatureEstimator::BodyTemperatureEstimator(IConfigRepository* config)
    : config_(config) {}

void BodyTemperatureEstimator::begin(uint32_t nowMs) {
    int16_t offsetX100;

    if (config_ != nullptr && config_->tempOffsetX100(offsetX100)) {
        current_.offsetC = offsetX100 / 100.0f;
        current_.calibrated = true;
    }

    lastSampleMs_ = nowMs;
    sampledOnce_ = false;
}

void BodyTemperatureEstimator::update(uint32_t nowMs, const HealthReading& reading) {
    if (sampledOnce_ && nowMs - lastSampleMs_ < SAMPLE_INTERVAL_MS) {
        return;
    }

    lastSampleMs_ = nowMs;
    sampledOnce_ = true;

    if (!reading.temperatureValid) {
        resetWindow();
        current_.state = BodyTemperatureState::Unavailable;
        current_.skinC = NAN;
        return;
    }

    current_.skinC = reading.temperatureC;

    // Không đeo: bắt đầu lại từ đầu khi đeo lên
    if (reading.temperatureC < MIN_WORN_SKIN_C) {
        resetWindow();
        current_.state = BodyTemperatureState::NotWorn;
        return;
    }

    addSample(nowMs, reading.temperatureC);
    evaluate();
}

void BodyTemperatureEstimator::addSample(uint32_t nowMs, float skinC) {
    // Bỏ mẫu cũ hơn cửa sổ cân bằng
    size_t keep = 0;

    for (size_t i = 0; i < sampleCount_; i++) {
        if (nowMs - samples_[i].ms <= STABLE_WINDOW_MS) {
            samples_[keep++] = samples_[i];
        }
    }

    sampleCount_ = keep;

    if (sampleCount_ == MAX_SAMPLES) {
        for (size_t i = 1; i < sampleCount_; i++) {
            samples_[i - 1] = samples_[i];
        }
        sampleCount_--;
    }

    samples_[sampleCount_++] = {nowMs, skinC};
}

void BodyTemperatureEstimator::evaluate() {
    const Sample& newest = samples_[sampleCount_ - 1];

    // Trung bình 1 phút gần nhất
    float sum = 0;
    size_t count = 0;

    for (size_t i = 0; i < sampleCount_; i++) {
        if (newest.ms - samples_[i].ms <= AVERAGE_WINDOW_MS) {
            sum += samples_[i].skinC;
            count++;
        }
    }

    skinAverageC_ = sum / count;

    // Cần đủ khoảng 3 phút dữ liệu liên tục (trừ 1 chu kỳ lấy mẫu vì lệch thời gian)
    uint32_t span = newest.ms - samples_[0].ms;

    if (span + SAMPLE_INTERVAL_MS < STABLE_WINDOW_MS) {
        current_.state = BodyTemperatureState::Stabilizing;
        current_.bodyC = NAN;
        return;
    }

    // Độ dốc (°C/phút) bằng hồi quy tuyến tính, ít bị nhiễu hơn so với lấy đầu - cuối
    float meanX = 0;
    float meanY = 0;

    for (size_t i = 0; i < sampleCount_; i++) {
        meanX += (samples_[i].ms - samples_[0].ms) / 60000.0f;
        meanY += samples_[i].skinC;
    }

    meanX /= sampleCount_;
    meanY /= sampleCount_;

    float covariance = 0;
    float variance = 0;

    for (size_t i = 0; i < sampleCount_; i++) {
        float dx = (samples_[i].ms - samples_[0].ms) / 60000.0f - meanX;
        covariance += dx * (samples_[i].skinC - meanY);
        variance += dx * dx;
    }

    float slope = variance > 0 ? covariance / variance : 0;

    if (fabsf(slope) > MAX_STABLE_RATE_C_PER_MIN) {
        current_.state = BodyTemperatureState::Stabilizing;
        current_.bodyC = NAN;
        return;
    }

    current_.state = BodyTemperatureState::Stable;
    current_.bodyC = skinAverageC_ + current_.offsetC;
}

void BodyTemperatureEstimator::resetWindow() {
    sampleCount_ = 0;
    skinAverageC_ = NAN;
    current_.bodyC = NAN;
}

bool BodyTemperatureEstimator::calibrate(float referenceC) {
    if (!current_.stable()) {
        return false;
    }

    return setOffset(referenceC - skinAverageC_);
}

bool BodyTemperatureEstimator::setOffset(float offsetC) {
    if (offsetC < MIN_OFFSET_C || offsetC > MAX_OFFSET_C) {
        return false;
    }

    current_.offsetC = offsetC;
    current_.calibrated = true;

    if (config_ != nullptr) {
        config_->setTempOffsetX100((int16_t)lroundf(offsetC * 100.0f));
    }

    if (current_.stable()) {
        current_.bodyC = skinAverageC_ + offsetC;
    }

    return true;
}

void BodyTemperatureEstimator::resetOffset() {
    current_.offsetC = DEFAULT_OFFSET_C;
    current_.calibrated = false;

    if (config_ != nullptr) {
        config_->clearTempOffset();
    }

    if (current_.stable()) {
        current_.bodyC = skinAverageC_ + DEFAULT_OFFSET_C;
    }
}
