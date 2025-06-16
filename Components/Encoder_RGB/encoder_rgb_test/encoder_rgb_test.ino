/*
  Rotary Encoder + RGB test
  HW-040 encoder on CLK, DT, SW, +5, GND
  Common-cathode RGB on 11 (R), 12 (G), 13 (B)
*/
  
// encoder pins
const uint8_t ENC_CLK = 9;
const uint8_t ENC_DT  = 8;

// rgb LED pins (common cathode)
const uint8_t RED_PIN   = 11;
const uint8_t GREEN_PIN = 12;
const uint8_t BLUE_PIN  = 13;

// state
volatile long RotPosition = 0;
int lastCLK = LOW;
int lastDecade = 0;

// a small palette of “decade” colors to cycle through
const uint8_t colorTable[][3] = {
  {255,   0,   0},  //  0 → red
  {  0, 255,   0},  //  1 → green
  {  0,   0, 255},  //  2 → blue
  {255, 255,   0},  //  3 → yellow
  {  0, 255, 255},  //  4 → cyan
  {255,   0, 255},  //  5 → magenta
  {255, 255, 255},  //  6 → white
};
const uint8_t NUM_COLORS = sizeof(colorTable) / sizeof(colorTable[0]);

void setup() {
  Serial.begin(9600);
  
  // encoder inputs
  pinMode(ENC_CLK, INPUT);
  pinMode(ENC_DT,  INPUT);
  lastCLK = digitalRead(ENC_CLK);
  
  // RGB outputs
  pinMode(RED_PIN,   OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN,  OUTPUT);
  
  // initialize LED off
  setRGB(0,0,0);
}

void loop() {
  // 1) read CLK and see if it changed
  int clk = digitalRead(ENC_CLK);
  if (clk != lastCLK) {
    // rotation happened: check DT to see direction
    if (digitalRead(ENC_DT) != clk) {
      RotPosition++;
      Serial.println("→ Clockwise");
    } else {
      RotPosition--;
      Serial.println("← Counter-clockwise");
    }
    Serial.print("Position = ");
    Serial.println(RotPosition);
    
    // 2) if we've crossed another 10-step boundary, cycle decade color
    int thisDecade = RotPosition / 10;
    if (thisDecade != lastDecade) {
      lastDecade = thisDecade;
      uint8_t idx = ( (thisDecade % NUM_COLORS) + NUM_COLORS ) % NUM_COLORS;
      auto &c = colorTable[idx];
      Serial.print(">> Decade "); Serial.print(thisDecade);
      Serial.print(" → Color idx "); Serial.println(idx);
      setRGB(c[0], c[1], c[2]);
    }
  }
  lastCLK = clk;
  
  // a tiny delay to debounce
  delay(2);
}

// helper: drive the common-cathode RGB LED
// r,g,b from 0–255; since pins 11/12/13 are NOT PWM on every board, 
// we’ll threshold at 128 for on/off
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(RED_PIN,   r > 128 ? HIGH : LOW);
  digitalWrite(GREEN_PIN, g > 128 ? HIGH : LOW);
  digitalWrite(BLUE_PIN,  b > 128 ? HIGH : LOW);
}
