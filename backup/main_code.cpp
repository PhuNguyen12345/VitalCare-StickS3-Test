#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <SparkFun_Bio_Sensor_Hub_Library.h>
#include <math.h>

// ======================================================
// RTC HYM8563 — Grove
// ======================================================

constexpr uint8_t RTC_ADDRESS = 0x51;
constexpr uint8_t RTC_SDA = 9;
constexpr uint8_t RTC_SCL = 10;

// ======================================================
// MAX30205 — Hat2
// ======================================================

constexpr uint8_t MAX_ADDRESS = 0x48;
constexpr uint8_t MAX_SDA = 6;
constexpr uint8_t MAX_SCL = 7;

// ======================================================
// SEN-15219 — Hat2
// ======================================================

constexpr uint8_t SEN_ADDRESS = 0x55;
constexpr uint8_t SEN_SDA = 4;
constexpr uint8_t SEN_SCL = 5;

constexpr uint8_t SEN_MFIO = 8;
constexpr uint8_t SEN_RESET = 1;

// Constructor nhận RESET trước, MFIO sau
SparkFun_Bio_Sensor_Hub bioHub(SEN_RESET, SEN_MFIO);

bioData body;

// ======================================================
// Màn hình
// ======================================================

M5Canvas canvas(&M5.Display);

// Chuẩn hóa cỡ chữ cho màn hình 240 x 135
constexpr float FONT_SMALL = 1.5f;
constexpr float FONT_MESSAGE = 2.0f;
constexpr float FONT_VALUE = 3.0f;

enum class ScreenPage : uint8_t {
    CLOCK,
    TEMPERATURE,
    HEART_RATE,
    SPO2
};

ScreenPage currentPage = ScreenPage::CLOCK;

// ======================================================
// Dữ liệu cảm biến
// ======================================================

struct RTCData {
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
    bool valid = false;
};

RTCData rtcData;

float skinTemperature = NAN;

bool senInitialized = false;
bool senDataAvailable = false;

// Theo dõi bus I2C hiện tại
uint8_t currentSDA = 255;
uint8_t currentSCL = 255;

// ======================================================
// Bộ định thời
// ======================================================

uint32_t lastRTCRead = 0;
uint32_t lastTemperatureRead = 0;
uint32_t lastBioRead = 0;
uint32_t lastDisplayUpdate = 0;
uint32_t lastSerialPrint = 0;

constexpr uint32_t RTC_INTERVAL = 1000;
constexpr uint32_t TEMPERATURE_INTERVAL = 1000;
constexpr uint32_t BIO_INTERVAL = 250;
constexpr uint32_t DISPLAY_INTERVAL = 200;
constexpr uint32_t SERIAL_INTERVAL = 1000;

// ======================================================
// Chuyển bus I2C
// ======================================================

bool selectI2CBus(uint8_t sda, uint8_t scl) {
    // Không khởi tạo lại nếu đã ở đúng bus
    if (currentSDA == sda && currentSCL == scl) {
        return true;
    }

    if (currentSDA != 255) {
        Wire.end();
        delay(5);
    }

    bool started = Wire.begin(sda, scl, 100000);

    if (!started) {
        currentSDA = 255;
        currentSCL = 255;

        Serial.printf(
            "Failed to start I2C: SDA G%d, SCL G%d\n",
            sda,
            scl
        );

        return false;
    }

    Wire.setTimeOut(100);

    currentSDA = sda;
    currentSCL = scl;

    return true;
}

// ======================================================
// Đọc RTC
// ======================================================

uint8_t bcdToDecimal(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
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
    rtcData.hour = bcdToDecimal(Wire.read() & 0x3F);
    rtcData.valid = true;

    return true;
}

// ======================================================
// Đọc MAX30205
// ======================================================

bool readMAX30205() {
    if (!selectI2CBus(MAX_SDA, MAX_SCL)) {
        skinTemperature = NAN;
        return false;
    }

    // Thanh ghi nhiệt độ 0x00
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

// ======================================================
// Khởi tạo và đọc SEN-15219
// ======================================================

// bool initializeSEN15219() {
//     Serial.println("\nStarting SEN-15219...");
//     Serial.printf(
//         "SDA=G%d, SCL=G%d, RESET=G%d, MFIO=G%d\n",
//         SEN_SDA,
//         SEN_SCL,
//         SEN_RESET,
//         SEN_MFIO
//     );

//     if (!selectI2CBus(SEN_SDA, SEN_SCL)) {
//         Serial.println("Cannot start SEN I2C bus");
//         senInitialized = false;
//         return false;
//     }

//     delay(100);

//     // Quan trọng: liên kết thư viện với bus Wire hiện tại
//     uint8_t beginResult = bioHub.begin(
//         Wire,
//         SEN_RESET,
//         SEN_MFIO
//     );

//     Serial.printf("bioHub.begin result: %u\n", beginResult);

//     if (beginResult != 0) {
//         Serial.println("SEN-15219 begin FAILED");
//         senInitialized = false;
//         return false;
//     }

//     delay(500);

//     uint8_t configResult = bioHub.configBpm(MODE_ONE);

//     Serial.printf("configBpm result: %u\n", configResult);

//     if (configResult != 0) {
//         Serial.println("SEN-15219 config FAILED");
//         senInitialized = false;
//         return false;
//     }

//     delay(4000);

//     uint8_t sensorState = bioHub.readMAX30101State();

//     Serial.printf(
//         "MAX30101 state: %u\n",
//         sensorState
//     );

//     senInitialized = true;
//     Serial.println("SEN-15219 initialized successfully");

//     return true;
// }

bool initializeSEN15219() {
    Serial.println("\nStarting SEN-15219...");
    Serial.printf(
        "SDA=G%d, SCL=G%d, RESET=G%d, MFIO=G%d\n",
        SEN_SDA,
        SEN_SCL,
        SEN_RESET,
        SEN_MFIO
    );

    // 1. Kích hoạt chân phần cứng thủ công trước
    pinMode(SEN_RESET, OUTPUT);
    digitalWrite(SEN_RESET, HIGH);
    pinMode(SEN_MFIO, INPUT);
    delay(50);

    // 2. Chuyển sang bus I2C của SEN
    if (!selectI2CBus(SEN_SDA, SEN_SCL)) {
        Serial.println("Cannot start SEN I2C bus");
        senInitialized = false;
        return false;
    }

    delay(100);

    // 3. Khởi tạo bioHub gắn với bus Wire hiện tại
    uint8_t beginResult = bioHub.begin(Wire);
    Serial.printf("bioHub.begin result: %u\n", beginResult);

    if (beginResult != 0) {
        Serial.println("SEN-15219 begin FAILED");
        senInitialized = false;
        return false;
    }

    delay(200);

    // 4. Cấu hình chế độ đo nhịp tim / SpO2
    uint8_t configResult = bioHub.configBpm(MODE_ONE);
    Serial.printf("configBpm result: %u\n", configResult);

    if (configResult != 0) {
        Serial.println("SEN-15219 config FAILED");
        senInitialized = false;
        return false;
    }

    senInitialized = true;
    Serial.println("SEN-15219 initialized successfully!");

    return true;
}

bool readSEN15219() {
    if (!senInitialized) {
        return false;
    }

    if (!selectI2CBus(SEN_SDA, SEN_SCL)) {
        senDataAvailable = false;
        return false;
    }

    body = bioHub.readBpm();
    senDataAvailable = true;

    return true;
}

// ======================================================
// Thành phần giao diện chung
// ======================================================

void drawFooter(const char* text) {
    canvas.setTextDatum(bottom_center);
    canvas.setTextSize(FONT_SMALL);
    canvas.setTextColor(DARKGREY, BLACK);

    canvas.drawString(
        text,
        canvas.width() / 2,
        canvas.height() - 4
    );
}

// ======================================================
// Trang đồng hồ
// ======================================================

void drawClockPage() {
    canvas.fillSprite(BLACK);

    canvas.setTextDatum(top_center);
    canvas.setTextSize(FONT_SMALL);
    canvas.setTextColor(CYAN, BLACK);

    canvas.drawString(
        "VITALCARE - RTC",
        canvas.width() / 2,
        6
    );

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
        canvas.setTextSize(FONT_VALUE);

        canvas.drawString(
            timeText,
            canvas.width() / 2,
            canvas.height() / 2
        );
    } else {
        canvas.setTextColor(RED, BLACK);
        canvas.setTextSize(FONT_MESSAGE);

        canvas.drawString(
            "RTC ERROR",
            canvas.width() / 2,
            canvas.height() / 2
        );
    }

    drawFooter("A: Measurements");
    canvas.pushSprite(0, 0);
}

// ======================================================
// Trang nhiệt độ
// ======================================================

void drawTemperaturePage() {
    canvas.fillSprite(BLACK);

    canvas.setTextDatum(top_center);
    canvas.setTextSize(FONT_SMALL);
    canvas.setTextColor(YELLOW, BLACK);

    canvas.drawString(
        "SKIN TEMPERATURE",
        canvas.width() / 2,
        6
    );

    canvas.setTextDatum(middle_center);

    if (!isnan(skinTemperature)) {
        char temperatureText[20];

        snprintf(
            temperatureText,
            sizeof(temperatureText),
            "%.2f C",
            skinTemperature
        );

        canvas.setTextColor(GREEN, BLACK);
        canvas.setTextSize(FONT_VALUE);

        canvas.drawString(
            temperatureText,
            canvas.width() / 2,
            canvas.height() / 2
        );
    } else {
        canvas.setTextColor(RED, BLACK);
        canvas.setTextSize(FONT_MESSAGE);

        canvas.drawString(
            "MAX ERROR",
            canvas.width() / 2,
            canvas.height() / 2
        );
    }

    drawFooter("A: Next   B: Clock");
    canvas.pushSprite(0, 0);
}

// ======================================================
// Trang nhịp tim
// ======================================================

void drawHeartRatePage() {
    canvas.fillSprite(BLACK);

    canvas.setTextDatum(top_center);
    canvas.setTextSize(FONT_SMALL);
    canvas.setTextColor(RED, BLACK);

    canvas.drawString(
        "HEART RATE",
        canvas.width() / 2,
        6
    );

    canvas.setTextDatum(middle_center);

    if (!senInitialized) {
        canvas.setTextColor(RED, BLACK);
        canvas.setTextSize(FONT_MESSAGE);

        canvas.drawString(
            "SEN ERROR",
            canvas.width() / 2,
            canvas.height() / 2
        );
    } else if (!senDataAvailable || body.status != 3) {
        canvas.setTextColor(YELLOW, BLACK);
        canvas.setTextSize(FONT_MESSAGE);

        canvas.drawString(
            "PLACE FINGER",
            canvas.width() / 2,
            canvas.height() / 2 - 8
        );

        canvas.setTextColor(WHITE, BLACK);
        canvas.setTextSize(FONT_SMALL);

        canvas.drawString(
            "Light, steady pressure",
            canvas.width() / 2,
            canvas.height() / 2 + 22
        );
    } else {
        char heartRateText[20];

        snprintf(
            heartRateText,
            sizeof(heartRateText),
            "%.1f BPM",
            (double)body.heartRate
        );

        canvas.setTextColor(WHITE, BLACK);
        canvas.setTextSize(FONT_VALUE);

        canvas.drawString(
            heartRateText,
            canvas.width() / 2,
            canvas.height() / 2 - 7
        );

        char confidenceText[24];

        snprintf(
            confidenceText,
            sizeof(confidenceText),
            "Confidence: %d%%",
            body.confidence
        );

        canvas.setTextColor(GREEN, BLACK);
        canvas.setTextSize(FONT_SMALL);

        canvas.drawString(
            confidenceText,
            canvas.width() / 2,
            canvas.height() / 2 + 27
        );
    }

    drawFooter("A: Next   B: Clock");
    canvas.pushSprite(0, 0);
}

// ======================================================
// Trang SpO2
// ======================================================

void drawSpO2Page() {
    canvas.fillSprite(BLACK);

    canvas.setTextDatum(top_center);
    canvas.setTextSize(FONT_SMALL);
    canvas.setTextColor(CYAN, BLACK);

    canvas.drawString(
        "BLOOD OXYGEN",
        canvas.width() / 2,
        6
    );

    canvas.setTextDatum(middle_center);

    if (!senInitialized) {
        canvas.setTextColor(RED, BLACK);
        canvas.setTextSize(FONT_MESSAGE);

        canvas.drawString(
            "SEN ERROR",
            canvas.width() / 2,
            canvas.height() / 2
        );
    } else if (!senDataAvailable || body.status != 3) {
        canvas.setTextColor(YELLOW, BLACK);
        canvas.setTextSize(FONT_MESSAGE);

        canvas.drawString(
            "PLACE FINGER",
            canvas.width() / 2,
            canvas.height() / 2 - 8
        );

        canvas.setTextColor(WHITE, BLACK);
        canvas.setTextSize(FONT_SMALL);

        canvas.drawString(
            "Wait for stable signal",
            canvas.width() / 2,
            canvas.height() / 2 + 22
        );
    } else {
        char oxygenText[20];

        snprintf(
            oxygenText,
            sizeof(oxygenText),
            "%.1f %%",
            (double)body.oxygen
        );

        canvas.setTextColor(WHITE, BLACK);
        canvas.setTextSize(FONT_VALUE);

        canvas.drawString(
            oxygenText,
            canvas.width() / 2,
            canvas.height() / 2 - 7
        );

        char confidenceText[24];

        snprintf(
            confidenceText,
            sizeof(confidenceText),
            "Confidence: %d%%",
            body.confidence
        );

        canvas.setTextColor(GREEN, BLACK);
        canvas.setTextSize(FONT_SMALL);

        canvas.drawString(
            confidenceText,
            canvas.width() / 2,
            canvas.height() / 2 + 27
        );
    }

    drawFooter("A: Next   B: Clock");
    canvas.pushSprite(0, 0);
}

// ======================================================
// Vẽ trang hiện tại
// ======================================================

void drawCurrentPage() {
    switch (currentPage) {
        case ScreenPage::CLOCK:
            drawClockPage();
            break;

        case ScreenPage::TEMPERATURE:
            drawTemperaturePage();
            break;

        case ScreenPage::HEART_RATE:
            drawHeartRatePage();
            break;

        case ScreenPage::SPO2:
            drawSpO2Page();
            break;
    }
}

// ======================================================
// Điều hướng màn hình
// ======================================================

void goToNextMeasurementPage() {
    switch (currentPage) {
        case ScreenPage::CLOCK:
            currentPage = ScreenPage::TEMPERATURE;
            break;

        case ScreenPage::TEMPERATURE:
            currentPage = ScreenPage::HEART_RATE;
            break;

        case ScreenPage::HEART_RATE:
            currentPage = ScreenPage::SPO2;
            break;

        case ScreenPage::SPO2:
            currentPage = ScreenPage::TEMPERATURE;
            break;
    }

    drawCurrentPage();
}

void goToClockPage() {
    currentPage = ScreenPage::CLOCK;
    drawCurrentPage();
}

// ======================================================
// Serial Monitor
// ======================================================

void printSensorData() {
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

    Serial.print(" | Temp: ");

    if (!isnan(skinTemperature)) {
        Serial.print(skinTemperature, 2);
        Serial.print(" C");
    } else {
        Serial.print("ERROR");
    }

    Serial.print(" | HR: ");
    Serial.print(body.heartRate);

    Serial.print(" | SpO2: ");
    Serial.print(body.oxygen);

    Serial.print(" | Confidence: ");
    Serial.print(body.confidence);

    Serial.print(" | Finger status: ");
    Serial.println(body.status);
}

// ======================================================
// Setup
// ======================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    auto config = M5.config();
    M5.begin(config);

    M5.Display.setRotation(1);
    M5.Display.setBrightness(100);

    // Bật nguồn 5V cho RTC qua cổng Grove
    M5.Power.setExtOutput(true);
    delay(500);

    canvas.setColorDepth(16);

    canvas.createSprite(
        M5.Display.width(),
        M5.Display.height()
    );

    canvas.setTextWrap(false);

    // Màn hình khởi động
    canvas.fillSprite(BLACK);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(WHITE, BLACK);
    canvas.setTextSize(FONT_MESSAGE);

    canvas.drawString(
        "Starting...",
        canvas.width() / 2,
        canvas.height() / 2
    );

    canvas.pushSprite(0, 0);

    // Khởi tạo SEN-15219 trước
    senInitialized = initializeSEN15219();

    // Đọc dữ liệu đầu tiên
    readRTC();
    readMAX30205();

    if (senInitialized) {
        readSEN15219();
    }

    // Mặc định hiển thị đồng hồ
    currentPage = ScreenPage::CLOCK;
    drawCurrentPage();

    uint32_t now = millis();

    lastRTCRead = now;
    lastTemperatureRead = now;
    lastBioRead = now;
    lastDisplayUpdate = now;
    lastSerialPrint = now;
}

// ======================================================
// Loop
// ======================================================

void loop() {
    M5.update();

    // Button A: chuyển giữa ba chế độ đo
    if (M5.BtnA.wasPressed()) {
        goToNextMeasurementPage();
    }

    // Button B: shortcut quay về đồng hồ
    if (M5.BtnB.wasPressed()) {
        goToClockPage();
    }

    uint32_t now = millis();

    // RTC: mỗi giây
    if (now - lastRTCRead >= RTC_INTERVAL) {
        lastRTCRead = now;
        readRTC();
    }

    // MAX30205: mỗi giây
    if (
        now - lastTemperatureRead >=
        TEMPERATURE_INTERVAL
    ) {
        lastTemperatureRead = now;
        readMAX30205();
    }

    // SEN-15219: mỗi 250 ms
    if (
        senInitialized &&
        now - lastBioRead >= BIO_INTERVAL
    ) {
        lastBioRead = now;
        readSEN15219();
    }

    // Cập nhật màn hình
    if (
        now - lastDisplayUpdate >=
        DISPLAY_INTERVAL
    ) {
        lastDisplayUpdate = now;
        drawCurrentPage();
    }

    // In dữ liệu debug
    if (
        now - lastSerialPrint >=
        SERIAL_INTERVAL
    ) {
        lastSerialPrint = now;
        printSensorData();
    }

    delay(10);
}