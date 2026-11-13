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

    // Các mẫu trong 1 phút gần nhất
    size_t recentFirst = sampleCount_ - 1;

    while (recentFirst > 0 && newest.ms - samples_[recentFirst - 1].ms <= AVERAGE_WINDOW_MS) {
        recentFirst--;
    }

    float sum = 0;

    for (size_t i = recentFirst; i < sampleCount_; i++) {
        sum += samples_[i].skinC;
    }

    skinAverageC_ = sum / (sampleCount_ - recentFirst);

    uint32_t span = newest.ms - samples_[0].ms;

    // Chưa đủ 1 phút: chưa có số
    if (span < FIRST_ESTIMATE_MS) {
        current_.state = BodyTemperatureState::Measuring;
        skinEstimateC_ = NAN;
        current_.bodyC = NAN;
        return;
    }

    // Đã cân bằng: đủ khoảng 3 phút dữ liệu (trừ 1 chu kỳ lấy mẫu vì lệch thời gian)
    // và độ dốc trên cả cửa sổ đủ nhỏ
    bool fullWindow = span + SAMPLE_INTERVAL_MS >= STABLE_WINDOW_MS;

    if (fullWindow && fabsf(slopeFrom(0)) <= MAX_STABLE_RATE_C_PER_MIN) {
        current_.state = BodyTemperatureState::Stable;
        skinEstimateC_ = skinAverageC_;
        current_.bodyC = skinEstimateC_ + current_.offsetC;
        return;
    }

    // Chưa cân bằng: dự đoán nhiệt độ da khi cân bằng từ độ dốc 1 phút gần nhất.
    // Chỉ bù khi da đang ấm lên, và giới hạn để nhiễu không đẩy số quá xa
    float correction = WARMUP_TIME_CONSTANT_MIN * slopeFrom(recentFirst);

    if (correction < 0) {
        correction = 0;
    } else if (correction > MAX_WARMUP_CORRECTION_C) {
        correction = MAX_WARMUP_CORRECTION_C;
    }

    current_.state = BodyTemperatureState::Stabilizing;
    skinEstimateC_ = skinAverageC_ + correction;
    current_.bodyC = skinEstimateC_ + current_.offsetC;
}

float BodyTemperatureEstimator::slopeFrom(size_t first) const {
    size_t count = sampleCount_ - first;

    if (count < 2) {
        return 0;
    }

    // Hồi quy tuyến tính, ít bị nhiễu hơn so với lấy đầu - cuối
    float meanX = 0;
    float meanY = 0;

    for (size_t i = first; i < sampleCount_; i++) {
        meanX += (samples_[i].ms - samples_[first].ms) / 60000.0f;
        meanY += samples_[i].skinC;
    }

    meanX /= count;
    meanY /= count;

    float covariance = 0;
    float variance = 0;

    for (size_t i = first; i < sampleCount_; i++) {
        float dx = (samples_[i].ms - samples_[first].ms) / 60000.0f - meanX;
        covariance += dx * (samples_[i].skinC - meanY);
        variance += dx * dx;
    }

    return variance > 0 ? covariance / variance : 0;
}

void BodyTemperatureEstimator::resetWindow() {
    sampleCount_ = 0;
    skinAverageC_ = NAN;
    skinEstimateC_ = NAN;
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

    if (current_.hasEstimate()) {
        current_.bodyC = skinEstimateC_ + offsetC;
    }

    return true;
}

void BodyTemperatureEstimator::resetOffset() {
    current_.offsetC = DEFAULT_OFFSET_C;
    current_.calibrated = false;

    if (config_ != nullptr) {
        config_->clearTempOffset();
    }

    if (current_.hasEstimate()) {
        current_.bodyC = skinEstimateC_ + DEFAULT_OFFSET_C;
    }
}
