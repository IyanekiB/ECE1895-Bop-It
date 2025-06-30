#include <SdFat.h>

#define SD_CS     10
#define BUFFER_SIZE 64
#define AUDIO_PIN 9

#define SWING_BUTTON 6
#define SHEATH_BUTTON 7
#define BOOST_BUTTON 8


SdFat sd;
File getAudio;

uint8_t buff[BUFFER_SIZE];


void playAudio(const char *filename){
  Serial.print("Opening file: ");
  Serial.println(filename);

  getAudio.close();
  if (!getAudio.open(filename, O_READ)){ 
  Serial.println("File Failed to open");
  return;
  }

  Serial.println("File opened. Skipping header...");
  getAudio.seekSet(44);
  Serial.println("Playing audio...");

  while(true){
    int n = getAudio.read(buff, BUFFER_SIZE);
    if (n <= 0) break;
    for(int i=0; i < n; i++){
      OCR1A = buff[i];
      delayMicroseconds(125);
    }
  }
  Serial.println("Playback finished.");
  getAudio.close();
}


void setup() {
  Serial.begin(9600);
  pinMode(AUDIO_PIN, OUTPUT);
  pinMode(SWING_BUTTON, INPUT_PULLUP);
  pinMode(SHEATH_BUTTON, INPUT_PULLUP);
  pinMode(BOOST_BUTTON, INPUT_PULLUP);

  Serial.println("Starting Setup");
  if(!sd.begin(SD_CS, SD_SCK_MHZ(8))){
    Serial.println("SD Card didnt int");
    while(1);
  }
  Serial.println("SD Card good");
  // Configure Timers for 8 bit PWN on Pin 9
  TCCR1A = _BV(COM1A1) | _BV(WGM10);
  TCCR1B = _BV(WGM12) | _BV(CS10);


}

void loop() {
  if(!digitalRead(SWING_BUTTON) {delay(20); if(!digitalRead(SWING_BUTTON)) playAudio("SWINGIT.wav");}
  if(!digitalRead(SHEATH_BUTTON)) {delay(20); if(!digitalRead(SHEATH_BUTTON)) playAudio("SHEATHIT.wav");}
  if(!digitalRead(BOOST_BUTTON)) {delay(20); if(!digitalRead(BOOST_BUTTON)) playAudio("BOOSTIT.wav");}

}