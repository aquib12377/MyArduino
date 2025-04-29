#include <SoftwareSerial.h>

const int fsrPin = A0;               // FSR sensor connected to analog pin A0
const int threshold = 100;           // Adjust this threshold based on your pressure needs

// HC-12 connected to SoftwareSerial (RX = pin 2, TX = pin 3)
SoftwareSerial hc12(2, 3);           // RX, TX

void setup() {
  Serial.begin(9600);               // USB Serial for debugging
  hc12.begin(9600);                 // HC-12 baud rate

  pinMode(fsrPin, INPUT);
  delay(1000);
  Serial.println("System ready. Waiting for pressure...");
}

void loop() {
  int fsrReading = analogRead(fsrPin);

  if (fsrReading > threshold) {
    Serial.print("Pressure detected: ");
    Serial.println(fsrReading);

    // Send message to HC-12
    hc12.print("Pressure: ");
    hc12.println(fsrReading);

    delay(500);  // Short delay to prevent flooding the serial
  }
}
