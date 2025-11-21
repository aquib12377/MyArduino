7// E18-D20 + ACTIVE-LOW Relay control
// - Sensor output on D2
// - Relay control on D8 (ACTIVE-LOW)

const int SENSOR_PIN = 2;   // E18-D20 OUT (black wire)
const int RELAY_PIN  = 4;   // Relay IN pin

// ---- Logic levels (change here if needed) ----
const bool RELAY_ACTIVE_LEVEL   = LOW;   // Relay ON when pin is LOW
const bool RELAY_INACTIVE_LEVEL = HIGH;  // Relay OFF when pin is HIGH

// For most E18-D20 NPN sensors, output goes LOW when object is detected
const bool SENSOR_ACTIVE_LEVEL  = LOW;   // Sensor "triggered" when pin reads LOW

void setup() {
  Serial.begin(9600);

  // Use internal pull-up (safe for open-collector/NPN outputs)
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_INACTIVE_LEVEL); // Start with relay OFF

  Serial.println("E18-D20 + Relay test started");
}

void loop() {
  int rawSensor = digitalRead(SENSOR_PIN);
  bool objectDetected = (rawSensor == SENSOR_ACTIVE_LEVEL);

  if (objectDetected) {
    // Turn relay ON (remember: active-low)
    digitalWrite(RELAY_PIN, RELAY_ACTIVE_LEVEL);
    Serial.println("Object detected -> Relay ON");
  } else {
    // Turn relay OFF
    digitalWrite(RELAY_PIN, RELAY_INACTIVE_LEVEL);
    Serial.println("No object -> Relay OFF");
  }

  delay(100); // small delay for stability
}
