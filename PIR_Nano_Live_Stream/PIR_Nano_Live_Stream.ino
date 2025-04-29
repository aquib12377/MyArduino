/*
  Arduino Sketch: PIR Sensor to Control an Active-Low Relay

  Description:
  - When the PIR sensor (connected to pin 2) detects motion (reads HIGH),
    the relay (connected to pin 8) is activated by writing LOW (because the relay is active low).
  - The relay remains activated for 5 minutes before turning off (setting the pin HIGH again).
*/

const int pirPin = 2;      // PIR sensor output pin
const int relayPin = A0;    // Relay control pin (active low)

// Relay activation duration: 5 minutes in milliseconds
const unsigned long relayOnDuration = 120000; // 300,000 ms = 5 minutes

void setup() {
  // Initialize the PIR sensor as an input
  pinMode(pirPin, INPUT);
  
  // Initialize the relay pin as an output.
  // Set the relay pin HIGH initially to ensure the relay is off.
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Optional: Start serial communication for debugging
  Serial.begin(9600);
  Serial.println("System Initialized. Waiting for motion...");
}

void loop() {
  // Read the state of the PIR sensor
  int pirState = digitalRead(pirPin);
  
  // Check if motion is detected
  if (pirState == HIGH) {
    Serial.println("Motion detected! Activating relay...");
    
    // Activate the relay (active low: LOW = ON)
    digitalWrite(relayPin, HIGH);
    
    // Keep the relay active for 5 minutes
    delay(relayOnDuration);
    
    // Deactivate the relay (set to HIGH)
    digitalWrite(relayPin, LOW);
    Serial.println("Relay deactivated. Waiting for next motion...");
    
    // Optional: Short delay to allow the PIR sensor to settle
    delay(1000);
  }
}
