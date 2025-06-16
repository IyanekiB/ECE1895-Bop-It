#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_LSM6DSOX.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialize OLED first
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  Serial.println("OLED initialized");

  delay(100);

  // Now initialize IMU
  if (!IMU.begin()) {
    Serial.println("IMU failed");
    while (1);
  }
  Serial.println("IMU initialized");

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Both OK!");
  display.display();
}

void loop() {
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    Serial.print("X: "); Serial.print(x);
    Serial.print(", Y: "); Serial.print(y);
    Serial.print(", Z: "); Serial.println(z);
  }
  delay(500);
}
