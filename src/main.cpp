#include <Arduino.h>
#include <M5Unified.h>

// put function declarations here:
int myFunction(int, int);

unsigned long lastRefresh = 0; 

void setup() {
  // put your setup code here, to run once:
  auto cfg = M5.config();
  M5.begin();
  //chua cap nguon 5V 
  M5.Power.setExtOutput(false); 

  Serial.begin(115200);
  delay(500);

  M5.Display.setRotation(1); 
  M5.Display.fillScreen(BLACK); 

  M5.Display.setTextColor(GREEN); 
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 20);
  M5.Display.println("VitalCare");

  M5.Display.setTextColor(WHITE); 
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 55);
  M5.Display.println("M5Stick: OK");

  M5.Display.setCursor(10, 75);
  M5.Display.println("PlatformIO Test.");

  Serial.println("VitalCare boot completed.");
  Serial.println("M5StickS3 display intialized.");
}

void loop() {
  // put your main code here, to run repeatedly:
  M5.update();
  delay(10); 
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}