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

// Encoder state
long encPosition = 0;
int  lastEncCLK  = LOW;

// Swing threshold (fixed)
const float SWING_DELTA_THRESHOLD = 0.4;

// Color palette for boost cycling
const uint8_t colorTable[][3] = {
  {255,   0,   0},   // Red
  {  0,   0, 255},   // Blue
  {  0, 255,   0},   // Green
  {255, 255,   0},   // Yellow
  {  0, 255, 255},   // Cyan
  {255,   0, 255},   // Magenta
  {255, 255, 255}    // White
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

// ───────────────────────────────────────────────
// Utility functions

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

void updateEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_DT) != clk) encPosition++;
    else                            encPosition--;
  }
  lastEncCLK = clk;
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(RED_PIN,   r > 128 ? HIGH : LOW);
  digitalWrite(GREEN_PIN, g > 128 ? HIGH : LOW);
  digitalWrite(BLUE_PIN,  b > 128 ? HIGH : LOW);
}

// ───────────────────────────────────────────────
// Main logic

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

// Detect swing (any axis)
bool isSwingDetected() {
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    float deltaX = abs(x - lastX);
    float deltaY = abs(y - lastY);
    float deltaZ = abs(z - lastZ);
    lastX = x; lastY = y; lastZ = z;
    return (deltaX > SWING_DELTA_THRESHOLD ||
            deltaY > SWING_DELTA_THRESHOLD ||
            deltaZ > SWING_DELTA_THRESHOLD);
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
      uint8_t cmd = random(1,4); // 1=Swing, 2=Sheath, 3=Boost
      const char* txt = (cmd==1 ? "Swing It!" :
                         cmd==2 ? "Sheath It!" :
                                  "Boost It!");

      // Show prompt and score
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print("Score: ");
      display.println(score);
      display.setTextSize(2);
      display.setCursor(0,16);
      display.println(txt);
      display.display();

      setRGB(0,0,0);

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

      // Input phase: only check for the correct input
      long baseEnc = encPosition;
      bool correct = false;
      bool lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
      bool lastSwingState = false;
      unsigned long start = millis();

      if (cmd == 3) { // BOOST: Show white, then cycle color as encoder moves
        setRGB(255, 255, 255); // White prompt
        int lastColorIdx = 0;
        long lastEncVal = baseEnc;
        bool encoderMoved = false;
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }
          long curEnc = encPosition;
          // If encoder rotates
          if (curEnc != lastEncVal) {
            encoderMoved = true;
            // Calculate color index based on encoder position
            int colorIdx = abs(curEnc) % NUM_COLORS;
            if (colorIdx != lastColorIdx) {
              auto &c = colorTable[colorIdx];
              setRGB(c[0], c[1], c[2]);
              lastColorIdx = colorIdx;
            }
            lastEncVal = curEnc;
            correct = true; // Register as correct on first movement
          }
          delay(5);
        }
        // Optionally turn off after round
        setRGB(0, 0, 0);
      } else {
        while (millis() - start < interval) {
          updateEncoder();
          if (checkPowerToggle()) { powerDownOLED(); return; }

          if (cmd == 1) { // Swing
            bool curSwing = isSwingDetected();
            if (!lastSwingState && curSwing) {
              setRGB(0, 255, 0); // Green
              delay(400);
              setRGB(0, 0, 0);
              correct = true; break;
            }
            lastSwingState = curSwing;
          } else if (cmd == 2) { // Sheath
            bool curSheath = (digitalRead(SHEATH_PIN) == LOW);
            if (!lastSheathState && curSheath) {
              setRGB(0, 255, 0); // Green
              delay(400);
              setRGB(0, 0, 0);
              correct = true; break;
            }
            lastSheathState = curSheath;
          }
          delay(10);
        }
      }

      // timeout with no correct input
      if (!correct) {
        setRGB(255, 0, 0); // Red on game over or wrong input
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
      // No need to turn off LED here—already done after green linger.
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
  setRGB(255, 0, 0); // Red

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

  // Linger for 4 seconds, allow power off during this time
  unsigned long t0 = millis();
  while (millis() - t0 < 4000) {
    if (checkPowerToggle()) {
      oledOn = false;
      powerDownOLED();
      return;
    }
  }
  // After lingering, turn off display/game automatically
  oledOn = false;
  powerDownOLED();
}

void powerDownOLED() {
  setRGB(0, 0, 0); // RGB off on shutdown
  display.clearDisplay();
  display.display();
}
