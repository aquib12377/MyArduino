#include <Adafruit_NeoPixel.h>

#define LED_PIN     2   // Pin where the data line is connected
#define LED_COUNT   120  // Number of LEDs in the strip

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
    strip.begin();  // Initialize the NeoPixel library
    strip.show();   // Turn off all LEDs
    strip.setBrightness(255);  // Set brightness (0-255)
}

void loop() {
      
    colorWipe(strip.Color(100, 0, 100), 10 ); // Blue
    delay(5000);
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
    int i = 0;
    while(i < 5)
    {
      for(long firstPixelHue = 0; firstPixelHue < 65536; firstPixelHue += 256) {
        for(int i = 0; i < strip.numPixels(); i++) {
            int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
            strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
        }
        strip.show();
        delay(wait);
    }
    i++;
    }
}

void turnOfAllLeds()
{
  for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, 0);
        strip.show();
        delay(50);
      }
}