// Relay Pin
#define RELAY_PIN 5 // Single relay controlling all motors (Active LOW)
#define RELAY_PIN1 6 // Single relay controlling all motors (Active LOW)

// Ultrasonic Sensor Pins
#define TRIG_PIN 4
#define ECHO_PIN 3

// IR Sensor Pin
#define IR_SENSOR_PIN 2 // IR sensor for crack detection (Active LOW)

// Buzzer Pin
#define BUZZER_PIN 10 // Buzzer for alert

// Distance Threshold for Obstacle Detection (in cm)
#define OBSTACLE_DISTANCE 20

void setup() {
  // Relay Setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Turn off the relay initially
  pinMode(RELAY_PIN1, OUTPUT);
  digitalWrite(RELAY_PIN1, HIGH); // Turn off the relay initially
  // Ultrasonic Sensor Setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // IR Sensor Setup
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  // Buzzer Setup
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Serial Monitor for Debugging
  Serial.begin(9600);
  Serial.println("Train System Initialized.");
}

void loop() {
  // Check for obstacle
  float distance = getUltrasonicDistance();
  if (distance > 0 && distance <= OBSTACLE_DISTANCE) {
    Serial.println("Obstacle Detected! Stopping train...");
    stopTrain();
    alertSystem();
    while (getUltrasonicDistance() <= OBSTACLE_DISTANCE) {
      // Wait until the obstacle is removed
    }
    Serial.println("Obstacle removed. Resuming train...");
    //startTrain();
  }

  // Check for crack detection
  else if (digitalRead(IR_SENSOR_PIN) == HIGH) { // IR Sensor Active LOW
    Serial.println("Crack Detected! Stopping++++++ train...");
    stopTrain();
    alertSystem();
    while (digitalRead(IR_SENSOR_PIN) == HIGH) {
      // Wait until the issue is resolved
    }
    Serial.println("Crack issue resolved. Resuming train...");
    startTrain();
  }
  else{
  // Default behavior: Keep the train moving
  startTrain();
}}

// Function to measure distance using ultrasonic sensor
float getUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2); // Non-blocking small delay
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); // Non-blocking small delay
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms
  if (duration == 0) {
    return 1000; // Assume no obstacle if no echo is received
  }
  float distance = duration * 0.034 / 2; // Convert duration to distance in cm
  return distance;
}

// Function to start the train
void startTrain() {
  digitalWrite(RELAY_PIN, LOW); // Activate relay (Active LOW)
  digitalWrite(RELAY_PIN1, HIGH); // Activate relay (Active LOW)
  Serial.println("Train is moving...");
}

// Function to stop the train
void stopTrain() {
  digitalWrite(RELAY_PIN, HIGH); // Deactivate relay (Active LOW)
  digitalWrite(RELAY_PIN1, LOW); // Deactivate relay (Active LOW)
  Serial.println("Train is stopped.");
}

// Function to alert the system (buzzer)
void alertSystem() {
  digitalWrite(BUZZER_PIN, HIGH); // Activate buzzer
  delay(1000); // Beep for 1 second
  digitalWrite(BUZZER_PIN, LOW); // Deactivate buzzer
}
