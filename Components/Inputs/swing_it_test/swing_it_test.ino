#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_LSM6DSOX.h>
#include <EEPROM.h>

// OLED & Game pins
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1
#define SWITCH_PIN        2
#define MODE_BTN_PIN      3
#define BUZZER_PIN        5
#define SHEATH_PIN        6

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Rotary encoder pins
const uint8_t ENC_CLK = 9;
const uint8_t ENC_DT  = 8;

// RGB LED pins
const uint8_t RED_PIN   = 11;
const uint8_t BLUE_PIN  = 12;
const uint8_t GREEN_PIN = 13;

// encoder state
long encPosition = 0;
int  lastEncCLK  = LOW;

// Swing threshold (fixed)
const float SWING_DELTA_THRESHOLD = 0.8;

// Color palette
const uint8_t colorTable[][3] = {
  {255,   0,   0}, {  0,   0, 255}, {  0, 255,   0},
  {255, 255,   0}, {  0, 255, 255}, {255,   0, 255}, {255, 255, 255}
};
const uint8_t NUM_COLORS = sizeof(colorTable) / sizeof(colorTable[0]);

// EEPROM & Game state
const int EEPROM_ADDR_HIGH = 0;
int highScore = 0;
bool oledOn = false;
uint8_t modeCount = 1;
int lastModeState = HIGH;

// IMU state for swing detection
float lastX = 0, lastY = 0, lastZ = 0;

// Power toggle
bool checkPowerToggle() {
  static int last = HIGH;
  int cur = digitalRead(SWITCH_PIN);
  if (cur != last) {
    delay(50);
    cur = digitalRead(SWITCH_PIN);
    if (cur != last && cur == LOW) {
      last = cur;
      return true;
    }
  }
  last = cur;
  return false;
}

// Rotary encoder update
void updateEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_DT) != clk) encPosition++;
    else                            encPosition--;
  }
  lastEncCLK = clk;
}

// Set RGB LED color
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(RED_PIN,   r > 128 ? HIGH : LOW);
  digitalWrite(GREEN_PIN, g > 128 ? HIGH : LOW);
  digitalWrite(BLUE_PIN,  b > 128 ? HIGH : LOW);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  display.clearDisplay();
  display.display();
  delay(100);

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  EEPROM.get(EEPROM_ADDR_HIGH, highScore);

  pinMode(SWITCH_PIN,     INPUT_PULLUP);
  pinMode(MODE_BTN_PIN,   INPUT_PULLUP);
  pinMode(BUZZER_PIN,     OUTPUT);
  pinMode(SHEATH_PIN,     INPUT_PULLUP);
  digitalWrite(BUZZER_PIN,   LOW);

  pinMode(ENC_CLK,   INPUT);
  pinMode(ENC_DT,    INPUT);
  lastEncCLK = digitalRead(ENC_CLK);

  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  setRGB(0,0,0);

  randomSeed(analogRead(A5));

  // Only print threshold once at startup
  Serial.print("Swing threshold: ");
  Serial.println(SWING_DELTA_THRESHOLD, 2);
}

void loop() {
  if (checkPowerToggle()) {
    oledOn = !oledOn;
    Serial.println(oledOn ? "OLED ON" : "OLED OFF");
    if (oledOn)      powerUpAndSelectMode();
    else             powerDownOLED();
  }
}

// Menu/intro
void powerUpAndSelectMode() {
  delay(10);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Bop-It!");
  display.setTextSize(1);
  display.setCursor(0,32);
  display.display();

  unsigned long t0 = millis();
  while (millis() - t0 < 2000) {
    if (checkPowerToggle()) {
      oledOn = false;
      powerDownOLED();
      return;
    }
  }

  modeCount = 1;
  showMenu();
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();

  while (millis() - lastPress < 3000) {
    if (checkPowerToggle()) {
      oledOn = false;
      powerDownOLED();
      return;
    }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(50);
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = (modeCount % 3) + 1;
        Serial.print("Mode = ");
        Serial.println(modeCount);
        showMenu();
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if      (modeCount == 1) display.println("Easy Mode");
  else if (modeCount == 2) display.println("Medium Mode");
  else                     display.println("Hard Mode");
  display.display();

  t0 = millis();
  while (millis() - t0 < 2000) {
    if (checkPowerToggle()) {
      oledOn = false;
      powerDownOLED();
      return;
    }
  }

  runGame();
}

// Only prints when required for the "Swing It!" prompt
bool isSwingDetected(bool showDebug) {
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);

    float deltaX = abs(x - lastX);
    float deltaY = abs(y - lastY);
    float deltaZ = abs(z - lastZ);

    lastX = x;
    lastY = y;
    lastZ = z;

    bool swung = (deltaX > SWING_DELTA_THRESHOLD ||
                  deltaY > SWING_DELTA_THRESHOLD ||
                  deltaZ > SWING_DELTA_THRESHOLD);

    if (showDebug) {
      // Serial.print("[IMU] dX: "); Serial.print(deltaX, 2);
      // Serial.print(" dY: "); Serial.print(deltaY, 2);
      // Serial.print(" dZ: "); Serial.print(deltaZ, 2);
      // Serial.print(" | Swung: ");
      Serial.println(swung ? "YES" : "NO");
    }

    return swung;
  }
  return false;
}

void runGame() {
  unsigned long interval = (modeCount==1 ? 5000 :
                            modeCount==2 ? 4000 : 3000);
  unsigned long lastCueTime = millis();
  int score = 0;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Score: ");
  display.println(score);
  display.display();

  while (oledOn) {
    updateEncoder();
    if (checkPowerToggle()) { powerDownOLED(); return; }

    if (millis() - lastCueTime >= interval) {
      lastCueTime = millis();
      uint8_t cmd = random(1,4);
      const char* txt = (cmd==1 ? "Swing It!" :
                         cmd==2 ? "Sheath It!" :
                                  "Boost It!");

      // Show prompt and score (always clear first!)
      setRGB(0,0,0);
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print("Score: ");
      display.println(score);
      display.setTextSize(2);
      display.setCursor(0,16);
      display.println(txt);
      display.display();

      // cue beeps
      for (uint8_t i = 0; i < cmd; i++) {
        if (checkPowerToggle()) { powerDownOLED(); return; }
        tone(BUZZER_PIN, 2000, 200);
        unsigned long t0 = millis();
        while (millis() - t0 < 200) if (checkPowerToggle()) { powerDownOLED(); return; }
        unsigned long t1 = millis();
        while (millis() - t1 < 250) if (checkPowerToggle()) { powerDownOLED(); return; }
      }
      noTone(BUZZER_PIN);

      // Input phase
      bool swung    = false;
      bool sheathed = false;
      bool boosted  = false;
      long  baseEnc = encPosition;
      bool  lastSheathState = (digitalRead(SHEATH_PIN)   == LOW);
      bool  lastSwingState  = false;

      unsigned long start = millis();
      bool correct = false;

      while (millis() - start < interval) {
        updateEncoder();
        if (checkPowerToggle()) { powerDownOLED(); return; }

        bool curSheath = (digitalRead(SHEATH_PIN) == LOW);
        bool curSwing  = (cmd == 1) ? isSwingDetected(true) : isSwingDetected(false);
        long curEnc   = encPosition;

        // sheath edge
        if (!sheathed && !lastSheathState && curSheath) {
          sheathed = true;
          if (cmd == 2) {
            correct = true;
          } else {
            setRGB(0,0,0);
            tone(BUZZER_PIN, 400, 200);
            showGameOver(score);
            return;
          }
        }

        // swing edge
        if (!swung && !lastSwingState && curSwing) {
          swung = true;
          if (cmd == 1) {
            correct = true;
          } else {
            setRGB(0,0,0);
            tone(BUZZER_PIN, 400, 200);
            showGameOver(score);
            return;
          }
        }

        // boost edge
        if (!boosted && curEnc != baseEnc) {
          boosted = true;
          if (cmd == 3) {
            int idx = (score / 10) % NUM_COLORS;
            auto &c = colorTable[idx];
            setRGB(c[0], c[1], c[2]);
            correct = true;
          } else {
            setRGB(0,0,0);
            tone(BUZZER_PIN, 400, 200);
            showGameOver(score);
            return;
          }
        }

        if (correct) break;

        lastSheathState = curSheath;
        lastSwingState  = curSwing;

        delay(10); // Keeps loop responsive, avoids resource overload
      }

      // timeout with no correct input
      if (!correct) {
        setRGB(0,0,0);
        tone(BUZZER_PIN, 400, 200);
        showGameOver(score);
        return;
      }

      score++;
      if(score>=99){
        setRGB(0,0,0);
        tone(BUZZER_PIN,400,200);
        showGameOver(score);
        return;
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print("Score: ");
      display.println(score);
      display.display();
      delay(200);
    }
  }
}

void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Select Mode:");
  for (uint8_t i = 1; i <= 3; i++) {
    display.setCursor(0, 16 + (i-1)*12);
    display.print(i == modeCount ? "> " : "  ");
    display.print(i);
    display.print(") ");
    display.println(i == 1 ? "Easy" : i == 2 ? "Medium" : "Hard");
  }
  display.display();
}

void showGameOver(int finalScore) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("GAME OVER");

  display.setTextSize(1);
  display.setCursor(0,32);
  display.print("Score: ");
  display.println(finalScore);

  if (finalScore > highScore) {
    highScore = finalScore;
    EEPROM.put(EEPROM_ADDR_HIGH, highScore);
    Serial.println("New High Score!");
  }
  display.setCursor(0,44);
  display.print("High: ");
  display.println(highScore);

  display.display();
  // Serial.print("Final Score: "); Serial.println(finalScore);
  // Serial.print("High Score: ");  Serial.println(highScore);

  unsigned long t0 = millis();
  while (millis() - t0 < 4000) {
    if (checkPowerToggle()) {
      oledOn = false;
      powerDownOLED();
      return;
    }
  }

  // menu after Game Over
  uint8_t option = 1;
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();
  showGameOverMenu(option);
  while (millis() - lastPress < 5000) {
    if (checkPowerToggle()) {
      oledOn = false;
      return;
    }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(50);
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        option = (option % 2) + 1;
        showGameOverMenu(option);
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

  if (option == 1)       runGame();
  else                   powerUpAndSelectMode();
}

void showGameOverMenu(uint8_t opt) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Select Option:");
  display.setCursor(0,16);
  display.print(opt == 1 ? "> " : "  "); display.println("Play Again");
  display.setCursor(0,28);
  display.print(opt == 2 ? "> " : "  "); display.println("Mode Select");
  display.display();
}

void powerDownOLED() {
  display.clearDisplay();
  display.display();
}
