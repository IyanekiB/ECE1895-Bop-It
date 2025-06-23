#include <Wire.h>
#include <Adafruit_GFX.h>              // Graphics library for OLED
#include <Adafruit_SSD1306.h>          // OLED driver
#include <Arduino_LSM6DSOX.h>          // IMU (accelerometer/gyro)
#include <EEPROM.h>                    // EEPROM for persistent storage
#include <Adafruit_NeoPixel.h>         // NeoPixel library for LED strip

// ------------------- Pin and Device Config -------------------
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1            // OLED reset (unused)
#define SWITCH_PIN        2            // Power switch pin
#define MODE_BTN_PIN      3            // Mode select button pin
#define BUZZER_PIN        5            // Buzzer pin
#define SHEATH_PIN        6            // Sheath action button pin

#define NEOPIXEL_PIN     7             // Data pin for NeoPixel LED strip
#define NUM_LEDS         30            // Number of LEDs in the strip

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Rotary encoder pins and state
const uint8_t ENC_CLK = 9, ENC_DT = 8;
long encPosition = 0;                  // Tracks the encoder position
int lastEncCLK = LOW;                  // Last CLK state

// IMU motion threshold for "swing" action
const float SWING_DELTA_THRESHOLD = 1.5;

// Game state variables
const int EEPROM_ADDR_HIGH = 0;        // EEPROM address for high score
int highScore = 0;                     // Highest score
bool oledOn = false;                   // OLED power state
uint8_t modeCount = 1;                 // Selected game mode
int lastModeState = HIGH;              // For button debouncing

// Previous IMU readings (used for swing detection)
float lastX = 0, lastY = 0, lastZ = 0;

// LED Helpers

// Turn off all NeoPixels
void stripOff() {
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, 0);
  strip.show();
}

// Flash the whole strip red for error/game over
void flashRed(uint16_t ms = 350) {
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  strip.show();
  delay(ms);
  stripOff();
}

// Green wave animation for correct swing/sheath input
void movingGreenWave(uint16_t duration_ms = 750) {
  uint8_t frames = 24;
  uint8_t frameDelay = duration_ms / frames;
  for (uint8_t f = 0; f < frames; f++) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      int d = abs((int)i - (f * NUM_LEDS / frames));
      uint8_t val = (d < 4) ? 200 - 45 * d : 0;
      strip.setPixelColor(i, 0, val, 0);
    }
    strip.show();
    delay(frameDelay);
  }
  stripOff();
}

// Light up a growing segment of the strip in a rainbow gradient
void showRainbowGrowing(int count) {
  count = constrain(count, 0, NUM_LEDS);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (i < count) {
      uint16_t hue = (uint32_t)i * 65535UL / (count > 1 ? count - 1 : 1);
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue)));
    } else {
      strip.setPixelColor(i, 0); // Off
    }
  }
  strip.show();
}

// Input and Game Utils

// Debounced check for power toggle switch
bool checkPowerToggle() {
  static int last = HIGH;
  int cur = digitalRead(SWITCH_PIN);
  if (cur != last) {
    delay(40);
    cur = digitalRead(SWITCH_PIN);
    if (cur != last && cur == LOW) {
      last = cur;
      return true;
    }
  }
  last = cur;
  return false;
}

// Update the encoder position value
void updateEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_DT) != clk) encPosition++;
    else                            encPosition--;
  }
  lastEncCLK = clk;
}

// Main Setup/Loop

void setup() {
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); display.display();

  IMU.begin();
  EEPROM.get(EEPROM_ADDR_HIGH, highScore);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SHEATH_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(ENC_CLK, INPUT);
  pinMode(ENC_DT, INPUT);
  lastEncCLK = digitalRead(ENC_CLK);

  strip.begin();
  strip.setBrightness(80);
  stripOff();

  randomSeed(analogRead(A5));
}

void loop() {
  // Wait for power toggle switch to turn game on/off
  if (checkPowerToggle()) {
    oledOn = !oledOn;
    if (oledOn)      powerUpAndSelectMode();
    else             powerDownOLED();
  }
}

// UI & Menu Logic

// Show mode selection and get mode from user
void powerUpAndSelectMode() {
  delay(7);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(F("Bop-It!"));
  display.setTextSize(1);
  display.setCursor(0,32);
  display.display();

  // Show splash for 1.6 seconds
  unsigned long t0 = millis();
  while (millis() - t0 < 1600) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  }

  modeCount = 1;
  showMenu();
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();

  // Wait for up to 2.3s for mode change button
  while (millis() - lastPress < 2300) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(28); // Debounce
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = (modeCount % 3) + 1; // Cycle through modes
        showMenu();
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

  // Show chosen mode for 1.3s
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if      (modeCount == 1) display.println(F("Easy Mode"));
  else if (modeCount == 2) display.println(F("Medium Mode"));
  else                     display.println(F("Hard Mode"));
  display.display();

  t0 = millis();
  while (millis() - t0 < 1300) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  }

  runGame();
}

// Detects a swing movement using IMU readings
bool isSwingDetected() {
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    float dx = abs(x - lastX), dy = abs(y - lastY), dz = abs(z - lastZ);
    lastX = x; lastY = y; lastZ = z;
    return (dx > SWING_DELTA_THRESHOLD || dy > SWING_DELTA_THRESHOLD || dz > SWING_DELTA_THRESHOLD);
  }
  return false;
}

// GAME LOGIC

void runGame() {
  // Set timing interval based on difficulty
  unsigned long interval = (modeCount==1 ? 4700 : modeCount==2 ? 3800 : 2800);
  unsigned long lastCueTime = millis();
  int score = 0;

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(F("Score: "));
  display.println(score);
  display.display();

  while (oledOn) {
    updateEncoder();
    if (checkPowerToggle()) { powerDownOLED(); return; }

    if (millis() - lastCueTime >= interval) {
      lastCueTime = millis();
      uint8_t cmd = random(1,4); // 1=Swing, 2=Sheath, 3=Boost
      const char* txt = (cmd==1 ? "Swing It!" : cmd==2 ? "Sheath It!" : "Boost It!");

      // Display the current prompt and score
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print(F("Score: "));
      display.println(score);
      display.setTextSize(2);
      display.setCursor(0,16);
      display.println(txt);
      display.display();

      // Beep sequence for cue
      stripOff();
      for (uint8_t i = 0; i < cmd; i++) {
        if (checkPowerToggle()) { powerDownOLED(); return; }
        tone(BUZZER_PIN, 2000, 175);
        unsigned long t0 = millis();
        while (millis() - t0 < 175) if (checkPowerToggle()) { powerDownOLED(); return; }
        unsigned long t1 = millis();
        while (millis() - t1 < 180) if (checkPowerToggle()) { powerDownOLED(); return; }
      }
      noTone(BUZZER_PIN);

      long baseEnc = encPosition;
      long maxDelta = 0;    // Store furthest clockwise movement
      bool correct = false;
      bool lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
      bool lastSwingState = false;
      unsigned long start = millis();

      // BOOST IT (encoder rainbow)
      if (cmd == 3) {
        long prevLit = 0;
        bool encoderMoved = false;
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          long curEnc = encPosition;
          // Count only maximum positive movement
          int delta = abs(curEnc - baseEnc);     // Only clockwise increases
          if (delta > maxDelta) maxDelta = delta;

          int num_lit = maxDelta / 2;       // Every 2 ticks lights 1 LED
          num_lit = constrain(num_lit, 0, NUM_LEDS);

          // Update LEDs only if lit count changes
          if (num_lit != prevLit) {
            showRainbowGrowing(num_lit);
            prevLit = num_lit;
          }

          if (num_lit > 0) encoderMoved = true;

          delay(12);
        }
        if (!encoderMoved) { 
          flashRed(); 
          tone(BUZZER_PIN, 400, 200); 
          showGameOver(score); 
          return; 
        }
        else correct = true;
        stripOff();
      } 
      // SWING/SHEATH input (motion/buttons)
      else if (cmd == 1) { // Swing
          long swingBaseEnc = encPosition; // Store encoder at start
          bool curSwing, lastSwingState = false;
          bool movedEncoder = false;
          while (millis() - start < interval) {
            updateEncoder();
            if (checkPowerToggle()) { powerDownOLED(); return; }

            // Penalize any encoder movement!
            if (encPosition != swingBaseEnc) {
              flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
            }

            curSwing = isSwingDetected();
            if (!lastSwingState && curSwing) {
              movingGreenWave();
              correct = true;
              break;
            }
            lastSwingState = curSwing;
            if (digitalRead(SHEATH_PIN) == LOW) {
              flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
            }
            delay(12);
          }
      }
      else if (cmd == 2) { // Sheath
          long sheathBaseEnc = encPosition; // Store encoder at start
          bool curSheath, lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
          while (millis() - start < interval) {
            updateEncoder();
            if (checkPowerToggle()) { powerDownOLED(); return; }

            // Penalize any encoder movement!
            if (encPosition != sheathBaseEnc) {
              flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
            }

            curSheath = (digitalRead(SHEATH_PIN) == LOW);
            if (!lastSheathState && curSheath) {
              movingGreenWave();
              correct = true;
              break;
            }
            lastSheathState = curSheath;
            if (isSwingDetected()) {
              flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
            }
            delay(12);
          }
      }

      // Timeout or incorrect input: fail
      if (!correct) { 
        flashRed(); 
        tone(BUZZER_PIN, 400, 200); 
        showGameOver(score); 
        return; 
      }

      // Success: update score, animate, and continue
      score++;
      if(score>=99){
        stripOff();
        tone(BUZZER_PIN,400,200);
        showGameOver(score);
        return;
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print(F("Score: "));
      display.println(score);
      display.display();
      delay(140);
      stripOff();
    }
  }
}

// UI helpers

// Show mode selection menu
void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(F("Select Mode:"));
  for (uint8_t i = 1; i <= 3; i++) {
    display.setCursor(0, 16 + (i-1)*12);
    display.print(i == modeCount ? F("> ") : F("  "));
    display.print(i);
    display.print(F(") "));
    display.println(i == 1 ? F("Easy") : i == 2 ? F("Medium") : F("Hard"));
  }
  display.display();
}

// Show game over screen and update high score
void showGameOver(int finalScore) {
  flashRed();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(F("GAME OVER"));
  display.setTextSize(1);
  display.setCursor(0,32);
  display.print(F("Score: "));
  display.println(finalScore);

  // Update high score if beaten
  if (finalScore > highScore) {
    highScore = finalScore;
    EEPROM.put(EEPROM_ADDR_HIGH, highScore);
  }
  display.setCursor(0,44);
  display.print(F("High: "));
  display.println(highScore);
  display.display();

  unsigned long t0 = millis();
  while (millis() - t0 < 3300) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  }
  oledOn = false;
  powerDownOLED();
}

// Power down sequence: clear LEDs and OLED
void powerDownOLED() {
  stripOff();
  display.clearDisplay();
  display.display();
}
