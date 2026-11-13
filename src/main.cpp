#include <Arduino.h>
#include <Wire.h>

#include "AppConfig.h"
#include "BleConfig.h"
#include "BoardPins.h"
#include "BuildConfig.h"

#include "app/App.h"
#include "services/BodyTemperatureEstimator.h"
#include "services/LogService.h"
#include "services/MeasurementService.h"
#include "services/SyncService.h"
#include "services/TimeService.h"

#include "board/I2cBusManager.h"
#include "board/StickS3Board.h"
#include "connectivity/BleManager.h"
#include "sensors/Max30205Provider.h"
#include "sensors/SenSensorProvider.h"
#include "storage/LittleFsLogRepository.h"
#include "storage/PreferencesConfigRepository.h"
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

PreferencesConfigRepository configRepository;

LittleFsLogRepository logRepository(
    AppConfig::LOG_SEGMENT_MAX_BYTES,
    AppConfig::LOG_RESERVE_BYTES
);

BleManager ble(
    BleConfig::NAME_PREFIX,
    {
        BleConfig::SERVICE_UUID,
        BleConfig::LIVE_UUID,
        BleConfig::CONTROL_UUID,
        BleConfig::SYNC_UUID,
        BleConfig::STATUS_UUID
    },
    BleConfig::PREFERRED_MTU
);

// ======================================================
// Ứng dụng
// ======================================================

MeasurementService measurementService;

TimeService timeService(
    rtc,
    &configRepository,
    AppConfig::RTC_INTERVAL,
    AppConfig::DEFAULT_TZ_MINUTES
);

// Độ lệch nhiệt độ đã hiệu chuẩn lưu trong NVS
BodyTemperatureEstimator bodyTemperature(&configRepository);

LogService logService(
    logRepository,
    AppConfig::LOG_SAMPLE_INTERVAL,
    AppConfig::LOG_WINDOW
);

SyncService syncService(
    ble,
    logRepository,
    timeService,
    AppConfig::BLE_LIVE_INTERVAL,
    AppConfig::BLE_STATUS_INTERVAL,
    AppConfig::BLE_SYNC_ACK_TIMEOUT
);

App app(
    &timeService,
    &measurementService,
    &logService,
    &syncService,
    &bodyTemperature
);

UiController ui(M5.Display);

uint32_t lastDisplayUpdate = 0;
uint32_t lastBatteryRead = 0;

void logLine(const char* message) {
    Serial.println(message);
}

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

    Serial.printf("Boot: reset reason = %s\n", StickS3Board::resetReason());

    app.setLogger(logLine);
    syncService.setLogger(logLine);
    syncService.setTemperatureEstimator(&bodyTemperature);

    configRepository.begin();
    logRepository.begin();
    syncService.begin();

    app.setBattery(board.batteryLevel(), board.isCharging());

    // Khởi tạo SEN-15219 trước, rồi MAX30205
    measurementService.addSource(senSensor, AppConfig::BIO_INTERVAL);
    measurementService.addSource(maxSensor, AppConfig::TEMPERATURE_INTERVAL);

    app.begin(millis());

    // Chẩn đoán phần cứng: dây và nguồn của cụm cảm biến trên Hat2.
    // Chạy sau khi khởi tạo cảm biến: MAX32664 (SEN-15219) chỉ trả lời I2C
    // sau khi được đưa vào chế độ ứng dụng qua chân RST/MFIO
    const uint8_t hatAddresses[] = {BoardPins::MAX_ADDRESS, BoardPins::SEN_ADDRESS};

    i2cBus.check(
        "Hat2",
        BoardPins::HAT_SDA,
        BoardPins::HAT_SCL,
        hatAddresses,
        sizeof(hatAddresses)
    );

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

    if (now - lastBatteryRead >= AppConfig::BATTERY_INTERVAL) {
        lastBatteryRead = now;
        app.setBattery(board.batteryLevel(), board.isCharging());
    }

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
