#pragma once

#include <stdint.h>

#include "app/AppEvent.h"
#include "app/AppState.h"
#include "app/AppViewModel.h"
#include "services/MeasurementService.h"
#include "services/TimeService.h"

class App {
public:
    using LogFn = void (*)(const char* message);

    // Service có thể là nullptr (dùng khi test)
    App(TimeService* timeService, MeasurementService* measurementService);

    void setLogger(LogFn logger) { logger_ = logger; }

    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);
    void handleEvent(AppEvent event);

    ScreenPage page() const { return state_.page; }

    // Trả về true một lần sau mỗi lần cần vẽ lại ngay (vd: đổi trang)
    bool consumeRedrawRequest();

    AppViewModel viewModel() const;

private:
    void goToNextMeasurementPage();
    void goToClockPage();
    void log(const char* message) const;

    TimeService* timeService_;
    MeasurementService* measurementService_;
    LogFn logger_ = nullptr;
    AppState state_;
};
