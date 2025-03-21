#include <Keyboard.h>
#include <Adafruit_NeoPixel.h>

// IR Setup
const int inputPin = 3;  // IR receiver connected to pin 4
int lastInputState = HIGH;
int currentState;
unsigned long lastChangeTime = 0;
int ledIndex = 0;
char keypressToSend = '1';
// LED Setup
#define LED_PIN 2      // Pin where WS2812 is connected
#define LED_COUNT 120  // Total number of LEDs

// Relay Setup
#define RELAY_PIN 4  // Relay control pin (choose your pin)
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pinMode(inputPin, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Ensure relay starts off (ACTIVE LOW)

  Serial.begin(9600);
  Keyboard.begin();

  // Initialize LED strip
  strip.begin();
  strip.show();              // Ensure all LEDs start off
  strip.setBrightness(100);  // Set brightness (0-255)
  turnOffAllLeds();
}

void turnOffAllLeds() {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, 0);
    strip.show();
    delay(50);
  }
}

void loop() {
  // Read the current state of IR input
  currentState = digitalRead(inputPin);

  // Detect state change (IR pulse)
  if (currentState != lastInputState && (millis() - lastChangeTime > 300 & ledIndex <= 119)) {
    lastChangeTime = millis();  // Update debounce timer
    Serial.println("IR signal detected!");


    // Send '1' keystroke
    Keyboard.print(keypressToSend);
    delay(5);
    Keyboard.release(keypressToSend);

    // Light up the next LED in purple
    strip.setPixelColor(ledIndex, strip.Color(128, 0, 128));
    strip.show();
    Serial.println(ledIndex);
    ledIndex++;

    // Check if LED 120 is reached
    if (ledIndex == 119) {  // LED index starts from 0, so LED 120 is index 119
      Serial.println("LED 120 reached! Activating relay...");
      digitalWrite(RELAY_PIN, LOW);  // Turn ON relay (ACTIVE 11111111111111111111LOW)
    }
    strip.show();
  }

  // Update last state
  lastInputState = currentState;
  delay(10);  // Debounce delay
}
