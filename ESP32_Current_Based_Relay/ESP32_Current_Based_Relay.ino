#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3Pzl9brGw"
#define BLYNK_TEMPLATE_NAME "Power Factor"
#define BLYNK_AUTH_TOKEN "MpdgfGAKEpvZW64dAyfmxK1ZSjUq4hNA"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "ACS712.h"

 ACS712  ACS(34, 3.3, 4095, 100);

char ssid[] = "MyProject";
char pass[] = "12345678";

// Relay Pins (Active LOW)
int relayPins[4] = {13, 12, 14, 27};

// LCD I2C Setup (0x27 or 0x3F depending on your module)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Relay States & Control Flags
bool relayStates[4] = {true, true, true, true}; // true = OFF (Active LOW)
bool manualControl[4] = {false, false, false, false}; // Manual mode ON/OFF

void setup() {
    Serial.begin(115200);
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Power Factor Ctrl");

ACS.autoMidPoint();
  Serial.print("MidPoint: ");
  Serial.println(ACS.getMidPoint());
  Serial.print("Noise mV: ");
  Serial.println(ACS.getNoisemV());
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH); // Turn off all relays initially
    }
}

void loop() {
    Blynk.run();
    checkCurrentAndControlRelays();
}

float getCurrent()
{
  float average = 0;
  uint32_t start = millis();
  for (int i = 0; i < 100; i++)
  {
    //  select appropriate function
    //  average += ACS.mA_AC_sampling();
    average += ACS.mA_AC();
  }
  float mA = average / 100.0;
  uint32_t duration = millis() - start;
  Serial.print("Time: ");
  Serial.print(duration);
  Serial.print("  mA: ");
  Serial.println(mA);
  return mA/1000;
}

// Read ACS712 and Control Relays Automatically
void checkCurrentAndControlRelays() {
    float current = getCurrent();

    Serial.print("Current: ");
    Serial.print(current, 2);
    Serial.println(" A");

    // Display on LCD
    lcd.setCursor(0, 1);
    lcd.print("Current: ");
    lcd.print(current, 2);
    lcd.print(" A  ");

    // Send data to Blynk
    Blynk.virtualWrite(V5, current);

    // Auto-control relays only if manual mode is OFF
    updateRelayState(0, (current > 0.5 && current < 1));
    updateRelayState(1, (current > 1.0 && current < 2));
    updateRelayState(2, (current > 2 && current < 3));
    updateRelayState(3, (current > 3 && current < 4));

    delay(1000);
}

// Update Relay State Based on Last Action
void updateRelayState(int relayIndex, bool shouldTurnOn) {
    if (manualControl[relayIndex]) {
        return; // If Blynk controlled it last, ignore auto changes
    }

    if (shouldTurnOn && relayStates[relayIndex]) {
        relayStates[relayIndex] = false; // Relay ON
        digitalWrite(relayPins[relayIndex], LOW);
        Blynk.virtualWrite(V1 + relayIndex, 1);
    } 
    else if (!shouldTurnOn && !relayStates[relayIndex]) {
        relayStates[relayIndex] = true; // Relay OFF
        digitalWrite(relayPins[relayIndex], HIGH);
        Blynk.virtualWrite(V1 + relayIndex, 0);
    }
}

// Blynk Manual Control
BLYNK_WRITE(V0) { handleBlynkRelay(0, param.asInt()); }
BLYNK_WRITE(V1) { handleBlynkRelay(1, param.asInt()); }
BLYNK_WRITE(V2) { handleBlynkRelay(2, param.asInt()); }
BLYNK_WRITE(V3) { handleBlynkRelay(3, param.asInt()); }

// Handle Blynk Control (Overrides Auto Mode)
void handleBlynkRelay(int relayIndex, int value) {
    manualControl[relayIndex] = true;  // Set Manual Mode ON
    relayStates[relayIndex] = !value;  // Convert to Active LOW
    digitalWrite(relayPins[relayIndex], relayStates[relayIndex] ? HIGH : LOW);
    Blynk.virtualWrite(V0 + relayIndex, value);
}

// Reset Manual Mode if Auto Control is Re-enabled
void resetManualControl(int relayIndex) {
    if (relayStates[relayIndex]) { // If Relay is OFF
        manualControl[relayIndex] = false; // Allow Auto Mode Again
    }
}
