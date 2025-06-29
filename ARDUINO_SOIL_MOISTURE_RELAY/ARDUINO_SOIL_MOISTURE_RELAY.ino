// Arduino sketch: Control an ACTIVE LOW relay based on an analog soil moisture sensor

// Pin definitions
const int soilSensorPin = A1;    // Analog pin connected to soil moisture sensor
const int relayPin      = A0;     // Digital pin connected to relay module IN pin (active LOW)

// Threshold for soil dryness
// Modify this value based on calibration: higher means dryer soil
int moistureThreshold = 400;

void setup() {
  // Initialize relay pin as output
  pinMode(relayPin, OUTPUT);
  // Ensure relay is OFF at startup (active LOW means HIGH turns it off)
  digitalWrite(relayPin, HIGH);

  // Start serial communication for debugging
  Serial.begin(9600);
  Serial.println("Soil Moisture Relay Control Initialized");
}

void loop() {
  // Read the analog value from the soil moisture sensor
  int sensorValue = analogRead(soilSensorPin);
  Serial.print("Soil moisture analog value: ");
  Serial.println(sensorValue);

  // Compare against threshold
  if (sensorValue > moistureThreshold) {
    // Soil is too dry -> activate relay (LOW)
    digitalWrite(relayPin, LOW);
    Serial.println("Relay ON - watering...");
  } else {
    // Soil is moist enough -> deactivate relay (HIGH)
    digitalWrite(relayPin, HIGH);
    Serial.println("Relay OFF - soil damp");
  }

  // Wait before next reading
  delay(1000);
}
