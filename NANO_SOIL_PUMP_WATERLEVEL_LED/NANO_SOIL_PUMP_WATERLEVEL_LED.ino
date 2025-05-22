// Pin configuration
const int soilMoisturePin = 2;     // Digital pin from soil moisture sensor
const int waterLevelPin   = A0;    // Analog pin from water level sensor
const int relayPin        = 3;     // Relay control pin (active LOW)
const int ledPin          = 4;     // LED for water level warning

// Threshold for analog water level sensor (tune this as per your sensor range)
const int waterLevelThreshold = 300; // Below this = empty, above = full

void setup() {
  pinMode(soilMoisturePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(relayPin, HIGH);  // Turn OFF relay initially (active LOW)
  digitalWrite(ledPin, LOW);     // Turn OFF LED initially

  Serial.begin(9600);
}

void loop() {
  int soilMoistureState = digitalRead(soilMoisturePin);  // LOW = dry
  int waterLevelValue   = analogRead(waterLevelPin);     // 0–1023

  Serial.print("Soil: ");
  Serial.print(soilMoistureState == LOW ? "Dry" : "Wet");
  Serial.print(" | Water Level: ");
  Serial.print(waterLevelValue);
  Serial.print(" (");
  Serial.print(waterLevelValue < waterLevelThreshold ? "Empty" : "Full");
  Serial.println(")");

  if (waterLevelValue < waterLevelThreshold) {
    digitalWrite(ledPin, HIGH);     // Turn on alert LED
    digitalWrite(relayPin, HIGH);   // Turn OFF relay (no watering)
  } else {
    digitalWrite(ledPin, LOW);      // Turn off alert LED

    // Only water if soil is dry
    if (soilMoistureState == HIGH) {
      digitalWrite(relayPin, LOW);  // Turn ON relay (start watering)
    } else {
      digitalWrite(relayPin, HIGH); // Turn OFF relay
    }
  }

  delay(1000);  // Delay before next read
}
