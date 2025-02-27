#include <Adafruit_NeoPixel.h>

#define LED_PIN     22   // Pin where the data line is connected
#define LED_COUNT   32  // Number of LEDs in the strip

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    strip.begin();  // Initialize the NeoPixel library
    strip.show();   // Turn off all LEDs
    strip.setBrightness(100);  // Set brightness (0-255)
}

void loop() {
    colorWipe(strip.Color(255, 255, 255), 10 ); // Blue
}

// Fill the LEDs one by one with a color
void colorWipe(uint32_t color, int wait) {
    for(int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
        strip.show();
        delay(wait);
    }
}

// Display a rainbow effect
void rainbowCycle(int wait) {
    for(long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256) {
        for(int i = 0; i < strip.numPixels(); i++) {
            int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
            strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        strip.show();
        delay(wait);
    }
}