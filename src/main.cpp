#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <math.h>

// RTC trên cổng Grove
constexpr uint8_t RTC_SDA = 9;
constexpr uint8_t RTC_SCL = 10;
constexpr uint8_t RTC_ADDRESS = 0x51;

// MAX30205 trên Hat2
constexpr uint8_t MAX_SDA = 7;
constexpr uint8_t MAX_SCL = 6;
constexpr uint8_t MAX_ADDRESS = 0x48;

M5Canvas canvas(&M5.Display);

enum DisplayPage {
    PAGE_RTC,
    PAGE_TEMPERATURE
};

DisplayPage currentPage = PAGE_RTC;

struct RTCData {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool valid;
};

RTCData rtcData = {0, 0, 0, false};
float skinTemperature = NAN;

uint32_t lastReadTime = 0;

// Chuyển giá trị BCD của RTC sang số thập phân
uint8_t bcdToDecimal(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
}

// Chuyển Wire sang một cặp GPIO khác
bool selectI2CBus(uint8_t sda, uint8_t scl) {
    Wire.end();
    delay(5);

    bool started = Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(50);

    return started;
}

bool readRTC() {
    if (!selectI2CBus(RTC_SDA, RTC_SCL)) {
        rtcData.valid = false;
        return false;
    }

    // Thanh ghi thời gian bắt đầu từ 0x02
    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(0x02);

    if (Wire.endTransmission(false) != 0) {
        rtcData.valid = false;
        return false;
    }

    if (Wire.requestFrom(RTC_ADDRESS, (uint8_t)3) != 3) {
        rtcData.valid = false;
        return false;
    }

    rtcData.second = bcdToDecimal(Wire.read() & 0x7F);
    rtcData.minute = bcdToDecimal(Wire.read() & 0x7F);
    rtcData.hour   = bcdToDecimal(Wire.read() & 0x3F);
    rtcData.valid = true;

    return true;
}

bool readMAX30205() {
    if (!selectI2CBus(MAX_SDA, MAX_SCL)) {
        skinTemperature = NAN;
        return false;
    }

    // Chọn thanh ghi nhiệt độ 0x00
    Wire.beginTransmission(MAX_ADDRESS);
    Wire.write(0x00);

    if (Wire.endTransmission(false) != 0) {
        skinTemperature = NAN;
        return false;
    }

    if (Wire.requestFrom(MAX_ADDRESS, (uint8_t)2) != 2) {
        skinTemperature = NAN;
        return false;
    }

    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    int16_t raw = (int16_t)((msb << 8) | lsb);
    skinTemperature = raw / 256.0f;

    return true;
}

void readAllSensors() {
    readRTC();
    delay(10);
    readMAX30205();

    Serial.print("RTC: ");

    if (rtcData.valid) {
        Serial.printf(
            "%02d:%02d:%02d",
            rtcData.hour,
            rtcData.minute,
            rtcData.second
        );
    } else {
        Serial.print("ERROR");
    }

    Serial.print(" | MAX30205: ");

    if (!isnan(skinTemperature)) {
        Serial.printf("%.2f C\n", skinTemperature);
    } else {
        Serial.println("ERROR");
    }
}

void drawRTCPage() {
    canvas.fillSprite(BLACK);

    canvas.setTextDatum(top_center);
    canvas.setTextColor(CYAN, BLACK);
    canvas.setTextSize(1.5);
    canvas.drawString("RTC TIME", canvas.width() / 2, 8);

    canvas.setTextDatum(middle_center);

    if (rtcData.valid) {
        char timeText[12];

        snprintf(
            timeText,
            sizeof(timeText),
            "%02d:%02d:%02d",
            rtcData.hour,
            rtcData.minute,
            rtcData.second
        );

        canvas.setTextColor(WHITE, BLACK);
        canvas.setTextSize(3);
        canvas.drawString(
            timeText,
            canvas.width() / 2,
            canvas.height() / 2
        );
    } else {
        canvas.setTextColor(RED, BLACK);
        canvas.setTextSize(2);
        canvas.drawString(
            "RTC ERROR",
            canvas.width() / 2,
            canvas.height() / 2
        );
    }

    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(DARKGREY, BLACK);
    canvas.setTextSize(1.5);
    canvas.drawString(
        "Button A: Temperature",
        canvas.width() / 2,
        canvas.height() - 5
    );

    canvas.pushSprite(0, 0);
}

void drawTemperaturePage() {
    canvas.fillSprite(BLACK);

    canvas.setTextDatum(top_center);
    canvas.setTextColor(ORANGE, BLACK);
    canvas.setTextSize(1.5);
    canvas.drawString(
        "WRIST SKIN TEMPERATURE",
        canvas.width() / 2,
        8
    );

    canvas.setTextDatum(middle_center);

    if (!isnan(skinTemperature)) {
        char temperatureText[16];

        snprintf(
            temperatureText,
            sizeof(temperatureText),
            "%.2f C",
            skinTemperature
        );

        canvas.setTextColor(GREEN, BLACK);
        canvas.setTextSize(3);
        canvas.drawString(
            temperatureText,
            canvas.width() / 2,
            canvas.height() / 2
        );
    } else {
        canvas.setTextColor(RED, BLACK);
        canvas.setTextSize(2);
        canvas.drawString(
            "MAX ERROR",
            canvas.width() / 2,
            canvas.height() / 2
        );
    }

    canvas.setTextDatum(bottom_center);
    canvas.setTextColor(DARKGREY, BLACK);
    canvas.setTextSize(1.5);
    canvas.drawString(
        "Button A: RTC",
        canvas.width() / 2,
        canvas.height() - 5
    );

    canvas.pushSprite(0, 0);
}

void drawCurrentPage() {
    if (currentPage == PAGE_RTC) {
        drawRTCPage();
    } else {
        drawTemperaturePage();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    auto config = M5.config();
    M5.begin(config);

    M5.Display.setRotation(1);
    M5.Display.setBrightness(100);

    // Cấp 5V cho RTC trên cổng Grove
    M5.Power.setExtOutput(true);
    delay(500);

    canvas.setColorDepth(16);
    canvas.createSprite(
        M5.Display.width(),
        M5.Display.height()
    );

    readAllSensors();
    drawCurrentPage();

    lastReadTime = millis();
}

void loop() {
    M5.update();

    // Nút màu xanh phía trước
    if (M5.BtnA.wasPressed()) {
        if (currentPage == PAGE_RTC) {
            currentPage = PAGE_TEMPERATURE;
        } else {
            currentPage = PAGE_RTC;
        }

        drawCurrentPage();
    }

    // Cập nhật dữ liệu mỗi giây
    if (millis() - lastReadTime >= 1000) {
        lastReadTime = millis();

        readAllSensors();
        drawCurrentPage();
    }

    delay(10);
}