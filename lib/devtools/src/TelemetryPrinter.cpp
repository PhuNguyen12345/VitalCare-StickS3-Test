#include "TelemetryPrinter.h"

#if VC_ENABLE_DEVTOOLS

TelemetryPrinter::TelemetryPrinter(Print& out, uint32_t intervalMs)
    : out_(out), intervalMs_(intervalMs) {}

void TelemetryPrinter::begin(uint32_t nowMs) {
    lastPrintMs_ = nowMs;
}

void TelemetryPrinter::update(uint32_t nowMs, const AppViewModel& model, bool mirrorEnabled) {
    if (nowMs - lastPrintMs_ < intervalMs_) {
        return;
    }

    lastPrintMs_ = nowMs;

    printReadable(model);
    printData(model, mirrorEnabled);
}

void TelemetryPrinter::printReadable(const AppViewModel& model) {
    out_.print("RTC: ");

    if (model.time.valid) {
        out_.printf(
            "%02d:%02d:%02d",
            model.time.hour,
            model.time.minute,
            model.time.second
        );
    } else {
        out_.print("ERROR");
    }

    out_.print(" | Temp: ");

    if (model.reading.temperatureValid) {
        out_.print(model.reading.temperatureC, 2);
        out_.print(" C");
    } else {
        out_.print("ERROR");
    }

    out_.print(" | Body: ");

    if (model.bodyTemperature.stable()) {
        out_.print(model.bodyTemperature.bodyC, 2);
        out_.print(" C");
    } else {
        out_.print(bodyTemperatureStateName(model.bodyTemperature.state));
    }

    out_.print(" | HR: ");
    out_.print(model.reading.heartRate);

    out_.print(" | SpO2: ");
    out_.print(model.reading.spo2);

    out_.print(" | Confidence: ");
    out_.print(model.reading.confidence);

    out_.print(" | Finger status: ");
    out_.println(model.reading.fingerStatus);
}

void TelemetryPrinter::printData(const AppViewModel& model, bool mirrorEnabled) {
    const HealthReading& reading = model.reading;

    out_.print("@DATA rtc=");

    if (model.time.valid) {
        out_.printf(
            "%02d:%02d:%02d",
            model.time.hour,
            model.time.minute,
            model.time.second
        );
    } else {
        out_.print("err");
    }

    out_.print(" temp=");

    if (reading.temperatureValid) {
        out_.print(reading.temperatureC, 2);
    } else {
        out_.print("err");
    }

    out_.print(" body=");

    if (model.bodyTemperature.stable()) {
        out_.print(model.bodyTemperature.bodyC, 2);
    } else {
        out_.print("err");
    }

    out_.printf(
        " tstate=%s tcal=%d",
        bodyTemperatureStateName(model.bodyTemperature.state),
        model.bodyTemperature.calibrated ? 1 : 0
    );

    if (model.status.senInitialized && reading.bioValid) {
        out_.printf(
            " hr=%u spo2=%u conf=%u status=%u",
            reading.heartRate,
            reading.spo2,
            reading.confidence,
            reading.fingerStatus
        );
    } else {
        out_.print(" hr=err spo2=err conf=err status=err");
    }

    out_.printf(
        " sen=%d bat=%u ble=%d sync=%d logseg=%u page=%s mirror=%d\n",
        model.status.senInitialized ? 1 : 0,
        model.status.batteryLevel,
        model.status.bleConnected ? 1 : 0,
        model.status.syncing ? 1 : 0,
        model.status.pendingSegments,
        screenPageName(model.page),
        mirrorEnabled ? 1 : 0
    );
}

#endif
