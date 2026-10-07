#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>

// put function declarations here:
int myFunction(int, int);

M5Canvas canvas(&M5.Display);
constexpr uint8_t RTC_ADDRESS = 0X51; 
constexpr uint8_t RTC_TIME_REGISTER = 0x02; 

constexpr int SDA_PIN = 9;
constexpr int SCL_PIN = 10;

constexpr bool ENABLE_RTC_SETTING = false;

// Hãy sửa thành thời điểm trước khoảng 2–3 phút
constexpr int SET_YEAR   = 2026;
constexpr int SET_MONTH  = 10;
constexpr int SET_DAY    = 5;
constexpr int SET_HOUR   = 10;
constexpr int SET_MINUTE = 51;
constexpr int SET_SECOND = 0;

struct RtcTime {
    int second; 
    int minute; 
    int hour; 
    int day; 
    int weekDay; 
    int month; 
    int year; 
    int voltageLow; 
}; 

uint8_t bcdToDecimal(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t decimalToBcd(uint8_t value) {
    return ((value / 10) << 4) | (value % 10);
}

// Kết quả: 0 = Chủ nhật, 1 = Thứ hai, ..., 6 = Thứ bảy
int calculateWeekday(int year, int month, int day) {
    static const int offsets[] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4
    };

    if (month < 3) {
        year--;
    }

    return (
        year +
        year / 4 -
        year / 100 +
        year / 400 +
        offsets[month - 1] +
        day
    ) % 7;
}

bool writeRtcRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(reg);
    Wire.write(value);

    return Wire.endTransmission() == 0;
}

bool setRtcTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second
) {
    if (year < 2000 || year > 2099 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 ||
        second < 0 || second > 59) {
        return false;
    }

    uint8_t data[7] = {
        decimalToBcd(second),
        decimalToBcd(minute),
        decimalToBcd(hour),
        decimalToBcd(day),
        decimalToBcd(calculateWeekday(year, month, day)),
        decimalToBcd(month),
        decimalToBcd(year - 2000)
    };

    // Dừng bộ đếm RTC trong lúc ghi
    if (!writeRtcRegister(0x00, 0x20)) {
        return false;
    }

    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(RTC_TIME_REGISTER);

    for (uint8_t value : data) {
        Wire.write(value);
    }

    bool writeSuccessful = Wire.endTransmission() == 0;

    // Khởi động lại bộ đếm, kể cả khi lần ghi trước thất bại
    bool restartSuccessful = writeRtcRegister(0x00, 0x00);

    return writeSuccessful && restartSuccessful;
}


bool readRtcTime(RtcTime &time) {
    // Chọn thanh ghi bắt đầu: Seconds = 0x02
    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write(RTC_TIME_REGISTER);

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    // Đọc liên tiếp 7 byte:
    // giây, phút, giờ, ngày, thứ, tháng, năm
    uint8_t received =
        Wire.requestFrom(RTC_ADDRESS, static_cast<uint8_t>(7));

    if (received != 7) {
        return false;
    }

    uint8_t rawSecond = Wire.read();
    uint8_t rawMinute = Wire.read();
    uint8_t rawHour = Wire.read();
    uint8_t rawDay = Wire.read();
    uint8_t rawWeekday = Wire.read();
    uint8_t rawMonth = Wire.read();
    uint8_t rawYear = Wire.read();

    // Bit 7 của thanh ghi giây là cờ Voltage Low
    time.voltageLow = (rawSecond & 0x80) != 0;

    time.second  = bcdToDecimal(rawSecond & 0x7F);
    time.minute  = bcdToDecimal(rawMinute & 0x7F);
    time.hour    = bcdToDecimal(rawHour & 0x3F);
    time.day     = bcdToDecimal(rawDay & 0x3F);
    time.weekDay = bcdToDecimal(rawWeekday & 0x07);
    time.month   = bcdToDecimal(rawMonth & 0x1F);
    time.year    = 2000 + bcdToDecimal(rawYear);

    return true;
}

void drawRtcTime(const RtcTime &time) {
    char timeText[16];
    char dateText[16];

    snprintf(
        timeText,
        sizeof(timeText),
        "%02d:%02d:%02d",
        time.hour,
        time.minute,
        time.second
    );

    snprintf(
        dateText,
        sizeof(dateText),
        "%02d/%02d/%04d",
        time.day,
        time.month,
        time.year
    );

    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(middle_center);

    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.setTextSize(1.5);
    canvas.drawString(
        "HYM8563 RTC",
        canvas.width() / 2,
        12
    );

    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextSize(2);
    canvas.drawString(
        timeText,
        canvas.width() / 2,
        45
    );

    canvas.setTextSize(1.75);
    canvas.drawString(
        dateText,
        canvas.width() / 2,
        72
    );

    if (time.voltageLow) {
        canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
        canvas.setTextSize(1.5); 
        canvas.drawString(
            "VL: Time not valid",
            canvas.width() / 2,
            100
        );
    } else {
        canvas.setTextColor(TFT_GREEN, TFT_BLACK);
        canvas.setTextSize(1.5); 
        canvas.drawString(
            "RTC data valid",
            canvas.width() / 2,
            100
        );
    }

    canvas.pushSprite(0, 0);
}

void drawReadError() {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextDatum(middle_center);

    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.setTextSize(1.5);

    canvas.drawString(
        "RTC read failed",
        canvas.width() / 2,
        45
    );

    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.drawString(
        "Address: 0x51",
        canvas.width() / 2,
        70
    );

    canvas.pushSprite(0, 0);
}


void setup() {
    auto config = M5.config();
    M5.begin(config);

    Serial.begin(115200);

    // Bật nguồn 5V cho thiết bị Grove bên ngoài
    M5.Power.setExtOutput(true);
    delay(300);

    // Khởi tạo I2C của StickS3
    Wire.begin(SDA_PIN, SCL_PIN, 100000);

    canvas.createSprite(
        M5.Display.width(),
        M5.Display.height()
    );

    canvas.setTextWrap(false);

    Serial.println("Starting HYM8563 RTC reader...");
}

void loop() {
    M5.update();

    if (ENABLE_RTC_SETTING && M5.BtnA.wasPressed()) {
    bool success = setRtcTime(
        SET_YEAR,
        SET_MONTH,
        SET_DAY,
        SET_HOUR,
        SET_MINUTE,
        SET_SECOND
    );

    if (success) {
        Serial.println("RTC time updated successfully");
    } else {
        Serial.println("Failed to update RTC time");
    }

    delay(300);
    }

    static unsigned long previousRead = 0;

    if (millis() - previousRead >= 1000 || previousRead == 0) {
        previousRead = millis();

        RtcTime currentTime;

        if (readRtcTime(currentTime)) {
            drawRtcTime(currentTime);

            Serial.printf(
                "%02d/%02d/%04d %02d:%02d:%02d | VL=%s\n",
                currentTime.day,
                currentTime.month,
                currentTime.year,
                currentTime.hour,
                currentTime.minute,
                currentTime.second,
                currentTime.voltageLow ? "YES" : "NO"
            );
        } else {
            drawReadError();
            Serial.println("Cannot read RTC at address 0x51");
        }
    }

    delay(10);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}