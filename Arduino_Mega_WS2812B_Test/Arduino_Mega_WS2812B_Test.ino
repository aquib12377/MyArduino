#include <FastLED.h>

// Define the number of LEDs per strip
#define NUM_LEDS 32

// Define data pins for each strip
#define DATA_PIN1 7
#define DATA_PIN2 23
#define DATA_PIN3 6
#define DATA_PIN4 25
#define DATA_PIN5 5
#define DATA_PIN6 27
#define DATA_PIN7 4
#define DATA_PIN8 29
#define DATA_PIN9 3
#define DATA_PIN10 31
#define DATA_PIN11 2
#define DATA_PIN12 33
#define DATA_PIN13 1
#define DATA_PIN14 35
#define DATA_PIN15 0
#define DATA_PIN16 37
#define DATA_PIN17 14
#define DATA_PIN18 39
#define DATA_PIN19 15
#define DATA_PIN20 41
#define DATA_PIN21 16
#define DATA_PIN22 43
#define DATA_PIN23 17
#define DATA_PIN24 45
#define DATA_PIN25 18
#define DATA_PIN26 47
#define DATA_PIN27 19
#define DATA_PIN28 49
#define DATA_PIN29 20
#define DATA_PIN30 51
#define DATA_PIN31 21
#define DATA_PIN32 53
#define DATA_PIN33 A15
#define DATA_PIN34 A14
#define DATA_PIN35 A13
#define DATA_PIN36 A12
#define DATA_PIN37 A11
#define DATA_PIN38 A10
#define DATA_PIN39 A9
#define DATA_PIN40 A8
#define DATA_PIN41 A7
#define DATA_PIN42 A6
#define DATA_PIN43 A5
#define DATA_PIN44 A4
#define DATA_PIN45 A3
#define DATA_PIN46 A2
#define DATA_PIN47 A1
#define DATA_PIN48 A0

// Create an array of CRGB objects for each strip
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];
CRGB leds4[NUM_LEDS];
CRGB leds5[NUM_LEDS];
CRGB leds6[NUM_LEDS];
CRGB leds7[NUM_LEDS];
CRGB leds8[NUM_LEDS];
CRGB leds9[NUM_LEDS];
CRGB leds10[NUM_LEDS];
CRGB leds11[NUM_LEDS];
CRGB leds12[NUM_LEDS];
CRGB leds13[NUM_LEDS];
CRGB leds14[NUM_LEDS];
CRGB leds15[NUM_LEDS];
CRGB leds16[NUM_LEDS];
CRGB leds17[NUM_LEDS];
CRGB leds18[NUM_LEDS];
CRGB leds19[NUM_LEDS];
CRGB leds20[NUM_LEDS];
CRGB leds21[NUM_LEDS];
CRGB leds22[NUM_LEDS];
CRGB leds23[NUM_LEDS];
CRGB leds24[NUM_LEDS];
CRGB leds25[NUM_LEDS];
CRGB leds26[NUM_LEDS];
CRGB leds27[NUM_LEDS];
CRGB leds28[NUM_LEDS];
CRGB leds29[NUM_LEDS];
CRGB leds30[NUM_LEDS];
CRGB leds31[NUM_LEDS];
CRGB leds32[NUM_LEDS];
CRGB leds33[NUM_LEDS];
CRGB leds34[NUM_LEDS];
CRGB leds35[NUM_LEDS];
CRGB leds36[NUM_LEDS];
CRGB leds37[NUM_LEDS];
CRGB leds38[NUM_LEDS];
CRGB leds39[NUM_LEDS];
CRGB leds40[NUM_LEDS];
CRGB leds41[NUM_LEDS];
CRGB leds42[NUM_LEDS];
CRGB leds43[NUM_LEDS];
CRGB leds44[NUM_LEDS];
CRGB leds45[NUM_LEDS];
CRGB leds46[NUM_LEDS];
CRGB leds47[NUM_LEDS];
CRGB leds48[NUM_LEDS];

CRGB* allLeds[48] = {
  leds1, leds2, leds3, leds4, leds5, leds6, leds7, leds8,
  leds9, leds10, leds11, leds12, leds13, leds14, leds15, leds16,
  leds17, leds18, leds19, leds20, leds21, leds22, leds23, leds24,
  leds25, leds26, leds27, leds28, leds29, leds30, leds31, leds32,
  leds33, leds34,leds35,leds36,leds37,leds38,leds39,leds40,leds41,
  leds42,leds43,leds44,leds45,leds46,leds47,leds48,
};

void setup() {
  Serial.begin(115200);
  Serial.println("WS2812B Test starting....");
  // Initialize each LED strip with its respective data pin
  FastLED.addLeds<WS2812B, DATA_PIN1, GRB>(leds1, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN2, GRB>(leds2, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN3, GRB>(leds3, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN4, GRB>(leds4, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN5, GRB>(leds5, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN6, GRB>(leds6, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN7, GRB>(leds7, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN8, GRB>(leds8, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN9, GRB>(leds9, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN10, GRB>(leds10, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN11, GRB>(leds11, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN12, GRB>(leds12, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN13, GRB>(leds13, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN14, GRB>(leds14, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN15, GRB>(leds15, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN16, GRB>(leds16, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN17, GRB>(leds17, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN18, GRB>(leds18, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN19, GRB>(leds19, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN20, GRB>(leds20, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN21, GRB>(leds21, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN22, GRB>(leds22, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN23, GRB>(leds23, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN24, GRB>(leds24, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN25, GRB>(leds25, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN26, GRB>(leds26, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN27, GRB>(leds27, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN28, GRB>(leds28, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN29, GRB>(leds29, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN30, GRB>(leds30, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN31, GRB>(leds31, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN32, GRB>(leds32, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN33, GRB>(leds33, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN34, GRB>(leds34, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN35, GRB>(leds35, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN36, GRB>(leds36, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN37, GRB>(leds37, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN38, GRB>(leds38, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN39, GRB>(leds39, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN40, GRB>(leds40, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN41, GRB>(leds41, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN42, GRB>(leds42, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN43, GRB>(leds43, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN44, GRB>(leds44, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN45, GRB>(leds45, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN46, GRB>(leds46, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN47, GRB>(leds47, NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN48, GRB>(leds48, NUM_LEDS);
  
  FastLED.setBrightness(255);  // Adjust brightness level (0 to 255)
}

void loop() {
  
  Serial.println("WS2812B Test starting red");
  // Example: Fill all strips with red
  for(int i = 0; i < 48; i++) {
    fill_solid(allLeds[i], NUM_LEDS, CRGB::Red);
  }
  FastLED.show();  // Display the red color on all LEDs
  delay(1000);     // Wait for 1 second

  Serial.println("WS2812B Test starting green");
  // Example: Fill all strips with green
  for(int i = 0; i < 48; i++) {
    fill_solid(allLeds[i], NUM_LEDS, CRGB::Green);
  }
  FastLED.show();  // Display the green color on all LEDs
  delay(1000);     // Wait for 1 second

  Serial.println("WS2812B Test starting blue");
  // Example: Fill all strips with blue
  for(int i = 0; i < 48; i++) {
    fill_solid(allLeds[i], NUM_LEDS, CRGB::Blue);
  }
  FastLED.show();  // Display the blue color on all LEDs
  delay(1000);     // Wait for 1 second


  Serial.println("WS2812B Test starting rainbow");
  // Example: Simple rainbow effect applied to all strips
  rainbowEffect(2);  // Call rainbow effect function with delay between frames
}

// Function for rainbow effect applied to all strips
void rainbowEffect(uint8_t wait) {
  for(long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
    for(int i = 0; i < 48; i++) { // Iterate through all strips
      for(int j = 0; j < NUM_LEDS; j++) { // Iterate through each LED in the strip
        // Calculate hue for each pixel
        int pixelHue = firstPixelHue + (j * 65536L / NUM_LEDS);
        allLeds[i][j] = CHSV(pixelHue / 256, 255, 255);
      }
    }
    FastLED.show();
    delay(wait);
  }
}
