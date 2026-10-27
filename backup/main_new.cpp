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
constexpr uint8_t MAX_SDA = 7;
constexpr uint8_t MAX_SCL = 6;

// ======================================================
// SEN-15219 — Hat2
// ======================================================

constexpr uint8_t SEN_ADDRESS = 0x55;
constexpr uint8_t SEN_SDA = 4;
constexpr uint8_t SEN_SCL = 5;

constexpr uint8_t SEN_MFIO = 1;
constexpr uint8_t SEN_RESET = 8;

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
    uint16_t year = 0; 
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t weekday = 0;  // 0: Chủ nhật, 1: Thứ hai, ..., 6: Thứ bảy
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

//chuyen doi binary-coded decimal sang decimal  
/*
    1 so thong tin can nam ro: 
    a. bcd: binary-coded decimal: so thap phan duoc ma hoa boi binary, 
    duoc RTC su dung de luu gia tri ngay thang nam 
    b. decimal: so thap phan 
    2. Logic (lay vi du chuyen so thap phan 0x37 (0011 0111)) thanh 37 decimal 
    a. Trong bcd, moi so thap phan duoc bieu dien boi 4 bit (0011(3) 0111(7))
    b. Dich bit sang phai 4 bit de lay hang chuc (0011 0111 -> 0000 0011(3)) 
    c. nhan 10 de dua gia tri vua lay ve hang chuc (3*10 = 30) 
    d. su dung 0x0F tuc 0000 1111, kem phep tinh AND de giu lai bit cuoi 
    (0011 0111 & 0000 1111 = 0000 1111 <-> 7 dec) 
    e. Cong gia tri vao de ra gia tri cuoi (30 + 7 = 37) 
*/
uint8_t bcdToDecimal(uint8_t value) {
    return ((value >> 4) * 10) + (value & 0x0F);
}


//chuyen doi decimal to binary-coded decimal 
/*
    1 so thong tin can nam ro: 
    a. bcd: binary-coded decimal: so thap phan duoc ma hoa boi binary, 
    duoc RTC su dung de luu gia tri ngay thang nam 
    b. decimal: so thap phan 
    2. Logic (lay vi du chuyen 37 thanh 0x37)
    a. Chia 10 de lay phep tinh hang chuc (3) 
    b. Dich bit sang trai 4 bit de lay duoc gia tri binary tuong ung 
    (0000 0011 << 4 = 0011 0000 )
    c. Lay gia tri hang don vi (37 % 10 = 7 <-> 0111)
    d. dung phep OR de giu lai ca 8 bit (0011 0000 | 0000 0111 = 0011 0111 <-> 0x37)

*/
uint8_t decimalToBCD(uint8_t value) {
    return ((value / 10) << 4) | (value % 10); 
}

//kiem tra xem co phai 1 so binary-coded decimal phu hop 
//kiem tra xem gia tri 4 bit cao va 4 bit thap co nam trong khoang 
bool isValidBCD(uint8_t value) {
    return (value & 0x0F <= 9) <= 9 && (value >> 4) <=9; 
}

//kiem tra xem co phai nam nhuan 
bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) 
    || (year % 400 == 0); 
}

//khai bao cac ngay co trong thang theo trinh tu thang 1, thang 2, ... , thang 12 
uint8_t daysInMonth(int year, int month) {
    //khai bao danh sach so ngay trong thang theo trinh tu t1, t2, ... , t12 
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30, 
        31, 31, 30, 31, 30, 31
    }; 

    //neu la thang khong hop le, tra ve 0 
    if (month < 1 || month > 12) {
        return 0; 
    }

    //neu la thang 2 trong nam nhuan 
    if (month == 2 && isLeapYear(year)) {
        return 29; //se co 29 ngay 
    }

    //con lai, se lay so ngay theo vi tri trong arr, tinh tu so 0 
    return days[month - 1]; 
}

//kiem tra xem co phai la ngay thang nam hop le 
bool isValidDateTime(
    int year, int month, int day, 
    int hour, int minute, int second
) {
    //HYM8563 Linux chi cho phep ghi thoi gian tu 2000 toi 2099 
    if (year < 2000 || year > 2099) return false; 
    //cac thang keo dai tu 1 toi 12 
    if (month < 1 || month > 12) return false; 
    //do chinh xac cua ngay keo dai 
    if (day < 1 || day > daysInMonth(year, month)) return false; 
    //thoi gian keo dai tu 0h toi 23h 
    if (hour < 0 || hour > 23) return false; 
    //phut se keo dai tu 0 toi 59 
    if (minute < 0 || minute > 59) return false; 
    //giay se keo dai tu 0 toi 59 
    if (second < 0 || second > 59) return false; 

    //cac truong hop con lai true 
    return true; 
}

// ham de tinh toan thu trong tuan 
//0 = chu nhat, 1 = thu hai, 2 = thu ba, ...
/*
    a. Mot so thong tin can ghi nho: 
    - offset[]: do lech thu theo thang duoc quy dinh san 
    (so ngay dich theo thang)
    - year: nam (trong nay se coi la so ngay dich theo bam) 
    - month: thang 
    - day: ngay 
    b. Cac cong thuc can chu y
    1. quy uoc: thang 1, 2 thuoc nam truoc 
    2. year/4 - year/100 + year/4000
    (cong thuc dung de trich xuat so ngay nhuan trong nam) 
    3. 
*/ 
uint8_t calculateWeekday(int year, int month, int day) {
    //danh sach do lech thu trong thang 
    static const int offsets[] = {
        0, 3, 2, 5, 0, 3, 
        5, 1, 4, 6, 2, 4
    }; 

    //phep tinh coi thang 1 va thang 2 thuoc nam truoc 
    if (month < 3) {
        year--; 
    }
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

bool initializeSEN15219() {
    Serial.println("\nStarting SEN-15219...");
    Serial.printf(
        "SDA=G%d, SCL=G%d, RESET=G%d, MFIO=G%d\n",
        SEN_SDA,
        SEN_SCL,
        SEN_RESET,
        SEN_MFIO
    );

    if (!selectI2CBus(SEN_SDA, SEN_SCL)) {
        Serial.println("Cannot start SEN I2C bus");
        senInitialized = false;
        return false;
    }

    delay(100);

    // Quan trọng: liên kết thư viện với bus Wire hiện tại
    uint8_t beginResult = bioHub.begin(
        Wire,
        SEN_RESET,
        SEN_MFIO
    );

    Serial.printf("bioHub.begin result: %u\n", beginResult);

    if (beginResult != 0) {
        Serial.println("SEN-15219 begin FAILED");
        senInitialized = false;
        return false;
    }

    delay(500);

    uint8_t configResult = bioHub.configBpm(MODE_ONE);

    Serial.printf("configBpm result: %u\n", configResult);

    if (configResult != 0) {
        Serial.println("SEN-15219 config FAILED");
        senInitialized = false;
        return false;
    }

    delay(4000);

    uint8_t sensorState = bioHub.readMAX30101State();

    Serial.printf(
        "MAX30101 state: %u\n",
        sensorState
    );

    senInitialized = true;
    Serial.println("SEN-15219 initialized successfully");

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