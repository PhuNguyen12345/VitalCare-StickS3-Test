#include <Arduino.h>
#include <Wire.h>

#include "AppConfig.h"
#include "BoardPins.h"
#include "BuildConfig.h"

#include "app/App.h"
#include "services/MeasurementService.h"
#include "services/TimeService.h"

#include "board/I2cBusManager.h"
#include "board/StickS3Board.h"
#include "sensors/Max30205Provider.h"
#include "sensors/SenSensorProvider.h"
#include "time/RtcTimeProvider.h"

#include "UiController.h"

#if VC_ENABLE_DEVTOOLS
#include "SerialMirror.h"
#include "SerialRemote.h"
#include "TelemetryPrinter.h"
#endif

// ======================================================
// Phần cứng
// ======================================================

StickS3Board board;

I2cBusManager i2cBus(
    Wire,
    AppConfig::I2C_FREQUENCY,
    AppConfig::I2C_TIMEOUT_MS
);

SenSensorProvider senSensor(
    i2cBus,
    BoardPins::HAT_SDA,
    BoardPins::HAT_SCL,
    BoardPins::SEN_RESET,
    BoardPins::SEN_MFIO
);

Max30205Provider maxSensor(
    i2cBus,
    BoardPins::HAT_SDA,
    BoardPins::HAT_SCL,
    BoardPins::MAX_ADDRESS
);

RtcTimeProvider rtc(
    i2cBus,
    BoardPins::RTC_SDA,
    BoardPins::RTC_SCL,
    BoardPins::RTC_ADDRESS
);

// ======================================================
// Ứng dụng
// ======================================================

MeasurementService measurementService;
TimeService timeService(rtc, AppConfig::RTC_INTERVAL);

App app(&timeService, &measurementService);

UiController ui(M5.Display);

uint32_t lastDisplayUpdate = 0;

// ======================================================
// Công cụ phát triển (mirror, lệnh Serial, @DATA)
// ======================================================

#if VC_ENABLE_DEVTOOLS
SerialMirror mirror(Serial);
SerialRemote remote(Serial, mirror);
TelemetryPrinter telemetry(Serial, AppConfig::SERIAL_INTERVAL);
#endif

// ======================================================
// Setup
// ======================================================

void setup() {
    board.begin(
        AppConfig::SERIAL_BAUD,
        AppConfig::DISPLAY_ROTATION,
        AppConfig::DISPLAY_BRIGHTNESS,
        AppConfig::BUTTON_HOLD_MS
    );

    if (!ui.begin(AppConfig::CANVAS_WIDTH, AppConfig::CANVAS_HEIGHT)) {
        Serial.println("Cannot allocate canvas");
    }

#if VC_ENABLE_DEVTOOLS
    ui.setFrameListener(&mirror);
    remote.setTestPatternHandler([]() { ui.showTestPattern(); });
#endif

    ui.showBoot();

    app.setLogger([](const char* message) { Serial.println(message); });

    // Khởi tạo SEN-15219 trước, rồi MAX30205
    measurementService.addSource(senSensor, AppConfig::BIO_INTERVAL);
    measurementService.addSource(maxSensor, AppConfig::TEMPERATURE_INTERVAL);

    app.begin(millis());

    // Mặc định hiển thị đồng hồ
    app.consumeRedrawRequest();
    ui.render(app.viewModel());

#if VC_ENABLE_DEVTOOLS
    Serial.println("Commands: n/c=click A/B, N/C=hold A/B, M/m=mirror on/off, s=capture, a=auto capture, t=test pattern");
    telemetry.begin(millis());
#endif

    lastDisplayUpdate = millis();
}

// ======================================================
// Loop
// ======================================================

void loop() {
    board.update();

    // Nút thật và lệnh từ simulator đều thành AppEvent
    AppEvent event;

    while (board.poll(event)) {
        app.handleEvent(event);
    }

#if VC_ENABLE_DEVTOOLS
    while (remote.poll(event)) {
        app.handleEvent(event);
    }
#endif

    uint32_t now = millis();

    app.update(now);

    // Vẽ ngay khi đổi trang, còn lại theo chu kỳ
    if (
        app.consumeRedrawRequest() ||
        now - lastDisplayUpdate >= AppConfig::DISPLAY_INTERVAL
    ) {
        lastDisplayUpdate = now;
        ui.render(app.viewModel());
    }

#if VC_ENABLE_DEVTOOLS
    telemetry.update(now, app.viewModel(), mirror.mirrorEnabled());
#endif

    delay(10);
}
