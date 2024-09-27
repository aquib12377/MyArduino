#include <FastLED.h>
#include <Keyboard.h>

#define LED_PIN 2
#define NUM_LEDS 600
#define IR_PIN 3

CRGB leds[NUM_LEDS];
int currentLED = 0;  // Track the current LED to turn on
int lastVal = 0;
unsigned long lastChangeTime = 0;  // Track the last time the IR signal changed
const unsigned long timeout = 15000;  // 20 seconds

void setup() {
  pinMode(IR_PIN, INPUT_PULLUP);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  // Initially turn off all LEDs
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  Serial.begin(9600);
  FastLED.clear();
  Keyboard.begin();
  lastChangeTime = millis();  // Initialize the last change time
}

void loop() {
  int currentVal = digitalRead(IR_PIN);
  unsigned long currentTime = millis();

  if (currentVal != lastVal) {  // IR signal changed
    lastVal = currentVal;
    lastChangeTime = currentTime;  // Reset the timer

    if (currentLED < NUM_LEDS) {
      Serial.println(currentLED);
      leds[currentLED] = CRGB(0, 255, 0);  // Turn on the current LED
      FastLED.show();
      currentLED++;  // Move to the next LED
        Keyboard.press('1');
        delay(5);  // Adjust delay as needed
        Keyboard.release('1');
    }
  }

  // Check if the IR signal hasn't changed for the timeout duration
  if (currentTime - lastChangeTime >= timeout) {
    Serial.println("Resetted");
    // Reset all LEDs and the currentLED counter
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    currentLED = 0; 
    lastChangeTime = currentTime;  // Reset the timer
  }
}
