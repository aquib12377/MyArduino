#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <TMRpcm.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <FastLED.h>

Adafruit_MPU6050 mpu;

const int switch1Pin = 2;
const int switch3Pin = 4;

#define LED_PIN 6
#define NUM_LEDS 10
CRGB leds[NUM_LEDS];

#define HC05_RX 7
#define HC05_TX 8
SoftwareSerial hc05(HC05_RX, HC05_TX);  // RX, TX for HC-05

#define SD_CS 10
TMRpcm audio;

// Variables to track switch 1 presses
int switch1PressCount = 0;
bool switch1LastState = HIGH;

// Variables for Switch 3 ON/OFF state
bool switch3On = false;
bool switch3LastState = HIGH;

// Track the current LED color for Switch 1
CRGB currentColor = CRGB::Black;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize SoftwareSerial for HC-05
  hc05.begin(9600);  // HC-05 baud rate
  Serial.println(F("HC-05 Bluetooth initialized."));

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD Card initialization failed!"));
    while (1)
      ;
  }
  Serial.println(F("SD Card initialized."));

  // Initialize audio
  audio.speakerPin = 9;  // PWM pin for audio output
  Serial.println(F("Audio initialized."));

  // Try to initialize the MPU6050
  if (!mpu.begin()) {
    Serial.println(F("Failed to find MPU6050 chip"));
    while (1) {
      delay(10);
    }
  }
  Serial.println(F("MPU6050 Found!"));

  // Setup motion detection
  mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
  mpu.setMotionDetectionThreshold(5);
  mpu.setMotionDetectionDuration(80);
  mpu.setInterruptPinLatch(true);  // Keep it latched. Will turn off when reinitialized.
  mpu.setInterruptPinPolarity(true);
  mpu.setMotionInterrupt(true);

  // Set the switch pins as inputs with pull-up resistors
  pinMode(switch1Pin, INPUT_PULLUP);
  pinMode(switch3Pin, INPUT_PULLUP);

  // Initialize the LED strip using FastLED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();  // Initialize all LEDs to off
  FastLED.show();

  Serial.println(F(""));
}

void loop() {
  // Read the switches (active LOW)
  bool switch1State = digitalRead(switch1Pin);
  bool switch3State = digitalRead(switch3Pin);
  // Handle Switch 1: Cycle through four colors on consecutive presses
  if (switch1State == LOW && switch1LastState == HIGH) {
    switch1PressCount = (switch1PressCount + 1) % 4;
    CRGB colors[] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow };
    currentColor = colors[switch1PressCount];

    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = currentColor;
      FastLED.show();
      delay(5);  // Running effect
    }

    // Play 1.wav when Switch 1 is pressed

  }

  // Handle Switch 3: Toggle ON/OFF state
  else if (switch3State == LOW && switch3LastState == HIGH) {

    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Magenta;
      FastLED.show();
      delay(5);  // Running effect
    }
    audio.play("1.wav");
  } else if (switch3State == HIGH && switch3LastState == LOW) {
    FastLED.clear();
    FastLED.show();
  }
  switch3LastState = switch3State;
  switch1LastState = switch1State;

  // Handle Motion Detection: Show white temporarily, then revert to the current color
  if (mpu.getMotionInterruptStatus()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::White;
      FastLED.show();
      delay(5);  // Running effect
    }

    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = currentColor;
      FastLED.show();
      delay(5);  // Running effect
    }
  }

  // Handle HC-05: Play 2.wav on command
  if (hc05.available()) {
    String command = hc05.readStringUntil('\n');
    command.trim();
    Serial.println(command);
    if (command == "p") {
      delay(100);
      Serial.println(F("Playing"));
      audio.play("2.wav");
      while (audio.isPlaying()) {
        delay(10);
      }
      delay(5000);
    }
  }

  // Add a short delay
  delay(200);
}
