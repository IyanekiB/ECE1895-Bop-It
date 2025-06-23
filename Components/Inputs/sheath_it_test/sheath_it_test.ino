#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ───────────────────────────────────────────────
// OLED & Game pins
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1    // unused
#define SWITCH_PIN        2    // START / power toggle
#define MODE_BTN_PIN      3    // mode select
#define OLED_POWER_PIN    4
#define BUZZER_PIN        5
#define SWING_PIN         A1   // Swing It! analog threshold
#define SHEATH_PIN        6    // Sheath It! digital
// BOOST_PIN removed: replaced by encoder
#define SWING_THRESHOLD   600

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

int  lastModeState = HIGH;
bool oledOn        = false;
uint8_t modeCount  = 1;       // 1=Easy,2=Medium,3=Hard

// ───────────────────────────────────────────────
// Rotary encoder pins & state
const uint8_t ENC_CLK = 9;
const uint8_t ENC_DT  = 8;

// RGB LED pins (common‐cathode)
const uint8_t RED_PIN   = 11;
const uint8_t BLUE_PIN  = 12;
const uint8_t GREEN_PIN = 13;

// encoder state
long encPosition = 0;
int  lastEncCLK  = LOW;

// color palette: 0→red,1→blue,2→green,3→yellow,4→cyan,5→magenta,6→white
const uint8_t colorTable[][3] = {
  {255,   0,   0},  // red
  {  0,   0, 255},  // blue
  {  0, 255,   0},  // green
  {255, 255,   0},  // yellow
  {  0, 255, 255},  // cyan
  {255,   0, 255},  // magenta
  {255, 255, 255},  // white
};
const uint8_t NUM_COLORS = sizeof(colorTable)/sizeof(colorTable[0]);

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

void setup() {
  Serial.begin(9600);

  // OLED & game pins
  pinMode(SWITCH_PIN,     INPUT_PULLUP);
  pinMode(MODE_BTN_PIN,   INPUT_PULLUP);
  pinMode(OLED_POWER_PIN, OUTPUT);
  pinMode(BUZZER_PIN,     OUTPUT);
  pinMode(SHEATH_PIN,     INPUT_PULLUP);
  digitalWrite(OLED_POWER_PIN, LOW);
  digitalWrite(BUZZER_PIN,   LOW);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed"));
    while (1);
  }
  display.clearDisplay();
  display.display();

  // encoder & RGB init
  pinMode(ENC_CLK,   INPUT);
  pinMode(ENC_DT,    INPUT);
  lastEncCLK = digitalRead(ENC_CLK);

  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  setRGB(0,0,0);  // off

  randomSeed(analogRead(A5));
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
  digitalWrite(OLED_POWER_PIN, HIGH);
  delay(10);

  // Title (interruptible)
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Bop-It!");
  display.display();
  {
    unsigned long t0 = millis();
    while (millis()-t0 < 2000) {
      if (checkPowerToggle()) {
        oledOn = false;
        powerDownOLED();
        return;
      }
    }
  }

  // Mode select: cycle indefinitely, then auto‐confirm after 3 s of no presses
  modeCount = 1;
  showMenu();
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();

  while (millis() - lastPress < 3000) {
    // power‐off check
    if (checkPowerToggle()) {
      oledOn = false;
      powerDownOLED();
      return;
    }
    // cycle modes on MODE_BTN
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(50);
      m = digitalRead(MODE_BTN_PIN);
      if (m == LOW) {
        modeCount = (modeCount % 3) + 1;
        Serial.print("Mode = "); Serial.println(modeCount);
        showMenu();
        lastPress = millis();  // reset timeout
      }
    }
    lastModeState = m;
  }

  // Confirm screen (2 s)
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,16);
  if      (modeCount==1) display.println("Easy Mode");
  else if (modeCount==2) display.println("Medium Mode");
  else                   display.println("Hard Mode");
  display.display();
  {
    unsigned long t0 = millis();
    while (millis()-t0 < 2000) {
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
  unsigned long interval = (modeCount==1?5000:
                            modeCount==2?4000:3000);
  unsigned long lastCueTime = millis();
  int score = 0;

  while (oledOn) {
    updateEncoder();  // keep encoder state fresh

    // power‐off?
    if (checkPowerToggle()) {
      Serial.println("OLED OFF during game");
      break;
    }

    // next command?
    if (millis() - lastCueTime >= interval) {
      lastCueTime = millis();
      uint8_t cmd = random(1,4);
      const char* txt = (cmd==1?"Swing It!":
                         cmd==2?"Sheath It!":
                                  "Boost It!");
      Serial.print("Command: "); Serial.println(txt);

      // show command & clear any RGB
      setRGB(0,0,0);
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0,16);
      display.println(txt);
      display.display();

      // beeps (interruptible)
      for (uint8_t i=0; i<cmd; i++) {
        if (checkPowerToggle()) {
          noTone(BUZZER_PIN);
          oledOn = false; powerDownOLED(); return;
        }
        tone(BUZZER_PIN, 2000, 200);
        unsigned long t0 = millis();
        while (millis()-t0 < 200) {
          if (checkPowerToggle()) {
            noTone(BUZZER_PIN);
            oledOn=false; powerDownOLED(); return;
          }
        }
        unsigned long t1 = millis();
        while (millis()-t1 < 250) {
          if (checkPowerToggle()) {
            noTone(BUZZER_PIN);
            oledOn=false; powerDownOLED(); return;
          }
        }
      }
      noTone(BUZZER_PIN);

      // wait for input
      unsigned long start = millis();
      bool ok = false;
      long  baseEnc = encPosition;
      while (millis() - start < interval) {
        updateEncoder();
        if (checkPowerToggle()) {
          oledOn = false; powerDownOLED(); return;
        }
        if (cmd==1 && analogRead(SWING_PIN) > SWING_THRESHOLD) {
          ok = true; break;
        }
        if (cmd == 2) {
          // wait for button press (LOW)
          if (digitalRead(SHEATH_PIN) == LOW) {
            delay(20);                     // simple debounce
            if (digitalRead(SHEATH_PIN) == LOW) {
              ok = true;
              break;
            }
          }
        }
        if (cmd==3) {
          // only light color after you actually rotate
          if (encPosition != baseEnc) {
            int idx = (score/10) % NUM_COLORS;
            auto &c = colorTable[idx];
            setRGB(c[0], c[1], c[2]);
            ok = true;
            break;
          }
        }
      }

      // miss → game over
      if (!ok) {
        setRGB(0,0,0);
        showGameOver(score);
        return;
      }

      score++;
      Serial.print("Score: "); Serial.println(score);
    }
  }

  powerDownOLED();
}

void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Select Mode:");
  for (uint8_t i=1; i<=3; i++) {
    display.setCursor(0,16 + (i-1)*12);
    display.print(i==modeCount?"> ":"  ");
    display.print(i); display.print(") ");
    display.println(i==1?"Easy": i==2?"Medium":"Hard");
  }
  display.display();
}

void showGameOver(int finalScore) {
  // GAME OVER
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("GAME OVER");
  display.setTextSize(1);
  display.setCursor(0,34);
  display.print("Score: ");
  display.println(finalScore);
  display.display();
  Serial.print("Final Score: "); Serial.println(finalScore);

  // pause 2 s
  {
    unsigned long t0 = millis();
    while (millis()-t0 < 2000) {
      if (checkPowerToggle()) {
        oledOn=false; powerDownOLED(); return;
      }
    }
  }

  // two‐option menu
  uint8_t option = 1;
  lastModeState = digitalRead(MODE_BTN_PIN);
  unsigned long lastPress = millis();
  showGameOverMenu(option);

  while (millis()-lastPress < 5000) {
    if (checkPowerToggle()) {
      oledOn=false; return;
    }
    int m = digitalRead(MODE_BTN_PIN);
    if (m != lastModeState) {
      delay(50);
      m = digitalRead(MODE_BTN_PIN);
      if (m==LOW) {
        option = (option%2)+1;
        showGameOverMenu(option);
        lastPress = millis();
      }
    }
    lastModeState = m;
  }

  if (option==1)      runGame();
  else                powerUpAndSelectMode();
}

void showGameOverMenu(uint8_t opt) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Select Option:");
  display.setCursor(0,16);
  display.print(opt==1?"> ":"  "); display.println("Play Again");
  display.setCursor(0,28);
  display.print(opt==2?"> ":"  "); display.println("Mode Select");
  display.display();
}

void powerDownOLED() {
  display.clearDisplay();
  display.display();
  digitalWrite(OLED_POWER_PIN, LOW);
}
