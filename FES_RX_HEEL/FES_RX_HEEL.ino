#include <SoftwareSerial.h>

#define RELAY_PIN 5          // Connect relay IN pin here (Active LOW)
#define HC12_RX 14            // ESP8266 pin receiving from HC-12 TX
#define HC12_TX 12            // ESP8266 pin sending to HC-12 RX

SoftwareSerial hc12(HC12_RX, HC12_TX); // RX, TX for HC-12

void setup() {
  Serial.begin(9600);
  hc12.begin(9600);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // Keep relay OFF initially (Active LOW)

  Serial.println("ESP8266 ready. Waiting for pressure signal...");
}

void loop() {
  if (hc12.available()) {
    String message = hc12.readStringUntil('\n');
    message.trim();  // Remove whitespace or newline

    Serial.println("Received: " + message);

    // Simple check for the keyword "Pressure"
    if (message.startsWith("Pressure")) {
      Serial.println("Pressure detected! Activating relay...");
      digitalWrite(RELAY_PIN, LOW);  // Turn relay ON (Active LOW)
      delay(10000);                   // Keep relay ON for 2 seconds
      digitalWrite(RELAY_PIN, HIGH); // Turn relay OFF
    }
  }
}
