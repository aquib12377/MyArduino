#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <Adafruit_NeoPixel.h>

Adafruit_MPU6050 mpu;

const int switch1Pin = 2;
const int switch3Pin = 4;

#define LED_PIN 6
#define NUM_LEDS 150
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define HC05_RX 7
#define HC05_TX 8
SoftwareSerial hc05(HC05_RX, HC05_TX);  // RX, TX for HC-05

// Variables to track switch 1 presses
int switch1PressCount = 0;
bool switch1LastState = HIGH;

// Variables for Switch 3 ON/OFF state
bool switch3On = false;
bool switch3LastState = HIGH;

// Track the current LED color for Switch 1
uint32_t currentColor = strip.Color(0, 0, 0);  // Initial color is black (off)

unsigned long lastMillis = 0;
unsigned long interval = 2;  // Interval for running effect (color change speed)

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize SoftwareSerial for HC-05
  hc05.begin(9600);  // HC-05 baud rate
  Serial.println(F("HC-05 Bluetooth initialized."));

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
  mpu.setMotionDetectionThreshold(50);
  mpu.setMotionDetectionDuration(150);
  mpu.setInterruptPinLatch(true);  // Keep it latched. Will turn off when reinitialized.
  mpu.setInterruptPinPolarity(true);
  mpu.setMotionInterrupt(true);

  // Set the switch pins as inputs with pull-up resistors
  pinMode(switch1Pin, INPUT_PULLUP);
  pinMode(switch3Pin, INPUT_PULLUP);

  // Initialize the LED strip using Adafruit NeoPixel
  strip.begin();
  strip.show();  // Initialize all LEDs to off
  Serial.println(F("LEDs initialized."));
}

void loop() {
  // Read the switches (active LOW)
  bool switch1State = digitalRead(switch1Pin);
  bool switch3State = digitalRead(switch3Pin);

  // Handle Switch 1: Cycle through four colors on consecutive presses
  if (switch1State == LOW && switch1LastState == HIGH) {
    switch1PressCount = (switch1PressCount + 1) % 4;
    uint32_t colors[] = { strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255), strip.Color(255, 255, 0) };
    currentColor = colors[switch1PressCount];
    
    // Apply the color to the two parts of the strip
    for (int i = 0; i <= 75; i++) {
      strip.setPixelColor(i, currentColor);  // First part
      strip.setPixelColor(NUM_LEDS - 1 - i, currentColor);  // Second part
          strip.show();

    }
  }

  // Handle Switch 3: Toggle ON/OFF state
  if (switch3State == LOW && switch3LastState == HIGH) {
    currentColor = strip.Color(255, 0, 255);  // Magenta
    for (int i = 0; i <= 75; i++) {
      strip.setPixelColor(i, currentColor);  // First part
      strip.setPixelColor(NUM_LEDS - 1 - i, currentColor);  // Second part
      
    strip.show();
    }
  } else if (switch3State == HIGH && switch3LastState == LOW) {
    strip.clear();
    strip.show();
  }

  // Handle Motion Detection: Show white temporarily, then revert to the running effect
  if (mpu.getMotionInterruptStatus()) {
    for (int i = 0; i <= 75; i++) {
      strip.setPixelColor(i, strip.Color(255, 255, 255));  // White for First part
      strip.setPixelColor(NUM_LEDS - 1 - i, strip.Color(255, 255, 255));  // White for Second part
    }
      strip.show();
    
    delay(1000);  // Show white for a moment

    // Revert to the running effect
    for (int i = 0; i <= 75; i++) {
      strip.setPixelColor(i, currentColor);  // Revert First part
      strip.setPixelColor(NUM_LEDS - 1 - i, currentColor);  // Revert Second part
          strip.show();

    }
    strip.show();
  }

  // Handle HC-05: Placeholder for future functionality
  if (hc05.available()) {
    String command = hc05.readStringUntil('\n');
    command.trim();
    Serial.println(command);
    if (command == "a") {
      for (int i = 0; i <= 75; i++) {
        strip.setPixelColor(i, strip.Color(50, 255, 100));  // Green for First part
        strip.setPixelColor(NUM_LEDS - 1 - i, strip.Color(50, 255, 100));  // Green for Second part
            strip.show();

      }
      strip.show();
    } else if (command == "b") {
      for (int i = 0; i <= 75; i++) {
        strip.setPixelColor(i, strip.Color(255, 100, 100));  // Red for First part
        strip.setPixelColor(NUM_LEDS - 1 - i, strip.Color(255, 100, 100));  // Red for Second part
            strip.show();

      }
      strip.show();
    } else if (command == "c") {
      for (int i = 0; i <= 75; i++) {
        strip.setPixelColor(i, strip.Color(150, 150, 200));  // Purple for First part
        strip.setPixelColor(NUM_LEDS - 1 - i, strip.Color(150, 150, 200));  // Purple for Second part
            strip.show();

      }
      strip.show();
    }
  }

  // Store previous switch states
  switch3LastState = switch3State;
  switch1LastState = switch1State;
}
