#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_LSM6DSOX.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// ----- Pinout -----
// 2  - Start (Power) Button (INPUT_PULLUP)
// 3  - Mode Select Button (INPUT_PULLUP)
// 5  - Encoder CLK (INPUT)
// 6  - Sheath Button (INPUT_PULLUP)
// 7  - NeoPixel Data (OUTPUT)
// 8  - Encoder DT (INPUT)
// 10 - FX: Boost It (OUTPUT, to FX board T0)
// 11 - FX: Sheath It (OUTPUT, to FX board T1)
// 12 - FX: Swing It (OUTPUT, to FX board T2)
// 13 - FX: Game On (OUTPUT, to FX board T3)
// 14 - FX: Game Over (OUTPUT, to FX board T4)
// GND - Shared Ground with FX board
// FX board's audio out to powered speaker/amplifier

#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1

#define SWITCH_PIN        2
#define MODE_BTN_PIN      3
#define SHEATH_PIN        6

#define NEOPIXEL_PIN      7
#define NUM_LEDS         60

#define ENC_A             5
#define ENC_B             8

#define FX_BOOST_PIN     9  // T0 - Boost It
#define FX_SHEATH_PIN    10  // T1 - Sheath It
#define FX_SWING_PIN     11  // T2 - Swing It
#define FX_GAME_ON       12  // T3 - Game On
#define FX_GAME_OVER     13  // T4 - Game Over

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

long encPosition = 0;
int lastEncCLK = LOW;
const float SWING_DELTA_THRESHOLD = 1.5;

const int EEPROM_ADDR_HIGH = 0;
int highScore = 0;
bool oledOn = false;
uint8_t modeCount = 1;
int lastModeState = HIGH;
uint16_t boostHue = 0;

float lastX = 0, lastY = 0, lastZ = 0;

// FX Board Sound Trigger Helper
void playSound(uint8_t fxPin, uint16_t holdMs=40, uint16_t pauseMs=180) {
  digitalWrite(fxPin, LOW);  // Trigger sound
  delay(holdMs);
  digitalWrite(fxPin, HIGH); // Release trigger
  delay(pauseMs);
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

void movingGreenWave(uint16_t duration_ms = 750) {
  uint8_t frames = 24;
  uint8_t frameDelay = duration_ms / frames;
  for (uint8_t f = 0; f < frames; f++) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      int d = abs((int)i - (f * NUM_LEDS / frames));
      uint8_t val = (d < 4) ? 200 - 45 * d : 0;
      strip.setPixelColor(i, 0,val,0,0);
    }
    strip.show();
    delay(frameDelay);
  }
  stripOff();
}

void showGradientBlade(int count, uint16_t baseHue) {
  count = constrain(count, 0, NUM_LEDS);
  uint16_t delta = 4000; // Spread hues for blade
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
  for (uint8_t i = 1; i <= 3; i++) {
    display.setCursor(0, 18 + (i-1)*12);
    display.print(i == highlight ? F("> ") : F("  "));
    display.print(i);
    display.print(F(") "));
    display.println(i == 1 ? F("Easy") : i == 2 ? F("Medium") : F("Hard"));
  }
  display.display();
}

void showModeConfirmation(uint8_t mode) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if      (mode == 1) display.println(F("Easy Mode"));
  else if (mode == 2) display.println(F("Medium Mode"));
  else                display.println(F("Hard Mode"));
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
        modeCount = (modeCount % 3) + 1;
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
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); display.display();

  delay(100);

  IMU.begin();
  EEPROM.get(EEPROM_ADDR_HIGH, highScore);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);
  pinMode(SHEATH_PIN, INPUT_PULLUP);
  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);
  lastEncCLK = digitalRead(ENC_A);

  strip.begin();
  strip.setBrightness(80);
  stripOff();

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

  playSound(FX_GAME_ON, 100, 180); // Play "Game On" sound (T3)

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
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    float dx = abs(x - lastX), dy = abs(y - lastY), dz = abs(z - lastZ);
    lastX = x; lastY = y; lastZ = z;
    return (dx > SWING_DELTA_THRESHOLD || dy > SWING_DELTA_THRESHOLD || dz > SWING_DELTA_THRESHOLD);
  }
  return false;
}

void runGame() {
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
      // for (uint8_t i = 0; i < cmd; i++) {
      // if (checkPowerToggle()) { powerDownOLED(); return; }
      if (cmd == 1) playSound(FX_SWING_PIN, 50, 180);
      else if (cmd == 2) playSound(FX_SHEATH_PIN, 50, 180);
      else if (cmd == 3) playSound(FX_BOOST_PIN, 50, 180);
      // }

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
          long curEnc = encPosition;
          int delta = abs(curEnc - baseEnc);
          if (delta > maxDelta) maxDelta = delta;
          int num_lit = maxDelta / 1;
          num_lit = constrain(num_lit, 0, NUM_LEDS);
          if (num_lit != prevLit) {
            showGradientBlade(num_lit, boostHue);
            prevLit = num_lit;
          }
          if (num_lit > 0) encoderMoved = true;

          if (digitalRead(SHEATH_PIN) == LOW) {
            flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return;
          }
          if (isSwingDetected()) {
            flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return;
          }
          delay(12);
        }
        if (!encoderMoved) { flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return; }
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
            flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return;
          }
          curSwing = isSwingDetected();
          if (!lastSwingState && curSwing) {
            movingGreenWave();
            correct = true; break;
          }
          lastSwingState = curSwing;
          if (digitalRead(SHEATH_PIN) == LOW) {
            flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return;
          }
          delay(12);
        }
      }
      else if (cmd == 2) { // Sheath
        long sheathBaseEnc = encPosition;
        bool curSheath, lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          int encDelta = abs(encPosition - sheathBaseEnc);
          if (encDelta >= 2) {
            flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return;
          }
          curSheath = (digitalRead(SHEATH_PIN) == LOW);
          if (!lastSheathState && curSheath) {
            movingGreenWave();
            correct = true; break;
          }
          lastSheathState = curSheath;
          if (isSwingDetected()) {
            flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return;
          }
          delay(12);
        }
      }

      if (!correct) { flashRed(); playSound(FX_GAME_OVER, 80, 500); showGameOver(score); return; }

      score++;
      if(score>=99){
        stripOff();
        playSound(FX_GAME_OVER, 120, 250);
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
  playSound(FX_GAME_OVER, 120, 700);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println(F("GAME OVER"));
  display.setTextSize(1);
  display.setCursor(0,32);
  display.print(F("Score: "));
  display.println(finalScore);

  if (finalScore > highScore) {
    highScore = finalScore;
    EEPROM.put(EEPROM_ADDR_HIGH, highScore);
  }
  display.setCursor(0,44);
  display.print(F("High: "));
  display.println(highScore);
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
