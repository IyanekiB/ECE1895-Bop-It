#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_RESET    -1     // you can leave -1 if you don't have a reset pin

#define SWITCH_PIN    2
#define OLED_POWER_PIN 4    // <— controls VCC to the OLED

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

int  lastSwitchState = HIGH;
bool oledState       = false; // off

void setup() {
  Serial.begin(9600);

  // Switch
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // OLED power control
  pinMode(OLED_POWER_PIN, OUTPUT);
  digitalWrite(OLED_POWER_PIN, LOW);  // keep OLED off

  // Init (but won't show until powered on)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed"));
    while(1);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  int switchState = digitalRead(SWITCH_PIN);

  // simple edge‐detect + debounce
  if (switchState != lastSwitchState) {
    delay(50);
    switchState = digitalRead(SWITCH_PIN);
    if (switchState != lastSwitchState && switchState == LOW) {
      oledState = !oledState;
      Serial.print("OLED toggled to ");
      Serial.println(oledState ? "ON" : "OFF");

      if (oledState) {
        // power up + show a message
        digitalWrite(OLED_POWER_PIN, HIGH);
        delay(10);             // give the display time to wake
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0,0);
        display.println("OLED ON");
        display.display();
      } else {
        // clear and power down
        display.clearDisplay();
        display.display();
        digitalWrite(OLED_POWER_PIN, LOW);
      }
    }
  }

  lastSwitchState = switchState;
}
