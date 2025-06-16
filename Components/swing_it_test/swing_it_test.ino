#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM6DSO32.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <math.h>

// ───────────────────────────────────────────────
// OLED & Game pins
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1    // unused
#define SWITCH_PIN        2    // START / power toggle
#define MODE_BTN_PIN      3    // mode select
#define OLED_POWER_PIN    4
#define BUZZER_PIN        5
#define SHEATH_PIN        6    // Sheath It! digital

// For SPI mode, we need a CS pin
#define LSM_CS A3

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// ───────────────────────────────────────────────
// I²C accel (replaces SWING_PIN)
Adafruit_LSM6DSO32 lsm6ds;

// threshold for swing detection (m/s² beyond gravity)
#define ACCEL_THRESHOLD 5.0  

// ───────────────────────────────────────────────
// Rotary encoder pins & state
const uint8_t ENC_CLK = 9;
const uint8_t ENC_DT  = 8;
long encPosition      = 0;
int  lastEncCLK       = LOW;

// RGB LED pins (common‐cathode)
const uint8_t RED_PIN   = 11;
const uint8_t BLUE_PIN  = 12;
const uint8_t GREEN_PIN = 13;
const uint8_t colorTable[][3] = {
  {255,   0,   0},  // red
  {  0,   0, 255},  // blue
  {  0, 255,   0},  // green
  {255, 255,   0},  // yellow
  {  0, 255, 255},  // cyan
  {255,   0, 255},  // magenta
  {255, 255, 255},  // white
};
const uint8_t NUM_COLORS = sizeof(colorTable) / sizeof(colorTable[0]);

// ───────────────────────────────────────────────
// High‐score persistence
const int EEPROM_ADDR_HIGH = 0;
int highScore = 0;

// ───────────────────────────────────────────────
// Game state
bool    oledOn        = false;
uint8_t modeCount     = 1;    // 1=Easy,2=Medium,3=Hard
int     lastModeState = HIGH;

// ───────────────────────────────────────────────
// Debounced falling‐edge detector for SWITCH_PIN
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

// ───────────────────────────────────────────────
// Update encoder position; call frequently
void updateEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_DT) != clk) encPosition++;
    else                            encPosition--;
  }
  lastEncCLK = clk;
}

// ───────────────────────────────────────────────
// Drive common‐cathode RGB at full on/off
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(RED_PIN,   r > 128 ? HIGH : LOW);
  digitalWrite(GREEN_PIN, g > 128 ? HIGH : LOW);
  digitalWrite(BLUE_PIN,  b > 128 ? HIGH : LOW);
}

// ───────────────────────────────────────────────
// Read accelerometer, compute |Δ| from 1g, return true if it exceeds threshold
bool detectSwing() {
  sensors_event_t accel;
  lsm6ds.getEvent(&accel, nullptr, nullptr);
  float mx = accel.acceleration.x;
  float my = accel.acceleration.y;
  float mz = accel.acceleration.z;
  float mag = sqrt(mx*mx + my*my + mz*mz);
  float delta = fabs(mag - 9.8f);  // remove gravity
  return (delta > ACCEL_THRESHOLD);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();  // initialize I²C bus

  // order is: CS, SCK, MOSI (SDA), MISO (DO)
  if (! lsm6ds.begin_SPI(A3, A0, A1, A2) ) {
    Serial.println("ERROR: LSM6DS bit-bang SPI failed!");
    while (1) delay(10);
  }

  // choose ranges (optional)
  lsm6ds.setAccelRange(LSM6DS_ACCEL_RANGE_2_G);
  lsm6ds.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  Serial.println("LSM6DS302 initialized.");

  // Load high score
  EEPROM.get(EEPROM_ADDR_HIGH, highScore);
  Serial.print("Loaded High Score: ");
  Serial.println(highScore);

  // Initialize pins
  pinMode(SWITCH_PIN,     INPUT_PULLUP);
  pinMode(MODE_BTN_PIN,   INPUT_PULLUP);
  pinMode(OLED_POWER_PIN, OUTPUT);
  pinMode(BUZZER_PIN,     OUTPUT);
  pinMode(SHEATH_PIN,     INPUT_PULLUP);
  digitalWrite(OLED_POWER_PIN, LOW);
  digitalWrite(BUZZER_PIN,   LOW);
  delay(50);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  display.clearDisplay();
  display.display();

  // Encoder & RGB
  pinMode(ENC_CLK,   INPUT);
  pinMode(ENC_DT,    INPUT);
  lastEncCLK = digitalRead(ENC_CLK);
  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  setRGB(0,0,0);

  // use a floating analog pin for random seed
  randomSeed(analogRead(A0));
}

void loop() {
  if (checkPowerToggle()) {
    oledOn = !oledOn;
    if (oledOn)      powerUpAndSelectMode();
    else             powerDownOLED();
  }
}

void powerUpAndSelectMode() {
  digitalWrite(OLED_POWER_PIN, HIGH);
  delay(10);

  // Title screen (interruptible)
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Bop-It!");
  display.display();
  {
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      if (checkPowerToggle()) {
        oledOn = false;
        powerDownOLED();
        return;
      }
    }
  }

  // Mode select: cycle modes, auto‐confirm after 3s
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

  // Confirm screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if      (modeCount == 1) display.println("Easy Mode");
  else if (modeCount == 2) display.println("Medium Mode");
  else                     display.println("Hard Mode");
  display.display();
  {
    unsigned long t0 = millis();
    while (millis() - t0 < 2000) {
      if (checkPowerToggle()) {
        oledOn = false;
        powerDownOLED();
        return;
      }
    }
  }

  runGame();
}

void runGame() {
  unsigned long interval = (modeCount==1 ? 5000 :
                            modeCount==2 ? 4000 : 3000);
  unsigned long lastCueTime = millis();
  int score = 98;

  // initial score display
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

      // show prompt + live score
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

      // set up edge flags + baselines
      bool swung    = false;
      bool sheathed = false;
      bool boosted  = false;
      long  baseEnc = encPosition;
      bool  lastSheathState = (digitalRead(SHEATH_PIN)   == LOW);
      bool  lastSwingState  = detectSwing(); // (digitalRead(SWING_PIN)    > SWING_THRESHOLD);

      unsigned long start = millis();
      bool correct = false;

      // wait for input or timeout
      while (millis() - start < interval) {
        updateEncoder();
        if (checkPowerToggle()) { powerDownOLED(); return; }

        bool curSheath = (digitalRead(SHEATH_PIN) == LOW);
        bool curSwing  = detectSwing();
        long  curEnc   = encPosition;

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
            // ← your color-cycling on correct Boost
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
      }

      // timeout with no correct input
      if (!correct) {
        setRGB(0,0,0);
        tone(BUZZER_PIN, 400, 200);
        showGameOver(score);
        return;
      }

      // correct → increment + update display
      score++;
      if(score>=99){
        // reached 99+ → end game
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
  Serial.print("Final Score: "); Serial.println(finalScore);
  Serial.print("High Score: ");  Serial.println(highScore);

  // linger
  {
    unsigned long t0 = millis();
    while (millis() - t0 < 4000) {
      if (checkPowerToggle()) {
        oledOn = false;
        powerDownOLED();
        return;
      }
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
  digitalWrite(OLED_POWER_PIN, LOW);
}
