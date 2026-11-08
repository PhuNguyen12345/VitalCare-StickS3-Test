#include "app/App.h"

#include <stdio.h>

App::App(
    TimeService* timeService,
    MeasurementService* measurementService,
    LogService* logService,
    SyncService* syncService,
    BodyTemperatureEstimator* temperatureEstimator
)
    : timeService_(timeService),
      measurementService_(measurementService),
      logService_(logService),
      syncService_(syncService),
      temperatureEstimator_(temperatureEstimator) {}

void App::begin(uint32_t nowMs) {
    // Khởi tạo SEN trước (theo thứ tự đã thêm vào MeasurementService), rồi tới RTC
    if (measurementService_ != nullptr) {
        measurementService_->begin(nowMs);
    }

    if (timeService_ != nullptr) {
        timeService_->begin(nowMs);
    }

    if (temperatureEstimator_ != nullptr) {
        temperatureEstimator_->begin(nowMs);
    }

    if (logService_ != nullptr) {
        logService_->begin(nowMs);
    }

    state_.page = ScreenPage::Clock;
    state_.redrawRequested = true;
}

void App::update(uint32_t nowMs) {
    if (timeService_ != nullptr) {
        timeService_->update(nowMs);
    }

    if (measurementService_ != nullptr) {
        measurementService_->update(nowMs);
    }

    HealthReading reading;

    if (measurementService_ != nullptr) {
        reading = measurementService_->reading();
    }

    if (temperatureEstimator_ != nullptr) {
        temperatureEstimator_->update(nowMs, reading);
    }

    if (logService_ != nullptr && timeService_ != nullptr) {
        logService_->update(
            nowMs,
            reading,
            bodyTemperature(),
            timeService_->epochUtc(),
            timeService_->time().reliable
        );
    }

    if (syncService_ != nullptr) {
        syncService_->update(nowMs, reading, deviceStatus());
    }
}

void App::handleEvent(AppEvent event) {
    char message[32];
    snprintf(message, sizeof(message), "Event: %s", appEventName(event));
    log(message);

    switch (event) {
        // Button A: chuyển giữa ba chế độ đo
        case AppEvent::BtnAClick:
            goToNextMeasurementPage();
            break;

        // Button B: shortcut quay về đồng hồ
        case AppEvent::BtnBClick:
            goToClockPage();
            break;

        // Giữ nút: dành cho chức năng sau (vd: đồng bộ BLE)
        case AppEvent::BtnAHold:
        case AppEvent::BtnBHold:
            break;
    }
}

bool App::consumeRedrawRequest() {
    bool requested = state_.redrawRequested;
    state_.redrawRequested = false;
    return requested;
}

AppViewModel App::viewModel() const {
    AppViewModel model;
    model.page = state_.page;

    if (timeService_ != nullptr) {
        model.time = timeService_->localTime();
    }

    if (measurementService_ != nullptr) {
        model.reading = measurementService_->reading();
    }

    model.status = deviceStatus();
    model.bodyTemperature = bodyTemperature();

    return model;
}

DeviceStatus App::deviceStatus() const {
    DeviceStatus status;

    if (measurementService_ != nullptr) {
        status = measurementService_->status();
    }

    if (timeService_ != nullptr) {
        status.rtcOk = timeService_->time().valid;
    }

    status.batteryLevel = batteryLevel_;
    status.charging = charging_;

    if (syncService_ != nullptr) {
        status.bleConnected = syncService_->connected();
        status.syncing = syncService_->syncing();
        status.pendingSegments = syncService_->logStats().segmentCount;
    }

    return status;
}

void App::goToNextMeasurementPage() {
    switch (state_.page) {
        case ScreenPage::Clock:
            state_.page = ScreenPage::Temperature;
            break;

        case ScreenPage::Temperature:
            state_.page = ScreenPage::HeartRate;
            break;

        case ScreenPage::HeartRate:
            state_.page = ScreenPage::SpO2;
            break;

        case ScreenPage::SpO2:
            state_.page = ScreenPage::Temperature;
            break;
    }

    state_.redrawRequested = true;
}

void App::goToClockPage() {
    state_.page = ScreenPage::Clock;
    state_.redrawRequested = true;
}

void App::log(const char* message) const {
    if (logger_ != nullptr) {
        logger_(message);
    }
}

BodyTemperature App::bodyTemperature() const {
    return temperatureEstimator_ != nullptr ? temperatureEstimator_->current() : BodyTemperature();
}
