#include <Arduino.h>
#include <M5Unified.h>

// put function declarations here:
int myFunction(int, int);

unsigned long lastRefresh = 0; 

void drawStatus() {
  const size_t psramBytes = ESP.getPsramSize();
  const int batteryLevel = M5.Power.getBatteryLevel();
  const int batteryVoltage = M5.Power.getBatteryVoltage();
  const bool isCharging = M5.Power.isCharging();

  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(8, 8);

  M5.Display.println("VitalCare Hardware Test");
    M5.Display.println("-----------------------");

    M5.Display.printf(
        "PSRAM: %.1f MB\n",
        psramBytes / 1024.0 / 1024.0
    );

    M5.Display.printf("Battery: %d%%\n", batteryLevel);
    M5.Display.printf("Voltage: %d mV\n", batteryVoltage);
    M5.Display.printf(
        "Charging: %s\n",
        isCharging ? "YES" : "NO"
    );

    M5.Display.printf(
        "Button A: %s\n",
        M5.BtnA.isPressed() ? "PRESSED" : "Released"
    );

    M5.Display.printf(
        "Button B: %s\n",
        M5.BtnB.isPressed() ? "PRESSED" : "Released"
    );
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  // Chưa cấp nguồn cho Grove
  M5.Power.setExtOutput(false);

  Serial.begin(115200);
  delay(500);

  M5.Display.setRotation(1);
  M5.Display.setTextColor(WHITE);

  Serial.println("=== VitalCare StickS3 Hardware Test ===");
  Serial.printf(
        "PSRAM size: %u bytes\n",
        ESP.getPsramSize()
    );

  drawStatus();
}

void loop() {
  M5.update();

    if (M5.BtnA.wasPressed()) {
        Serial.println("Button A pressed");
    }

    if (M5.BtnB.wasPressed()) {
        Serial.println("Button B pressed");
    }

    if (millis() - lastRefresh >= 200) {
        lastRefresh = millis();
        drawStatus();
    }

    delay(10);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}