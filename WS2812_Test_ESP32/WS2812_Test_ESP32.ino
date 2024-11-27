#include <FastLED.h>

#define NUM_LEDS 24       // Number of LEDs in your strip

//ESP32 38 Pin
#define DATA_PIN 13       // D  ata pin for the WS2812 connected to GPIO 15 on ESP32
//ESP32 30 Pin
//#define DATA_PIN 13       // D  ata pin for the WS2812 connected to GPIO 15 on ESP32

CRGB leds[NUM_LEDS];      // Define an array of CRGB objects to hold the LED data

void setup() {
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);  // Initialize the LED strip
  FastLED.setBrightness(50);
  FastLED.clear();        // Clear the LED strip initially (all LEDs off)
  FastLED.show();         // Show the initial LED state (all off)
  Serial.begin(115200);   // Start serial communication for debugging
}

void loop() {
  // Smooth RGB transition between red, green, and blue
  smoothTransition(CRGB::Red, CRGB::Cyan, 100, 20);   // Transition from red to green
  delay(1000);    // Hold green for 1 second
  smoothTransition(CRGB::Green, CRGB::Blue, 100, 20);  // Transition from green to blue
  delay(1000);    // Hold blue for 1 second
  smoothTransition(CRGB::Blue, CRGB::Red, 100, 20);    // Transition from blue to red
  delay(1000);    // Hold red for 1 second

  // Turn off all LEDs for 1 second
  FastLED.clear();
  FastLED.show();
  delay(1000);
}

// Function to perform a smooth transition between two colors
void smoothTransition(CRGB startColor, CRGB endColor, int steps, int delayMs) {
  for (int step = 0; step <= steps; step++) {
    float ratio = (float)step / steps;  // Calculate ratio for interpolation

    // Interpolate between the startColor and endColor
    CRGB currentColor = blend(startColor, endColor, ratio * 255);

    // Apply the interpolated color to all LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = currentColor;
    }

    FastLED.show();   // Update the LEDs
    delay(delayMs);   // Delay between each step to create the smooth transition effect
  }
}
