#include <Arduino.h>
#include <M5Unified.h>

// put function declarations here:
int myFunction(int, int);

M5Canvas canvas(&M5.Display);

unsigned long lastBatteryUpdate = 0;
unsigned long lastFrameUpdate = 0;

int batteryLevel = 0;
int batteryVoltage = 0;
bool charging = false;

void updateBatteryData() {
    batteryLevel = M5.Power.getBatteryLevel();
    batteryVoltage = M5.Power.getBatteryVoltage();
    charging = M5.Power.isCharging();
}

void drawStatus() {
  const size_t psramBytes = ESP.getPsramSize();

    // Chỉ xoá vùng nhớ ảo, chưa tác động lên màn hình thật
    canvas.fillSprite(BLACK);

    canvas.setTextSize(1.75);
    canvas.setTextColor(WHITE);
    canvas.setCursor(8, 8);

    canvas.println("VitalCare Hardware Test");
    canvas.println("-----------------------");

    canvas.printf(
        "PSRAM: %.1f MB\n",
        psramBytes / 1024.0 / 1024.0
    );

    canvas.printf("Battery: %d%%\n", batteryLevel);
    canvas.printf("Voltage: %d mV\n", batteryVoltage);
    canvas.printf(
        "Charging: %s\n",
        charging ? "YES" : "NO"
    );

    canvas.printf(
        "Button A: %s\n",
        M5.BtnA.isPressed() ? "PRESSED" : "Released"
    );

    canvas.printf(
        "Button B: %s\n",
        M5.BtnB.isPressed() ? "PRESSED" : "Released"
    );

    // Đẩy toàn bộ khung hình lên màn hình cùng lúc
    canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
    M5.begin(cfg);

    M5.Power.setExtOutput(false);

    Serial.begin(115200);
    delay(500);

    M5.Display.setRotation(1);

    canvas.createSprite(
        M5.Display.width(),
        M5.Display.height()
    );

    updateBatteryData();
    drawStatus();

    Serial.println("VitalCare hardware test started");
    Serial.printf(
        "PSRAM size: %u bytes\n",
        ESP.getPsramSize()
    );
}

void loop() {
  M5.update();

    if (M5.BtnA.wasPressed()) {
        Serial.println("Button A pressed");
    }

    if (M5.BtnB.wasPressed()) {
        Serial.println("Button B pressed");
    }

    // Pin chỉ cần đọc mỗi giây
    if (millis() - lastBatteryUpdate >= 1000) {
        lastBatteryUpdate = millis();
        updateBatteryData();
    }

    // Giao diện cập nhật 10 lần/giây
    if (millis() - lastFrameUpdate >= 100) {
        lastFrameUpdate = millis();
        drawStatus();
    }

    delay(5);
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}