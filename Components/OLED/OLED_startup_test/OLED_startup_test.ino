#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH      128
#define OLED_HEIGHT      64
#define OLED_RESET       -1    // not used
#define SWITCH_PIN       2
#define MODE_BTN_PIN     3
#define OLED_POWER_PIN   4

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

int  lastPowerState = HIGH;
int  lastModeState  = HIGH;
bool oledOn         = false;
uint8_t modeCount   = 1;      // 1=Easy,2=Medium,3=Hard

void setup() {
  Serial.begin(9600);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);
  pinMode(OLED_POWER_PIN, OUTPUT);
  digitalWrite(OLED_POWER_PIN, LOW);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while(1);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  int p = digitalRead(SWITCH_PIN);
  if (p != lastPowerState) {
    delay(50);
    p = digitalRead(SWITCH_PIN);
    if (p != lastPowerState && p == LOW) {
      oledOn = !oledOn;
      Serial.print("OLED ");
      Serial.println(oledOn ? "ON" : "OFF");
      if (oledOn) powerUpAndSelectMode();
      else          powerDownOLED();
    }
  }
  lastPowerState = p;
}

// Toggle OLED on, show title, then cycle modes until idle 2s
void powerUpAndSelectMode() {
  digitalWrite(OLED_POWER_PIN, HIGH);
  delay(10);

  // Title
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Bop-It!");
  display.display();
  delay(2000);

  // Initial menu
  modeCount = 1;
  unsigned long lastPress = millis();
  showMenu();

  // Cycle until 2s of no presses
  while (millis() - lastPress < 2000) {
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(50);
      m = digitalRead(MODE_BTN_PIN);
      if (m != lastModeState && m == LOW) {
        modeCount = (modeCount % 3) + 1;
        Serial.print("Mode = "); Serial.println(modeCount);
        showMenu();
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

  // Final selection display
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if (modeCount == 1) display.println("Easy Mode");
  else if (modeCount == 2) display.println("Medium Mode");
  else                    display.println("Hard Mode");
  display.display();
}

// Draw the mode menu with current highlight
void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Select Mode:");
  for (uint8_t i = 1; i <= 3; i++) {
    display.setCursor(0, 16 + (i-1)*10);
    display.print(i == modeCount ? "> " : "  ");
    display.print(i); display.print(") ");
    switch(i) {
      case 1: display.println("Easy");   break;
      case 2: display.println("Medium"); break;
      case 3: display.println("Hard");   break;
    }
  }
  display.display();
}

void powerDownOLED() {
  display.clearDisplay();
  display.display();
  digitalWrite(OLED_POWER_PIN, LOW);
}
