#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_LSM6DSOX.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// ----- Pin Config -----
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1
#define SWITCH_PIN        2    // Start (power) button
#define MODE_BTN_PIN      3    // Mode select button
#define BUZZER_PIN        9
#define SHEATH_PIN        6

#define NEOPIXEL_PIN      7
#define NUM_LEDS         60

// Use RGBW (NEO_RGBW) for SK6812
Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

const uint8_t ENC_A = 5, ENC_B = 8;
long encPosition = 0; int lastEncCLK = LOW;
const float SWING_DELTA_THRESHOLD = 1.5;

const int EEPROM_ADDR_HIGH = 0;
int highScore = 0;
bool oledOn = false;
uint8_t modeCount = 1;
int lastModeState = HIGH;
uint16_t boostHue = 0; // Will be set randomly for each BOOST IT

float lastX = 0, lastY = 0, lastZ = 0;

// ──────────────────────────────
// RGBW LED helpers

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

// Gradient Hues; only 'count' LEDs lit
void showGradientBlade(int count, uint16_t baseHue) {
  count = constrain(count, 0, NUM_LEDS);
  uint16_t delta = 4000; // How much the hue shifts across blade
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (i < count) {
      // Spread hues across the blade: baseHue to baseHue+delta
      uint16_t thisHue = baseHue + ((uint32_t)i * delta / (count>1?count-1:1));
      uint32_t color = strip.gamma32(strip.ColorHSV(thisHue));
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8) & 0xFF;
      uint8_t b = color & 0xFF;
      strip.setPixelColor(i, r, g, b, 0); // RGBW
    } else {
      strip.setPixelColor(i, 0,0,0,0);
    }
  }
  strip.show();
}

// ──────────────────────────────
// Utility/game helpers

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

void updateEncoder() {
  int clk = digitalRead(ENC_A);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_B) != clk) encPosition++;
    else                            encPosition--;
  }
  lastEncCLK = clk;
}

// ──────────────────────────────
// OLED, Mode, and Main Menu Logic

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
  delay(1300); // Briefly show selection
  display.clearDisplay(); display.display();
}

void waitForStartButton() {
  display.clearDisplay();
  display.display();  // Blank OLED
  // Wait until the START (power) button is pressed
  while (digitalRead(SWITCH_PIN) == HIGH) {
    delay(8);
  }
  // Debounce
  while (digitalRead(SWITCH_PIN) == LOW) delay(8);
}

void modeSelectMenu() {
  modeCount = 1;
  showMenu(modeCount);
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();

  // Wait for user to cycle/click. If idle 5s, selects current mode.
  while (1) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(25); // debounce
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = (modeCount % 3) + 1;
        showMenu(modeCount);
        lastPress = millis();
      }
    }
    lastModeState = m;
    // Select mode if idle for 5s
    if (millis() - lastPress > 5000) break;
    delay(10);
  }
  showModeConfirmation(modeCount);
  runGame();
}

// ──────────────────────────────
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

  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);
  lastEncCLK = digitalRead(ENC_A);

  strip.begin();
  strip.setBrightness(80);
  stripOff();

  randomSeed(analogRead(A5));
}

void loop() {
  // 1. Start up: OLED blank
  waitForStartButton();

  tone(BUZZER_PIN, 1047, 300); // C6 note for 300ms
  delay(300); // Wait for the tone to finish
  noTone(BUZZER_PIN);

  // 2. Splash screen after pressing START
  showSplash();
  unsigned long splashStart = millis();
  while (millis() - splashStart < 1600) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    delay(8);
  }
  display.clearDisplay(); display.display(); // Blank again

  oledOn = true; // Mark system as powered up

  // 3. Wait for mode select button to be pressed (blank until user interacts)
  while (oledOn) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    if (digitalRead(MODE_BTN_PIN) == LOW) {
      while (digitalRead(MODE_BTN_PIN) == LOW) delay(8); // Debounce release
      modeSelectMenu();
      // When game ends or powers down, return to blank and require restart
      break;
    }
    delay(8);
  }
}

// ──────────────────────────────
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
      long maxDelta = 0;
      bool correct = false;
      bool lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
      bool lastSwingState = false;
      unsigned long start = millis();

      if (cmd == 3) { // BOOST IT
        // Pick a random hue at the start of each BOOST IT prompt
        boostHue = random(0, 65536); // 0 to 65535

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
            showGradientBlade(num_lit, boostHue); // Now sends in the random hue we chose
            prevLit = num_lit;
          }
          if (num_lit > 0) encoderMoved = true;

          if (digitalRead(SHEATH_PIN) == LOW) {
              flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
          }
          if (isSwingDetected()) {
              flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
          }

          delay(12);
        }
        if (!encoderMoved) { flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return; }
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
            flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
          }
          curSwing = isSwingDetected();
          if (!lastSwingState && curSwing) {
            movingGreenWave();
            correct = true; break;
          }
          lastSwingState = curSwing;
          if (digitalRead(SHEATH_PIN) == LOW) {
            flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
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
            flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
          }
          curSheath = (digitalRead(SHEATH_PIN) == LOW);
          if (!lastSheathState && curSheath) {
            movingGreenWave();
            correct = true; break;
          }
          lastSheathState = curSheath;
          if (isSwingDetected()) {
            flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return;
          }
          delay(12);
        }
      }

      if (!correct) { flashRed(); tone(BUZZER_PIN, 400, 200); showGameOver(score); return; }

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

// ──────────────────────────────
// Game Over, Power Down

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
