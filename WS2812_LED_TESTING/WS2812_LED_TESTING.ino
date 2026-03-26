#include <Adafruit_NeoPixel.h>

// Pin definition
#define LED_PIN 2

// LED configuration
#define NUM_LEDS 300  // Your LED count

// NeoPixel object
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  Serial.println("\n=================================");
  Serial.println("ESP32 LED Strip Test - Pin 33");
  Serial.println("=================================");
  
  strip.begin();
  strip.setBrightness(200);
  strip.show(); // Initialize all pixels to 'off'
  
  Serial.print("Testing ");
  Serial.print(NUM_LEDS);
  Serial.print(" LEDs on pin ");
  Serial.println(LED_PIN);
  Serial.println("\nStarting color test...\n");
}

void loop() {
  // Test 1: Red
  Serial.println("Test 1: RED");
  colorWipe(strip.Color(255, 0, 0), 10);
  delay(1000);
  
  // Test 2: Green
  Serial.println("Test 2: GREEN");
  colorWipe(strip.Color(0, 255, 0), 10);
  delay(1000);
  
  // Test 3: Blue
  Serial.println("Test 3: BLUE");
  colorWipe(strip.Color(0, 0, 255), 10);
  delay(1000);
  
  // Test 4: White
  Serial.println("Test 4: WHITE");
  colorWipe(strip.Color(255, 255, 255), 10);
  delay(1000);
  
  // Test 5: Rainbow
  Serial.println("Test 5: RAINBOW");
  rainbow(5);
  delay(1000);
  
  // Clear
  Serial.println("Clearing all LEDs...\n");
  colorWipe(strip.Color(0, 0, 0), 5);
  delay(2000);
  
  Serial.println("=================================");
  Serial.println("Test cycle complete! Repeating...\n");
}

// Fill strip with color
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
    strip.show();
    delay(wait);
  }
}

// Rainbow cycle
void rainbow(int wait) {
  for(long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(wait);
  }
}