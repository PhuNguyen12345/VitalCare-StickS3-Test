#pragma once

#include "app/AppState.h"
#include "domain/BodyTemperature.h"
#include "domain/DeviceStatus.h"
#include "domain/HealthReading.h"
#include "domain/TimeInfo.h"

// Snapshot dữ liệu để UI vẽ một frame
struct AppViewModel {
    ScreenPage page = ScreenPage::Clock;
    TimeInfo time;
    HealthReading reading;
    BodyTemperature bodyTemperature;
    DeviceStatus status;
};
