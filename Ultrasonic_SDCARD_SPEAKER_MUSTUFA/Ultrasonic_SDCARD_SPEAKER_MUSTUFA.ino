#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>

// Audio and SD
#define SD_CS 10
#define AUDIO_PIN 9
TMRpcm tmrpcm;

// Ultrasonic
#define TRIG_PIN A5
#define ECHO_PIN A4

void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card failed!");
    while (1);
  }
  Serial.println("SD Card initialized.");

  tmrpcm.speakerPin = AUDIO_PIN;
  tmrpcm.setVolume(5); // Range: 0–7
}

void loop() {
  int distance = getDistance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance < 180) {
    playSound("kbc.wav");
  }

  delay(3000); // Wait before next reading
}

int getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int cm = duration * 0.034 / 2;
  return cm;
}

void playSound(const char* filename) {
  if (!tmrpcm.isPlaying()) {
    Serial.print("Playing: ");
    Serial.println(filename);
    tmrpcm.play(filename);
  }
}
