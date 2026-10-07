#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <math.h>

constexpr uint8_t MAX30205_ADDRESS = 0x48;
constexpr uint8_t MAX_SDA = 7;
constexpr uint8_t MAX_SCL = 6;
constexpr uint8_t TEMP_REGISTER = 0x00;

float readTemperature() {
    // Chọn thanh ghi nhiệt độ 0x00
    Wire.beginTransmission(MAX30205_ADDRESS);
    Wire.write(TEMP_REGISTER);

    if (Wire.endTransmission(false) != 0) {
        return NAN;
    }

    // MAX30205 trả về hai byte
    if (Wire.requestFrom(MAX30205_ADDRESS, (uint8_t)2) != 2) {
        return NAN;
    }

    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    int16_t raw = (int16_t)((msb << 8) | lsb);

    // Độ phân giải: 1/256°C
    return raw / 256.0f;
}

void setup() {
    Serial.begin(115200);
    delay(500);

    auto config = M5.config();
    M5.begin(config);

    M5.Display.setRotation(1);
    M5.Display.setBrightness(100);
    M5.Display.fillScreen(BLACK);

    M5.Power.setExtOutput(true);
    delay(500);

    // Chuyển bus ngoài sang G7/G6 để test MAX
    Wire.end();
    delay(100);

    if (!Wire.begin(MAX_SDA, MAX_SCL, 100000)) {
        Serial.println("I2C initialization failed");

        M5.Display.setTextColor(RED, BLACK);
        M5.Display.setCursor(5, 5);
        M5.Display.println("I2C init failed");
        return;
    }

    Wire.setTimeOut(50);

    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setTextSize(1.75);
    M5.Display.setCursor(5, 5);
    M5.Display.println("MAX30205 Test");
    M5.Display.println("Address: 0x48");
    M5.Display.println("SDA: G7  SCL: G6");
}

void loop() {
    M5.update();

    float temperature = readTemperature();

    M5.Display.fillRect(0, 55, M5.Display.width(), 70, BLACK);
    M5.Display.setCursor(5, 60);

    if (isnan(temperature)) {
        Serial.println("Failed to read MAX30205");

        M5.Display.setTextColor(RED, BLACK);
        M5.Display.println("Read failed");
    } else {
        Serial.printf("Temperature: %.4f C\n", temperature);

        M5.Display.setTextColor(GREEN, BLACK);
        M5.Display.setTextSize(1.75);
        M5.Display.printf("%.2f C", temperature);
        M5.Display.setTextSize(1.75);
    }

    delay(1000);
}