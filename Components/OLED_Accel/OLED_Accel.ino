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
const uint8_t ENC_CLK = 9, ENC_DT = 8;
// RGB LED pins
const uint8_t RED_PIN = 11, BLUE_PIN = 12, GREEN_PIN = 13;
// Color palette
const uint8_t colorTable[][3] = {
  {255,0,0}, {0,0,255}, {0,255,0}, {255,255,0}, {0,255,255}, {255,0,255}, {255,255,255}
};
const uint8_t NUM_COLORS = sizeof(colorTable) / sizeof(colorTable[0]);
// EEPROM & game state
const int EEPROM_ADDR_HIGH = 0;
int highScore = 0;
bool oledOn = false;
uint8_t modeCount = 1;
int lastModeState = HIGH;
long encPosition = 0;
int lastEncCLK = LOW;

// Swing detection state
float lastX = 0, lastY = 0, lastZ = 0;
const float SWING_DELTA_THRESHOLD = 0.2;

// ---- Utility ----
bool checkPowerToggle() {
  static int last = HIGH;
  int cur = digitalRead(SWITCH_PIN);
  if (cur != last) {
    delay(50);
    cur = digitalRead(SWITCH_PIN);
    if (cur != last && cur == LOW) { last = cur; return true; }
  }
  last = cur;
  return false;
}
void updateEncoder() {
  int clk = digitalRead(ENC_CLK);
  if (clk != lastEncCLK) {
    if (digitalRead(ENC_DT) != clk) encPosition++;
    else encPosition--;
  }
  lastEncCLK = clk;
}
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(RED_PIN, r > 128 ? HIGH : LOW);
  digitalWrite(GREEN_PIN, g > 128 ? HIGH : LOW);
  digitalWrite(BLUE_PIN, b > 128 ? HIGH : LOW);
}

// ---- Input logic modularized ----
bool checkSheathEdge(bool &lastSheathState) {
  bool curSheath = (digitalRead(SHEATH_PIN) == LOW);
  bool edge = !lastSheathState && curSheath;
  lastSheathState = curSheath;
  return edge;
}
bool checkBoostEdge(long &baseEnc) {
  if (encPosition != baseEnc) { baseEnc = encPosition; return true; }
  return false;
}
bool checkSwingEdge(bool &lastSwingState, bool showDebug = false) {
  float x, y, z;
  bool swung = false;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    float dx = abs(x - lastX), dy = abs(y - lastY), dz = abs(z - lastZ);
    swung = (dx > SWING_DELTA_THRESHOLD || dy > SWING_DELTA_THRESHOLD || dz > SWING_DELTA_THRESHOLD);
    lastX = x; lastY = y; lastZ = z;
    if (showDebug) Serial.println(swung ? "Swing: YES" : "Swing: NO");
  }
  bool edge = !lastSwingState && swung;
  lastSwingState = swung;
  return edge;
}

// ---- OLED display helpers ----
void oledClear() {
  display.clearDisplay(); // Always enough, never double with fillScreen(BLACK)
}
void showScorePrompt(int score, const char *prompt, uint8_t textSize=2) {
  oledClear();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Score: ");
  display.println(score);
  display.setTextSize(textSize);
  display.setCursor(0,16);
  display.println(prompt);
  display.display();
}

// ---- Main ----
void setup() {
  Serial.begin(9600);
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { Serial.println(F("OLED init failed")); while (1); }
  display.clearDisplay(); display.display(); delay(100);
  if (!IMU.begin()) { Serial.println("Failed to initialize IMU!"); while (1); }
  EEPROM.get(EEPROM_ADDR_HIGH, highScore);
  pinMode(SWITCH_PIN,INPUT_PULLUP); pinMode(MODE_BTN_PIN,INPUT_PULLUP);
  pinMode(BUZZER_PIN,OUTPUT); pinMode(SHEATH_PIN,INPUT_PULLUP);
  digitalWrite(BUZZER_PIN,LOW);
  pinMode(ENC_CLK,INPUT); pinMode(ENC_DT,INPUT); lastEncCLK = digitalRead(ENC_CLK);
  pinMode(RED_PIN,OUTPUT); pinMode(GREEN_PIN,OUTPUT); pinMode(BLUE_PIN,OUTPUT);
  setRGB(0,0,0);
  randomSeed(analogRead(A5));
  Serial.print("Swing threshold: "); Serial.println(SWING_DELTA_THRESHOLD, 2);
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
  oledClear();
  display.setTextSize(2); display.setTextColor(WHITE); display.setCursor(0,0); display.println("Bop-It!");
  display.setTextSize(1); display.setCursor(0,32); display.display();
  unsigned long t0 = millis(); while (millis() - t0 < 2000) if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  modeCount = 1; showMenu(); lastModeState = digitalRead(MODE_BTN_PIN); unsigned long lastPress = millis();
  while (millis() - lastPress < 3000) {
    if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) { delay(50); m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) { modeCount = (modeCount % 3) + 1; Serial.print("Mode = "); Serial.println(modeCount); showMenu(); lastPress = millis(); }
    }
    lastModeState = m;
  }
  oledClear(); display.setTextSize(2); display.setCursor(0,16);
  if (modeCount==1) display.println("Easy Mode"); else if (modeCount==2) display.println("Medium Mode"); else display.println("Hard Mode");
  display.display();
  t0 = millis(); while (millis() - t0 < 2000) if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  runGame();
}

void runGame() {
  unsigned long interval = (modeCount==1 ? 5000 : modeCount==2 ? 4000 : 3000);
  unsigned long lastCueTime = millis();
  int score = 0;
  showScorePrompt(score, "");
  while (oledOn) {
    updateEncoder();
    if (checkPowerToggle()) { powerDownOLED(); return; }
    if (millis() - lastCueTime >= interval) {
      lastCueTime = millis();
      uint8_t cmd = random(1,4);
      const char* txt = (cmd==1 ? "Swing It!" : cmd==2 ? "Sheath It!" : "Boost It!");
      showScorePrompt(score, txt, 2);
      setRGB(0,0,0);

      // cue beeps
      for (uint8_t i = 0; i < cmd; i++) {
        if (checkPowerToggle()) { powerDownOLED(); return; }
        tone(BUZZER_PIN, 2000, 200);
        unsigned long t0 = millis(); while (millis() - t0 < 200) if (checkPowerToggle()) { powerDownOLED(); return; }
        unsigned long t1 = millis(); while (millis() - t1 < 250) if (checkPowerToggle()) { powerDownOLED(); return; }
      }
      noTone(BUZZER_PIN);

      // Input phase, modular logic!
      bool swung = false, sheathed = false, boosted = false;
      long baseEnc = encPosition;
      bool lastSheathState = (digitalRead(SHEATH_PIN) == LOW);
      bool lastSwingState = false;
      unsigned long start = millis();
      bool correct = false;
      while (millis() - start < interval) {
        updateEncoder();
        if (checkPowerToggle()) { powerDownOLED(); return; }
        bool edgeSheath = checkSheathEdge(lastSheathState);
        bool edgeBoost  = checkBoostEdge(baseEnc);
        bool edgeSwing  = checkSwingEdge(lastSwingState, cmd == 1);
        if (!sheathed && edgeSheath) {
          sheathed = true;
          if (cmd == 2) { correct = true; }
          else { setRGB(0,0,0); tone(BUZZER_PIN,400,200); showGameOver(score); return; }
        }
        if (!swung && edgeSwing) {
          swung = true;
          if (cmd == 1) { correct = true; }
          else { setRGB(0,0,0); tone(BUZZER_PIN,400,200); showGameOver(score); return; }
        }
        if (!boosted && edgeBoost) {
          boosted = true;
          if (cmd == 3) {
            int idx = (score / 10) % NUM_COLORS;
            auto &c = colorTable[idx];
            setRGB(c[0], c[1], c[2]);
            correct = true;
          } else { setRGB(0,0,0); tone(BUZZER_PIN,400,200); showGameOver(score); return; }
        }
        if (correct) break;
        delay(10);
      }
      if (!correct) { setRGB(0,0,0); tone(BUZZER_PIN,400,200); showGameOver(score); return; }
      score++;
      if(score>=99) { setRGB(0,0,0); tone(BUZZER_PIN,400,200); showGameOver(score); return; }
      showScorePrompt(score, "", 1);
      delay(200);
    }
  }
}

void showMenu() {
  oledClear();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Select Mode:");
  for (uint8_t i = 1; i <= 3; i++) {
    display.setCursor(0, 16 + (i-1)*12);
    display.print(i == modeCount ? "> " : "  ");
    display.print(i); display.print(") ");
    display.println(i == 1 ? "Easy" : i == 2 ? "Medium" : "Hard");
  }
  display.display();
}
void showGameOver(int finalScore) {
  oledClear();
  display.setTextSize(2); display.setCursor(0,0); display.println("GAME OVER");
  display.setTextSize(1); display.setCursor(0,32); display.print("Score: "); display.println(finalScore);
  if (finalScore > highScore) { highScore = finalScore; EEPROM.put(EEPROM_ADDR_HIGH, highScore); Serial.println("New High Score!"); }
  display.setCursor(0,44); display.print("High: "); display.println(highScore);
  display.display();
  unsigned long t0 = millis(); while (millis() - t0 < 4000) if (checkPowerToggle()) { oledOn = false; powerDownOLED(); return; }
  uint8_t option = 1; lastModeState = digitalRead(MODE_BTN_PIN); unsigned long lastPress = millis(); showGameOverMenu(option);
  while (millis() - lastPress < 5000) {
    if (checkPowerToggle()) { oledOn = false; return; }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) { delay(50); m = digitalRead(MODE_BTN_PIN); if (m == LOW) { option = (option % 2) + 1; showGameOverMenu(option); lastPress = millis(); } }
    lastModeState = m;
  }
  if (option == 1) runGame();
  else powerUpAndSelectMode();
}
void showGameOverMenu(uint8_t opt) {
  oledClear();
  display.setTextSize(1);
  display.setCursor(0,0); display.println("Select Option:");
  display.setCursor(0,16); display.print(opt == 1 ? "> " : "  "); display.println("Play Again");
  display.setCursor(0,28); display.print(opt == 2 ? "> " : "  "); display.println("Mode Select");
  display.display();
}
void powerDownOLED() { oledClear(); display.display(); }
