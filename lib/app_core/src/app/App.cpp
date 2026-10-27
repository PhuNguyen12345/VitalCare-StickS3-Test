#include "app/App.h"

#include <stdio.h>

App::App(TimeService* timeService, MeasurementService* measurementService)
    : timeService_(timeService), measurementService_(measurementService) {}

void App::begin(uint32_t nowMs) {
    // Khởi tạo SEN trước (theo thứ tự đã thêm vào MeasurementService), rồi tới RTC
    if (measurementService_ != nullptr) {
        measurementService_->begin(nowMs);
    }

    if (timeService_ != nullptr) {
        timeService_->begin(nowMs);
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
        model.time = timeService_->time();
    }

    if (measurementService_ != nullptr) {
        model.reading = measurementService_->reading();
        model.status = measurementService_->status();
    }

    model.status.rtcOk = model.time.valid;

    return model;
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
