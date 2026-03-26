#include <Adafruit_NeoPixel.h>
#include <NewPing.h>

// Pin definitions
#define TRIG_PIN A0
#define ECHO_PIN A1
#define RING_PIN 10

// LED configuration
#define RING_LEDS 300        // Adjust based on your ring
#define DETECTION_DISTANCE 10  // cm - adjust for sensitivity
#define LIGHT_ON_DELAY 5000    // 5 seconds delay before ring turns ON

NewPing sonar(TRIG_PIN, ECHO_PIN, 200); // NewPing setup of pins and maximum distance.

// NeoPixel object - ONLY RING
Adafruit_NeoPixel ring = Adafruit_NeoPixel(RING_LEDS, RING_PIN, NEO_GRB + NEO_KHZ800);

// State variables
bool handDetectedOnce = false;
bool countdownStarted = false;
bool ringLightOn = false;
unsigned long countdownStartTime = 0;
unsigned long lastDistanceCheck = 0;

// Single color (you can change this to any color you want)
struct Color {
  uint8_t r, g, b;
};

Color ringColor = {220, 20, 60};  // Crimson Red - change as needed
// Other options:
// {0, 255, 255}    // Cyan
// {180, 120, 220}  // Soft Purple
// {255, 255, 255}  // White
// {255, 215, 0}    // Gold

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=================================");
  Serial.println("Ring Light Circuit Starting...");
  Serial.println("=================================");
  
  // Ultrasonic setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.print("Ultrasonic pins configured: TRIG=");
  Serial.print(TRIG_PIN);
  Serial.print(", ECHO=");
  Serial.println(ECHO_PIN);
  
  // Initialize ring
  Serial.print("Initializing Ring Light on pin ");
  Serial.print(RING_PIN);
  Serial.print(" with ");
  Serial.print(RING_LEDS);
  Serial.println(" LEDs...");
  ring.begin();
  ring.setBrightness(120);  // Adjust if too dim
  ring.show();
  Serial.println("Ring initialized ✓");
  
  ring.clear();
  ring.show();
  
  Serial.println("\n=================================");
  Serial.println("✓ Ring Light Circuit Ready!");
  Serial.println("=================================");
  Serial.print("Detection distance: ");
  Serial.print(DETECTION_DISTANCE);
  Serial.println(" cm");
  Serial.print("Light ON delay: ");
  Serial.print(LIGHT_ON_DELAY / 1000);
  Serial.println(" seconds AFTER hand detection");
  Serial.print("Ring color (RGB): ");
  Serial.print(ringColor.r);
  Serial.print(",");
  Serial.print(ringColor.g);
  Serial.print(",");
  Serial.println(ringColor.b);
  Serial.println("\nWaiting for hand detection...\n");
}

void loop() {
  // Check distance every 100ms to avoid spam
  if (millis() - lastDistanceCheck >= 100) {
    lastDistanceCheck = millis();
    float distance = getDistance();
    
    // Debug: Print distance (comment out if too spammy)
    if (distance > 0 && distance < 50) {  // Only print if something nearby
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
    }
    
    // Hand detection logic - only trigger once
    if (distance > 0 && distance < DETECTION_DISTANCE && !handDetectedOnce && !countdownStarted && !ringLightOn) {
      Serial.println("\n>>> HAND DETECTED! <<<");
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
      Serial.print("Starting ");
      Serial.print(LIGHT_ON_DELAY / 1000);
      Serial.println(" second countdown...");
      Serial.println("(You can remove your hand now)");
      
      handDetectedOnce = true;
      countdownStarted = true;
      countdownStartTime = millis();
    }
  }
  
  // Countdown timer - runs independently after hand detection
  if (countdownStarted && !ringLightOn) {
    unsigned long elapsed = millis() - countdownStartTime;
    
    // Show countdown every second
    static unsigned long lastCountdownPrint = 0;
    if (elapsed / 1000 > lastCountdownPrint && elapsed < LIGHT_ON_DELAY) {
      lastCountdownPrint = elapsed / 1000;
      Serial.print("⏱ Time remaining: ");
      Serial.print((LIGHT_ON_DELAY - elapsed) / 1000);
      Serial.println(" seconds...");
    }
    
    // 5 seconds elapsed - turn on ring light
    if (elapsed >= LIGHT_ON_DELAY) {
      turnOnRingLight();
    }
  }
}

float getDistance() {
  float distance = sonar.ping_cm();
  return distance;
}

void turnOnRingLight() {
  Serial.println("\n*** 5 SECONDS ELAPSED - TURNING ON RING LIGHT ***");
  Serial.print("Color: RGB(");
  Serial.print(ringColor.r);
  Serial.print(",");
  Serial.print(ringColor.g);
  Serial.print(",");
  Serial.print(ringColor.b);
  Serial.println(")");
  
  ringLightOn = true;
  countdownStarted = false;
  
  Serial.println("Setting all LEDs...");
  
  // OPTION 1: Set all LEDs at once (FASTEST - RECOMMENDED)
  uint32_t color = ring.Color(ringColor.r, ringColor.g, ringColor.b);
  for (int i = 0; i < 300; i++) {
    Serial.println(i);
    ring.setPixelColor(i, color);
  }
  ring.show();  // Show once after setting all
  
  /* OPTION 2: Progressive fill with visual effect (SLOWER)
  for (int i = 0; i < RING_LEDS; i++) {
    ring.setPixelColor(i, ring.Color(ringColor.r, ringColor.g, ringColor.b));
    
    // Show every 10 LEDs for smooth fill effect
    if (i % 10 == 0 || i == RING_LEDS - 1) {
      ring.show();
      
      // Print progress
      if (i % 50 == 0) {
        Serial.print("  LEDs lit: ");
        Serial.print(i);
        Serial.print("/");
        Serial.println(RING_LEDS);
      }
    }
  }
  */
  
  Serial.println("✓ Ring light: ON");
  Serial.print("Total LEDs lit: ");
  Serial.println(RING_LEDS);
  
  // Verify by checking a few LEDs
  Serial.println("\nVerifying LEDs...");
  Serial.print("LED 0 color: ");
  Serial.println(ring.getPixelColor(0), HEX);
  Serial.print("LED 149 (middle) color: ");
  Serial.println(ring.getPixelColor(149), HEX);
  Serial.print("LED 299 (last) color: ");
  Serial.println(ring.getPixelColor(299), HEX);
  
  Serial.println("\n=================================");
  Serial.println("Ring will stay ON indefinitely");
  Serial.println("=================================");
  Serial.println("To reset: Power cycle the device\n");
  
  // Optional: Auto turn off and reset after some time
  // Uncomment below if you want it to auto-reset
  /*
  delay(10000);  // Stay on for 10 seconds
  Serial.println("\n*** AUTO-RESET ***");
  Serial.println("Turning OFF ring light...");
  ring.clear();
  ring.show();
  handDetectedOnce = false;
  ringLightOn = false;
  Serial.println("✓ Reset complete - Ready for next detection!\n");
  */
}