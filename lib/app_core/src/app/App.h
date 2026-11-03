#pragma once

#include <stdint.h>

#include "app/AppEvent.h"
#include "app/AppState.h"
#include "app/AppViewModel.h"
#include "services/BodyTemperatureEstimator.h"
#include "services/LogService.h"
#include "services/MeasurementService.h"
#include "services/SyncService.h"
#include "services/TimeService.h"

class App {
public:
    using LogFn = void (*)(const char* message);

    // Service có thể là nullptr (dùng khi test)
    App(
        TimeService* timeService = nullptr,
        MeasurementService* measurementService = nullptr,
        LogService* logService = nullptr,
        SyncService* syncService = nullptr,
        BodyTemperatureEstimator* temperatureEstimator = nullptr
    );

    void setLogger(LogFn logger) { logger_ = logger; }

    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    void handleEvent(AppEvent event);

    void setBatteryLevel(uint8_t level) { batteryLevel_ = level; }

    ScreenPage page() const { return state_.page; }

    // Trả về true một lần sau mỗi lần cần vẽ lại ngay (vd: đổi trang)
    bool consumeRedrawRequest();

    AppViewModel viewModel() const;

private:
    void goToNextMeasurementPage();
    void goToClockPage();
    void log(const char* message) const;
    DeviceStatus deviceStatus() const;
    BodyTemperature bodyTemperature() const;

    TimeService* timeService_;
    MeasurementService* measurementService_;
    LogService* logService_;
    SyncService* syncService_;
    BodyTemperatureEstimator* temperatureEstimator_;
    LogFn logger_ = nullptr;
    uint8_t batteryLevel_ = 0;
    AppState state_;
};
