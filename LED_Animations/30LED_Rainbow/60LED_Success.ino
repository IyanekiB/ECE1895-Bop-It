#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
#include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
class Strip
{
public:
  uint8_t   effect;
  uint8_t   effects;
  uint16_t  effStep;
  unsigned long effStart;
  Adafruit_NeoPixel strip;
  Strip(uint16_t leds, uint8_t pin, uint8_t toteffects, uint16_t striptype) : strip(leds, pin, striptype) {
    effect = -1;
    effects = toteffects;
    Reset();
  }
  void Reset(){
    effStep = 0;
    effect = (effect + 1) % effects;
    effStart = millis();
  }
};

struct Loop
{
  uint8_t currentChild;
  uint8_t childs;
  bool timeBased;
  uint16_t cycles;
  uint16_t currentTime;
  Loop(uint8_t totchilds, bool timebased, uint16_t tottime) {currentTime=0;currentChild=0;childs=totchilds;timeBased=timebased;cycles=tottime;}
};

Strip strip_0(60, 7, 60, NEO_RGB + NEO_KHZ800);
struct Loop strip0loop0(1, false, 1);

//[GLOBAL_VARIABLES]

void setup() {

  #if defined(__AVR_ATtiny85__) && (F_CPU == 8000000)
  clock_prescale_set(clock_div_1);
  #endif
  //Your setup here:

  strip_0.strip.begin();
}

void loop() {

  //Your code here:

  strips_loop();
}

void strips_loop() {
  if(strip0_loop0() & 0x01)
    strip_0.strip.show();
}

uint8_t strip0_loop0() {
  uint8_t ret = 0x00;
  switch(strip0loop0.currentChild) {
    case 0: 
           ret = strip0_loop0_eff0();break;
  }
  if(ret & 0x02) {
    ret &= 0xfd;
    if(strip0loop0.currentChild + 1 >= strip0loop0.childs) {
      strip0loop0.currentChild = 0;
      if(++strip0loop0.currentTime >= strip0loop0.cycles) {strip0loop0.currentTime = 0; ret |= 0x02;}
    }
    else {
      strip0loop0.currentChild++;
    }
  };
  return ret;
}

uint8_t strip0_loop0_eff0() {
    // Strip ID: 0 - Effect: Rainbow - LEDS: 60
    // Steps: 19 - Delay: 20
    // Colors: 3 (120.246.0, 0.255.0, 0.0.0)
    // Options: rainbowlen=51, toLeft=true, 
  if(millis() - strip_0.effStart < 20 * (strip_0.effStep)) return 0x00;
  float factor1, factor2;
  uint16_t ind;
  for(uint16_t j=0;j<60;j++) {
    ind = strip_0.effStep + j * 0.37254901960784315;
    switch((int)((ind % 19) / 6.333333333333333)) {
      case 0: factor1 = 1.0 - ((float)(ind % 19 - 0 * 6.333333333333333) / 6.333333333333333);
              factor2 = (float)((int)(ind - 0) % 19) / 6.333333333333333;
              strip_0.strip.setPixelColor(j, 120 * factor1 + 0 * factor2, 246 * factor1 + 255 * factor2, 0 * factor1 + 0 * factor2);
              break;
      case 1: factor1 = 1.0 - ((float)(ind % 19 - 1 * 6.333333333333333) / 6.333333333333333);
              factor2 = (float)((int)(ind - 6.333333333333333) % 19) / 6.333333333333333;
              strip_0.strip.setPixelColor(j, 0 * factor1 + 0 * factor2, 255 * factor1 + 0 * factor2, 0 * factor1 + 0 * factor2);
              break;
      case 2: factor1 = 1.0 - ((float)(ind % 19 - 2 * 6.333333333333333) / 6.333333333333333);
              factor2 = (float)((int)(ind - 12.666666666666666) % 19) / 6.333333333333333;
              strip_0.strip.setPixelColor(j, 0 * factor1 + 120 * factor2, 0 * factor1 + 246 * factor2, 0 * factor1 + 0 * factor2);
              break;
    }
  }
  if(strip_0.effStep >= 19) {strip_0.Reset(); return 0x03; }
  else strip_0.effStep++;
  return 0x01;
}

