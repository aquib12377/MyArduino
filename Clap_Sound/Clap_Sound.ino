#include <SD.h>
#include <TMRpcm.h>

// Sound sensor pin
const int soundSensorPin = A0;

// Audio output pin (PWM capable)
const int audioOutPin = 9;

// SD card CS pin
const int chipSelect = 4;

TMRpcm audio;  // Create an object for TMRpcm

void setup() {
  Serial.begin(9600);

  // Initialize sound sensor pin
  pinMode(soundSensorPin, INPUT);

  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD card initialized.");

  // Configure TMRpcm
  audio.speakerPin = audioOutPin;  // Set audio output pin
}

void loop() {
  int soundLevel = analogRead(soundSensorPin);

  // Debug: Print sound level
  Serial.println(soundLevel);

  // Threshold to detect sound (adjust as needed)
  if (soundLevel < 600) {  // Adjust this threshold based on your environment
    Serial.println("Sound detected! Playing audio.");

    // Play audio file from SD card
    if (!audio.isPlaying()) {
      audio.play("audio.wav");  // Replace with your audio file name
    }

    // Wait for the audio to finish playing
    while (audio.isPlaying()) {
      delay(100);
    }
  }

  //delay(500);  // Short delay to avoid rapid triggers
}
