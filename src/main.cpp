#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>

// put function declarations here:
int myFunction(int, int);

M5Canvas canvas(&M5.Display);

constexpr int SDA_PIN = 9; 
constexpr int SCL_PIN = 10; 
constexpr uint8_t RTC_ADDRESS = 0x51;

//draw scanning section 
void drawScanningScreen() {
    canvas.fillSprite(BLACK); 
    canvas.setTextSize(1); 
    canvas.setTextColor(WHITE); 
    canvas.setCursor(8,8); 

    canvas.println("VitalCare RTC Test"); 
    canvas.println("------------------"); 
    canvas.println(); 
    canvas.println("Scanning I2C bus..."); 
    canvas.println("SDA: G9"); 
    canvas.println("SCL: G10");
    
    //push screen 
    canvas.pushSprite(0, 0); 
}

//draw result section 
void drawResultScreen(
    const uint8_t addresses[], 
    int deviceCount, 
    bool rtcFound
) {
    canvas.fillSprite(BLACK);
    canvas.setTextSize(1.75);
    canvas.setCursor(8, 8);

    canvas.setTextColor(WHITE);
    canvas.println("VitalCare RTC Test");
    canvas.println("-----------------------");

    if (rtcFound) {
        canvas.setTextColor(GREEN);
        canvas.println("RTC HYM8563: OK");
    } else {
        canvas.setTextColor(RED);
        canvas.println("RTC: NOT FOUND");
    }

    canvas.setTextColor(WHITE);
    canvas.printf("Devices found: %d\n", deviceCount);

    for (int i = 0; i < deviceCount && i < 8; i++) {
        canvas.printf(
            "I2C address: 0x%02X\n",
            addresses[i]
        );
    }

    canvas.println();
    canvas.println("Button A: Scan again");

    canvas.pushSprite(0, 0);
}

void scanI2C() {
    uint8_t foundAddresses[16];
    int deviceCount = 0;
    bool rtcFound = false;

    drawScanningScreen();

    Serial.println();
    Serial.println("=== VitalCare External I2C Scan ===");

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf(
                "Device found at 0x%02X\n",
                address
            );

            if (deviceCount < 16) {
                foundAddresses[deviceCount] = address;
                deviceCount++;
            }

            if (address == RTC_ADDRESS) {
                rtcFound = true;
            }
        }
    }

    if (deviceCount == 0) {
        Serial.println("No I2C device found");
    }

    if (rtcFound) {
        Serial.println("HYM8563 RTC detected successfully");
    } else {
        Serial.println("HYM8563 RTC was not detected");
    }

    Serial.printf(
        "Total I2C devices: %d\n",
        deviceCount
    );

    drawResultScreen(
        foundAddresses,
        deviceCount,
        rtcFound
    );
}

void setup() {
  auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(500);

    M5.Display.setRotation(1);

    // Tạo vùng vẽ ảo bằng đúng kích thước màn hình
    canvas.createSprite(
        M5.Display.width(),
        M5.Display.height()
    );

    // Cấp 5V từ StickS3 ra cổng Grove
    M5.Power.setExtOutput(true);
    delay(200);

    // Khởi tạo bus I2C ngoài
    Wire.begin(
        SDA_PIN,
        SCL_PIN,
        100000
    );

    delay(100);
    scanI2C();
}

void loop() {
  M5.update();

    if (M5.BtnA.wasPressed()) {
        scanI2C();
    }

    delay(10);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}