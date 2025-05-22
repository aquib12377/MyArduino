#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>

TMRpcm audio;
const int irPin   = A3;   // IR module signal pin (active LOW when object detected)
const int sdCardCS = 10; // SD module CS pin
const char* filename = "sound.wav";

void setup() {
  pinMode(irPin, INPUT_PULLUP);
  Serial.begin(9600);

  if (!SD.begin(sdCardCS)) {
    Serial.println("SD fail");
    while (1);
  }

  audio.speakerPin = 9;   // D9 is default PWM pin
  audio.setVolume(5);     // 0 (quiet) to 7 (loud)
  audio.quality(true);    // higher quality sampling
  Serial.println("Setup done");
}

void loop() {
  bool objectDetected = (digitalRead(irPin) == LOW);

  if (objectDetected) {
    // Object in front: start playing if not already
    if (!audio.isPlaying()) {
      Serial.println("Object detected – playing sound");
      audio.play(filename);
    }
  } 
  else {
    // No object: stop playback immediately if it’s playing
    if (audio.isPlaying()) {
      Serial.println("No object – stopping sound");
      audio.stopPlayback();
    }
  }
}
