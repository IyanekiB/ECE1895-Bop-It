#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_LSM6DSOX.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <SD.h>
#include <TMRpcm.h>

// Pin and Device Config 
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1            // OLED reset (unused)
#define SWITCH_PIN        2            // Power switch pin
#define MODE_BTN_PIN      3            // Mode select button pin
#define SHEATH_PIN        6            // Sheath action button pin

#define NEOPIXEL_PIN     7             // Data pin for NeoPixel LED strip
#define NUM_LEDS         30            // Number of LEDs in the strip

#define SD_CS_PIN        4             // Chip select for SD card
#define AUDIO_PIN        9             // TMRpcm ONLY WORKS ON PIN 9 (Uno/Nano)

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
TMRpcm audio;

// Rotary encoder pins and state
const uint8_t ENC_CLK = 5, ENC_DT = 8;
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

// NeoPixel LED helpers
void stripOff() {
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, 0);
  strip.show();
}
void flashRed(uint16_t ms = 350) {
  for (uint8_t i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, strip.Color(255, 0, 0));
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
      strip.setPixelColor(i, 0, val, 0);
    }
    strip.show();
    delay(frameDelay);
  }
  stripOff();
}
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

// Input and game utils
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
  int clk = digitalRead(ENC_CLK);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_DT) != clk) encPosition++;
    else                            encPosition--;
  }
  lastEncCLK = clk;
}

// AUDIO PLAYBACK HELPERS
void playPromptAudio(uint8_t cmd) {
  // cmd: 1=Swing, 2=Sheath, 3=Boost
  switch(cmd) {
    case 1: audio.play("SWINGIT.WAV"); break;
    case 2: audio.play("SHEATHIT.WAV"); break;
    case 3: audio.play("BOOSTIT.WAV"); break;
    default: break;
  }
}
void playErrorAudio() {
  // Play a simple beep (or an error file if you like)
  audio.speakerPin = AUDIO_PIN;
  audio.setVolume(5);
  audio.play("ERROR.WAV"); // Optional: create a short "fail" audio file
}

// MAIN SETUP/LOOP
void setup() {
  Wire.begin();
  // display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  display.clearDisplay(); display.display();

  IMU.begin();
  EEPROM.get(EEPROM_ADDR_HIGH, highScore);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);
  pinMode(SHEATH_PIN, INPUT_PULLUP);
  pinMode(ENC_CLK, INPUT);
  pinMode(ENC_DT, INPUT);
  lastEncCLK = digitalRead(ENC_CLK);

  strip.begin();
  strip.setBrightness(80);
  stripOff();

  randomSeed(analogRead(A5));

  // SD card and audio setup
  if (!SD.begin(SD_CS_PIN)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("SD Fail");
    display.display();
    delay(2000);
    while(1); // Halt
  }
  audio.speakerPin = AUDIO_PIN;
  audio.setVolume(5);
}

void loop() {
  if (checkPowerToggle()) {
    oledOn = !oledOn;
    if (oledOn)      powerUpAndSelectMode();
    else             powerDownOLED();
  }
}

// Menu and Game Logic
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

  unsigned long t0 = millis();
  while (millis() - t0 < 1600) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  }

  modeCount = 1;
  showMenu();
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();

  while (millis() - lastPress < 2300) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(28);
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = (modeCount % 3) + 1;
        showMenu();
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

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

bool isSwingDetected() {
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    float dx = abs(x - lastX), dy = abs(y - lastY), dz = abs(z - lastZ);
    lastX = x; lastY = y; lastZ = z;
    return (dx > 1.5 || dy > 1.5 || dz > 1.5);
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

      // Play audio prompt for cue (replace buzzer)
      stripOff();
      playPromptAudio(cmd); 
      delay(700); // Give time for short prompt playback

      long baseEnc = encPosition;
      long maxDelta = 0;
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
          int delta = abs(curEnc - baseEnc);
          if (delta > maxDelta) maxDelta = delta;
          int num_lit = maxDelta / 2; 
          num_lit = constrain(num_lit, 0, NUM_LEDS);

          if (num_lit != prevLit) {
            showRainbowGrowing(num_lit);
            prevLit = num_lit;
          }
          if (num_lit > 0) encoderMoved = true;

          delay(12);
        }
        if (!encoderMoved) { 
          flashRed(); 
          playErrorAudio(); 
          showGameOver(score); 
          return; 
        }
        else correct = true;
        stripOff();
      } 
      // SWING input (motion/buttons)
      else if (cmd == 1) {
        long swingBaseEnc = encPosition;
        bool curSwing, lastSwingState = false;
        bool movedEncoder = false;
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          if (encPosition != swingBaseEnc) {
            flashRed(); playErrorAudio(); showGameOver(score); return;
          }
          curSwing = isSwingDetected();
          if (!lastSwingState && curSwing) {
            movingGreenWave();
            correct = true;
            break;
          }
          lastSwingState = curSwing;
          if (digitalRead(SHEATH_PIN) == LOW) {
            flashRed(); playErrorAudio(); showGameOver(score); return;
          }
          delay(12);
        }
      }
      // SHEATH input (motion/buttons)
      else if (cmd == 2) {
        long sheathBaseEnc = encPosition;
        bool curSheath, lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          if (encPosition != sheathBaseEnc) {
            flashRed(); playErrorAudio(); showGameOver(score); return;
          }
          curSheath = (digitalRead(SHEATH_PIN) == LOW);
          if (!lastSheathState && curSheath) {
            movingGreenWave();
            correct = true;
            break;
          }
          lastSheathState = curSheath;
          if (isSwingDetected()) {
            flashRed(); playErrorAudio(); showGameOver(score); return;
          }
          delay(12);
        }
      }

      if (!correct) { 
        flashRed(); playErrorAudio(); showGameOver(score); return; 
      }
      score++;
      if(score>=99){
        stripOff();
        playErrorAudio();
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
  while (millis() - t0 < 3300) {
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
