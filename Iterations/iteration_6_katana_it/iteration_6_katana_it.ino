#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <MPU6050.h>

// Pinout
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1

#define SWITCH_PIN        2
#define MODE_BTN_PIN      3
#define SHEATH_PIN        6

#define NEOPIXEL_PIN      7
#define NUM_LEDS         30   // Change this as well after debugging

#define ENC_A             5   // Terminal A (right-most pin when encoder is facing you)
#define ENC_B             8   // Terminal B (left-most pin when encoder is facing you)

#define FX_BOOST_PIN      9   // T0 - Boost It
#define FX_SHEATH_PIN     10  // T1 - Sheath It
#define FX_SWING_PIN      11  // T2 - Swing It
#define FX_GAME_ON        12  // T3 - Game On
#define FX_GAME_OVER      13  // T4 - Game Over

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRBW + NEO_KHZ800); // Remember to change this depending on what strip we end up using
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
MPU6050 mpu;

#define SWING_MAG_THRESHOLD 3  // G's

long encPosition = 0;
int lastEncCLK = LOW;

const int EEPROM_ADDR_HIGH = 0;
int highScore = 0;
bool oledOn = false;
uint8_t modeCount = 1;
int lastModeState = HIGH;
uint16_t boostHue = 0;

float lastX = 0, lastY = 0, lastZ = 0;
int16_t ax, ay, az;

// Adafruit FX Soundboard Trigger Helper (ACTIVE LOW)
void playSound(uint8_t fxPin) {
  pinMode(fxPin, OUTPUT);     
  digitalWrite(fxPin, HIGH);   // Ensure idle
  delay(20);
  digitalWrite(fxPin, LOW);    // Pulse LOW
  delay(250);                  // Hold LOW at least 200-300ms
  digitalWrite(fxPin, HIGH);   // Return HIGH
  delay(100);                  // Wait before next trigger
}


// NeoPixel helpers
void stripOff() {
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, 0,0,0,0);
  strip.show();
}
void flashRed(uint16_t ms = 350) {
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, 255,0,0,0);
  strip.show();
  delay(ms);
  stripOff();
}
void yellowMiddleOut(uint16_t duration_ms = 650) {
  // How many animation steps (pairs of LEDs to light up)
  int steps = NUM_LEDS / 2 + (NUM_LEDS % 2 ? 1 : 0);
  uint16_t frameDelay = duration_ms / steps;

  // Turn off all LEDs first
  stripOff();

  for (int i = 0; i < steps; i++) {
    int midLeft  = (NUM_LEDS - 1) / 2 - i;
    int midRight = NUM_LEDS / 2 + i;
    uint32_t yellow = strip.Color(255, 180, 0, 0); // Bright yellow

    if (midLeft >= 0) strip.setPixelColor(midLeft, yellow);
    if (midRight < NUM_LEDS) strip.setPixelColor(midRight, yellow);

    strip.show();
    delay(frameDelay);
  }
  // brief hold at max brightness
  delay(180);
  stripOff();
}
void blueBladeExtend(uint16_t extend_ms = 480) {
  uint16_t frameDelay = extend_ms / NUM_LEDS;
  uint32_t electricBlue = strip.Color(60, 200, 255, 0);
  stripOff();
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, electricBlue);
    strip.show();
    delay(frameDelay);
  }
}
void blueBladeRetract(uint16_t retract_ms = 420) {
  uint16_t frameDelay = retract_ms / NUM_LEDS;
  for (int i = NUM_LEDS - 1; i >= 0; i--) {
    strip.setPixelColor(i, 0,0,0,0);
    strip.show();
    delay(frameDelay);
  }
}
void showGradientBlade(int count, uint16_t baseHue) {
  count = constrain(count, 0, NUM_LEDS);
  uint16_t delta = 4000;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (i < count) {
      uint16_t thisHue = baseHue + ((uint32_t)i * delta / (count>1?count-1:1));
      uint32_t color = strip.gamma32(strip.ColorHSV(thisHue));
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8) & 0xFF;
      uint8_t b = color & 0xFF;
      strip.setPixelColor(i, r, g, b, 0);
    } else {
      strip.setPixelColor(i, 0,0,0,0);
    }
  }
  strip.show();
}

// Encoder
void updateEncoder() {
  int clk = digitalRead(ENC_A);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_B) != clk) encPosition++;
    else                           encPosition--;
  }
  lastEncCLK = clk;
}

// Power toggle
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

// OLED UI
void showSplash() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(F("Katana-It!"));
  display.display();
}
void showMenu(uint8_t highlight) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(F("Mode Select:"));
  for (uint8_t i = 1; i <= 4; i++) {
    display.setCursor(0, 18 + (i-1)*12);
    display.print(i == highlight ? F("> ") : F("  "));
    display.print(i);
    display.print(F(") "));
    switch(i) {
      case 1: display.println(F("Easy"));   break;
      case 2: display.println(F("Medium")); break;
      case 3: display.println(F("Hard"));   break;
      case 4: display.println(F("Quick"));  break;
    }
  }
  display.display();
}
void showModeConfirmation(uint8_t mode) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  switch(mode) {
    case 1: display.println(F("Easy Mode"));   break;
    case 2: display.println(F("Medium Mode")); break;
    case 3: display.println(F("Hard Mode"));   break;
    case 4: display.println(F("Quick Mode"));
            display.setTextSize(1);
            display.setCursor(0,40);
            display.println(F("Start at 90pts"));
            break;
  }
  display.display();
  delay(1300);
  display.clearDisplay(); display.display();
}
void waitForStartButton() {
  display.clearDisplay();
  display.display();
  while (digitalRead(SWITCH_PIN) == HIGH) delay(8);
  while (digitalRead(SWITCH_PIN) == LOW) delay(8);
}
void modeSelectMenu() {
  modeCount = 1;
  showMenu(modeCount);
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();
  while (1) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(25);
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = (modeCount % 4) + 1;
        showMenu(modeCount);
        lastPress = millis();
      }
    }
    lastModeState = m;
    if (millis() - lastPress > 5000) break;
    delay(10);
  }
  showModeConfirmation(modeCount);
  runGame();
}

// Setup & Loop
void setup() {
  delay(500);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); display.display();

  delay(500); // Allow time for OLED to initialize

  mpu.initialize();
  EEPROM.get(EEPROM_ADDR_HIGH, highScore);

  delay(500);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);
  pinMode(SHEATH_PIN, INPUT_PULLUP);
  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);
  lastEncCLK = digitalRead(ENC_A);

  delay(500);

  strip.begin();
  strip.setBrightness(80);
  stripOff();

  delay(500);

  // Set all FX trigger pins to OUTPUT and HIGH (idle for ACTIVE LOW)
  pinMode(FX_BOOST_PIN, OUTPUT);   digitalWrite(FX_BOOST_PIN, HIGH);
  pinMode(FX_SHEATH_PIN, OUTPUT);  digitalWrite(FX_SHEATH_PIN, HIGH);
  pinMode(FX_SWING_PIN, OUTPUT);   digitalWrite(FX_SWING_PIN, HIGH);
  pinMode(FX_GAME_ON, OUTPUT);     digitalWrite(FX_GAME_ON, HIGH);
  pinMode(FX_GAME_OVER, OUTPUT);   digitalWrite(FX_GAME_OVER, HIGH);

  randomSeed(analogRead(A5));
}

void loop() {
  waitForStartButton();

  delay(100);

  playSound(FX_GAME_ON); // Play "Game On" sound (T3, active LOW)

  showSplash();
  unsigned long splashStart = millis();
  while (millis() - splashStart < 1600) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    delay(8);
  }
  display.clearDisplay(); display.display();

  oledOn = true;

  while (oledOn) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    if (digitalRead(MODE_BTN_PIN) == LOW) {
      while (digitalRead(MODE_BTN_PIN) == LOW) delay(8);
      modeSelectMenu();
      break;
    }
    delay(8);
  }
}

// IMU and Game Logic
bool isSwingDetected() {
  mpu.getAcceleration(&ax, &ay, &az);

  float gx = ax / 16384.0;
  float gy = ay / 16384.0;
  float gz = az / 16384.0;

  float mag = sqrt(gx*gx + gy*gy + gz*gz);

  Serial.print("Accel Mag: "); Serial.println(mag); // For tuning

  // At rest, mag ~1.0; swinging sharply gives higher values
  return (mag > SWING_MAG_THRESHOLD);
}

void runGame() {
  unsigned long baseInterval;
  int score = 0;

  switch(modeCount) {
    case 1: baseInterval = 4700; score = 0;  break;
    case 2: baseInterval = 3800; score = 0;  break;
    case 3: baseInterval = 2800; score = 0;  break;
    case 4: baseInterval = 4700; score = 90; break;
  }
  // unsigned long baseInterval = (modeCount==1 ? 4700 : modeCount==2 ? 3800 : 2800);
  unsigned long interval = baseInterval;
  const unsigned long minInterval = 600; // ms

  unsigned long lastCueTime = millis();
  // int score = 0;

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

      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print(F("Score: "));
      display.println(score);
      display.setTextSize(2);
      display.setCursor(0,16);
      display.println(txt);
      display.display();

      stripOff();
      // ---- Trigger the correct sound immediately (active LOW pulse) ----
      if (cmd == 1) playSound(FX_SWING_PIN);
      else if (cmd == 2) playSound(FX_SHEATH_PIN);
      else if (cmd == 3) playSound(FX_BOOST_PIN);

      long baseEnc = encPosition;
      long maxDelta = 0;
      bool correct = false;
      bool lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
      bool lastSwingState = false;
      unsigned long start = millis();

      if (cmd == 3) { // BOOST IT
        boostHue = random(0, 65536);
        long prevLit = 0;
        bool encoderMoved = false;
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          long curEnc = encPosition;                  // Current encoder position
          int delta = abs(curEnc - baseEnc);          // How far the encoder has moved since cue
          if (delta > maxDelta) maxDelta = delta;     // Track maximum distance for this round
          int num_lit = (maxDelta / 2) * 2;
          num_lit = constrain(num_lit, 0, NUM_LEDS);  // For every 2 encoder ticks, light up 2 LEDs and clamp to LED strip length
          if (num_lit != prevLit) {
            showGradientBlade(num_lit, boostHue);
            prevLit = num_lit;
          }
          if (num_lit > 0) encoderMoved = true;

          if (digitalRead(SHEATH_PIN) == LOW) {
            flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return;
          }
          if (isSwingDetected()) {
            flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return;
          }
          delay(12);
        }
        if (!encoderMoved) { flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return; }
        else correct = true;
        stripOff();
      }
      else if (cmd == 1) { // Swing
        long swingBaseEnc = encPosition;
        bool curSwing, lastSwingState = false;
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          int encDelta = abs(encPosition - swingBaseEnc);
          if (encDelta >= 2) {
            flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return;
          }
          curSwing = isSwingDetected();
          if (!lastSwingState && curSwing) {
            yellowMiddleOut();
            correct = true; break;
          }
          lastSwingState = curSwing;
          if (digitalRead(SHEATH_PIN) == LOW) {
            flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return;
          }
          delay(12);
        }
      }
      else if (cmd == 2) { // Sheath
        blueBladeExtend();
        long sheathBaseEnc = encPosition;
        bool curSheath, lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          int encDelta = abs(encPosition - sheathBaseEnc);
          if (encDelta >= 2) {
            flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return;
          }
          curSheath = (digitalRead(SHEATH_PIN) == LOW);
          if (!lastSheathState && curSheath) {
            blueBladeRetract();
            correct = true; break;
          }
          lastSheathState = curSheath;
          if (isSwingDetected()) {
            flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return;
          }
          delay(12);
        }
      }

      if (!correct) { flashRed(); playSound(FX_GAME_OVER); showGameOver(score); return; }

      score++;
      // Decrease interval for next round, but we shouldn't go below minInterval -- i think
      if (interval > minInterval + 100) interval -= 200;
      else interval = minInterval;

      // Score logic if game reaches 99 points (which really who's gonna get there)
      if(score>=99){
        stripOff();
        playSound(FX_GAME_OVER);
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

// Game Over, Power Down
void showGameOver(int finalScore) {
  flashRed();
  playSound(FX_GAME_OVER);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(F("GAME OVER"));
  display.setTextSize(1);
  display.setCursor(0,32);
  display.print(F("Score: "));
  display.println(finalScore);

  if ((modeCount != 4) && (finalScore > highScore)) {
    highScore = finalScore;
    EEPROM.put(EEPROM_ADDR_HIGH, highScore);
  }
  display.setCursor(0,44);
  display.print(F("High Score: "));
  if (modeCount == 4) {
    display.println(finalScore);
  } else {
    display.println(highScore);
  }
  display.display();

  unsigned long t0 = millis();
  while (millis() - t0 < 8000) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  }
  oledOn = false;
  powerDownOLED();
}

void powerDownOLED() {
  stripOff();
  display.clearDisplay();
  display.display();
}