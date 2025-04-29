#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>

TMRpcm audio;
const int irPin = 2;         // IR module signal pin
const int sdCardCS = 10;     // SD module CS pin (could be 4 on some boards)
bool soundPlayed = false;

void setup() {
  pinMode(irPin, INPUT);
  Serial.begin(9600);

  if (!SD.begin(sdCardCS)) {
    Serial.println("SD fail");
    while (1);
  }

  audio.speakerPin = 9; // D9 is default PWM pin
  audio.setVolume(6);   // 0 to 7
  Serial.println("Setup done");
}

void loop() {
  int irValue = digitalRead(irPin); // LOW = object detected (active LOW)

  if (irValue == HIGH && !soundPlayed) {
    Serial.println("IR triggered - playing sound");
    audio.play("a.wav");
    soundPlayed = true;
  }

  if (irValue == LOW && !audio.isPlaying()) {
    soundPlayed = false; // Reset when object is gone and playback stops
  }
}
