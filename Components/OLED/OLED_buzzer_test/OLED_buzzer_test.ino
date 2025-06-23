#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH      128
#define OLED_HEIGHT      64
#define OLED_RESET       -1    // not used
#define SWITCH_PIN       2     // START button
#define MODE_BTN_PIN     3
#define OLED_POWER_PIN   4
#define BUZZER_PIN       5

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

int  lastPowerState = HIGH;
int  lastModeState  = HIGH;
bool oledOn         = false;
uint8_t modeCount   = 1;      // 1=Easy,2=Medium,3=Hard

void setup() {
  Serial.begin(9600);
  pinMode(SWITCH_PIN,    INPUT_PULLUP);
  pinMode(MODE_BTN_PIN,  INPUT_PULLUP);
  pinMode(OLED_POWER_PIN,OUTPUT);
  pinMode(BUZZER_PIN,    OUTPUT);
  digitalWrite(OLED_POWER_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  display.clearDisplay();
  display.display();
  randomSeed(analogRead(A5));
}

void loop() {
  int p = digitalRead(SWITCH_PIN);
  if (p != lastPowerState) {
    delay(50);
    p = digitalRead(SWITCH_PIN);
    if (p != lastPowerState && p == LOW) {
      oledOn = !oledOn;
      Serial.println(oledOn ? "OLED ON" : "OLED OFF");
      if (oledOn)      powerUpAndSelectMode();
      else              powerDownOLED();
    }
  }
  lastPowerState = p;
}

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

  // Mode menu
  modeCount = 1;
  showMenu();
  unsigned long lastPress = millis();

  // Cycle until 2s inactivity or OFF pressed
  while (millis() - lastPress < 2000) {
    if (!digitalRead(SWITCH_PIN)) {
      Serial.println("OLED OFF during mode select");
      oledOn = false;
      powerDownOLED();
      return;
    }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(50);
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = modeCount % 3 + 1;
        Serial.print("Mode = "); Serial.println(modeCount);
        showMenu();
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

  // Confirm selection for 2s with OFF check
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if      (modeCount==1) display.println("Easy Mode");
  else if (modeCount==2) display.println("Medium Mode");
  else                   display.println("Hard Mode");
  display.display();

  unsigned long confirmStart = millis();
  while (millis() - confirmStart < 2000) {
    if (!digitalRead(SWITCH_PIN)) {
      Serial.println("OLED OFF during confirm screen");
      oledOn = false;
      powerDownOLED();
      return;
    }
  }

  runGame();
}

void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Select Mode:");
  for (uint8_t i = 1; i <= 3; i++) {
    display.setCursor(0, 16 + (i-1)*12);
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

void runGame() {
  unsigned long interval;
  switch (modeCount) {
    case 1: interval = 5000; break;
    case 2: interval = 4000; break;
    case 3: interval = 3000; break;
  }
  unsigned long lastCueTime = millis();

  while (oledOn) {
    if (!digitalRead(SWITCH_PIN)) {
      Serial.println("OLED OFF during game");
      oledOn = false;
      break;
    }
    if (millis() - lastCueTime >= interval) {
      lastCueTime = millis();
      uint8_t cmd = random(1,4);
      const char* txt = (cmd==1?"Swing It!":cmd==2?"Sheath It!":"Boost It!");
      Serial.print("Command: "); Serial.println(txt);

      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,16);
      display.println(txt);
      display.display();

      for (uint8_t i=0; i<cmd; i++) {
        tone(BUZZER_PIN, 2000, 200);
        delay(250);
      }
      noTone(BUZZER_PIN);
    }
  }
  powerDownOLED();
}

void powerDownOLED() {
  display.clearDisplay();
  display.display();
  digitalWrite(OLED_POWER_PIN, LOW);
}
