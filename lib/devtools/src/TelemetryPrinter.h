#pragma once

#include "BuildConfig.h"

#if VC_ENABLE_DEVTOOLS

#include <Arduino.h>

#include "app/AppViewModel.h"

// In số liệu mỗi chu kỳ: dòng log dễ đọc + dòng @DATA cho simulator
//   @DATA rtc=10:00:00 temp=36.52 hr=78 spo2=98 conf=95 status=3 sen=1 bat=87 ble=1 sync=0 logseg=3
//         page=clock mirror=1
class TelemetryPrinter {
public:
    TelemetryPrinter(Print& out, uint32_t intervalMs);

    void begin(uint32_t nowMs);
    void update(uint32_t nowMs, const AppViewModel& model, bool mirrorEnabled);

private:
    void printReadable(const AppViewModel& model);
    void printData(const AppViewModel& model, bool mirrorEnabled);

    Print& out_;
    uint32_t intervalMs_;
    uint32_t lastPrintMs_ = 0;
};

#endif
