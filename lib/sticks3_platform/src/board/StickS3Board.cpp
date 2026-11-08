#include "board/StickS3Board.h"

#include <esp32s3/rom/rtc.h>

void StickS3Board::begin(
    uint32_t serialBaud,
    uint8_t rotation,
    uint8_t brightness,
    uint32_t holdThresholdMs
) {
    Serial.begin(serialBaud);
    delay(500);

    auto config = M5.config();
    M5.begin(config);

    M5.Display.setRotation(rotation);
    M5.Display.setBrightness(brightness);

    // Bật nguồn 5V cho RTC qua cổng Grove
    M5.Power.setExtOutput(true);
    delay(500);

    // wasClicked() chỉ kích hoạt khi nhả nút trước ngưỡng giữ,
    // nên click và hold không bao giờ cùng xảy ra
    M5.BtnA.setHoldThresh(holdThresholdMs);
    M5.BtnB.setHoldThresh(holdThresholdMs);
}

void StickS3Board::update() {
    M5.update();

    if (M5.BtnA.wasClicked()) {
        push(AppEvent::BtnAClick);
    }

    if (M5.BtnA.wasHold()) {
        push(AppEvent::BtnAHold);
    }

    if (M5.BtnB.wasClicked()) {
        push(AppEvent::BtnBClick);
    }

    if (M5.BtnB.wasHold()) {
        push(AppEvent::BtnBHold);
    }
}

uint8_t StickS3Board::batteryLevel() {
    int32_t level = M5.Power.getBatteryLevel();

    if (level < 0) {
        return DeviceStatus::BATTERY_UNKNOWN;
    }

    return (uint8_t)min(level, (int32_t)100);
}

const char* StickS3Board::resetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:
            return "power-on";

        case ESP_RST_EXT:
            return "external pin";

        case ESP_RST_SW:
            return "software restart";

        case ESP_RST_PANIC:
            return "panic";

        case ESP_RST_INT_WDT:
            return "interrupt watchdog";

        case ESP_RST_TASK_WDT:
            return "task watchdog";

        case ESP_RST_WDT:
            return "other watchdog";

        case ESP_RST_DEEPSLEEP:
            return "deep sleep wake";

        case ESP_RST_BROWNOUT:
            return "brownout";

        case ESP_RST_SDIO:
            return "sdio";

        default:
            break;
    }

    // esp_reset_reason() không phân loại được reset do cổng USB Serial
    // (máy tính mở cổng: simulator, Serial Monitor, esptool)
    if (rtc_get_reset_reason(0) == USB_UART_CHIP_RESET) {
        return "usb serial port opened";
    }

    return "unknown";
}

bool StickS3Board::isCharging() {
    return M5.Power.isCharging() == m5::Power_Class::is_charging;
}

bool StickS3Board::poll(AppEvent& out) {
    if (queueCount_ == 0) {
        return false;
    }

    out = queue_[0];
    queueCount_--;

    for (uint8_t i = 0; i < queueCount_; i++) {
        queue_[i] = queue_[i + 1];
    }

    return true;
}

void StickS3Board::push(AppEvent event) {
    if (queueCount_ < QUEUE_SIZE) {
        queue_[queueCount_++] = event;
    }
}
