#include "Adafruit_APDS9960.h"
#include <FastLED.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TRIG_PIN 3    // Ultrasonic sensor trigger pin
#define ECHO_PIN 4    // Ultrasonic sensor echo pin
#define LED_PIN 9     // WS2812 LED data pin
#define NUM_LEDS 300  // Total number of WS2812 LEDs in the strip

Adafruit_APDS9960 apds;
CRGB leds[NUM_LEDS];  // FastLED array for LED colors
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C LCD with address 0x27 and 16x2 size

unsigned long lastGestureTime = 0;          // Time of the last gesture detected
const unsigned long gestureCooldown = 500; // Cooldown period in ms

unsigned long lastBrightnessUpdateTime = 0;          // Time of the last brightness update
const unsigned long brightnessUpdateInterval = 500;  // Brightness update interval in ms

void setup() {
  Serial.begin(115200);

  // Initialize the APDS9960
  if (!apds.begin()) {
    Serial.println("Failed to initialize APDS9960! Please check your wiring.");
  } else {
    Serial.println("APDS9960 initialized!");
  }
  apds.enableProximity(true);
  apds.enableGesture(true);

  // Initialize the ultrasonic sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize FastLED
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);  // Initial brightness
  FastLED.clear();             // Turn off all LEDs initially
  FastLED.show();

  // Initialize the LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();
}

void loop() {
  unsigned long currentTime = millis();

  // Handle gesture detection with a cooldown
  if (currentTime - lastGestureTime > gestureCooldown) {
    uint8_t gesture = apds.readGesture();
    if (gesture != 0) {  // If a gesture is detected
      handleGesture(gesture);
      lastGestureTime = currentTime;
    }
  }

  // Update brightness every 500 milliseconds
  if (currentTime - lastBrightnessUpdateTime >= brightnessUpdateInterval) {
    lastBrightnessUpdateTime = currentTime;

    int distance = measureDistance();
    int brightness = map(distance, 5, 50, 0, 255);  // Map 5-50 cm to 0-255 brightness
    brightness = constrain(brightness, 0, 255);     // Ensure brightness is within 0-255

    FastLED.setBrightness(brightness);
    FastLED.show();  // Update the LED strip with new brightness

    Serial.print("Brightness: ");
    Serial.println(brightness);
  }
}

// Function to handle gestures
void handleGesture(uint8_t gesture) {
  switch (gesture) {
    case APDS9960_DOWN:
      Serial.println("DOWN");
      setLEDColor(255, 0, 0);  // Red for DOWN
      updateLCD("ANGRY MODE");
      break;
    case APDS9960_UP:
      Serial.println("UP");
      setLEDColor(0, 255, 0);  // Green for UP
      updateLCD("PARTY MODE");
      break;
    case APDS9960_LEFT:
      Serial.println("LEFT");
      setLEDColor(0, 0, 255);  // Blue for LEFT
      updateLCD("PEACE MODE");
      break;
    case APDS9960_RIGHT:
      Serial.println("RIGHT");
      setLEDColor(255, 255, 0);  // Yellow for RIGHT
      updateLCD("HAPPY MODE");
      break;
    default:
      break;
  }
}

// Function to measure distance using the ultrasonic sensor
int measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);  // Timeout after 30ms
  int distance = duration * 0.034 / 2;             // Convert time to distance (cm)

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  return (distance > 0) ? distance : 50;  // Return 50 cm if no valid reading
}

// Function to set LEDs to a specific color
void setLEDColor(int red, int green, int blue) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(red, green, blue);
  }
  FastLED.show();
}

// Function to update the LCD with a message
void updateLCD(const char *message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
}