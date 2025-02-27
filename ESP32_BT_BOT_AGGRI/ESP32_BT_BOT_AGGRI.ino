// ============== Library Includes ==============
#include <Arduino.h>
#include <BluetoothSerial.h>

// ============== Bluetooth Setup ==============
BluetoothSerial SerialBT;
const char *btName = "Aggriculture_Bot";  // Name as it appears when scanning

// ============== Pin Definitions ==============
// Motor driver pins (Adjust according to your driver setup)
const int IN1 = 27;  // Left motor forward
const int IN2 = 14;  // Left motor backward
const int IN3 = 26;  // Right motor forward
const int IN4 = 25;  // Right motor backward

// Ultrasonic sensor pins
const int trigPin = 18;
const int echoPin = 19;

// ============== Global Variables ==============

bool autoMode = false;            // false => Manual Mode, true => Auto Mode
unsigned long lastMoveTime = 0;   // For timing movement changes in auto mode
int currentMovement = 0;          // Track the current movement pattern

// ============== Functions ==============

// Motor control helper functions
void stopMotor() {
  Serial.println("[Motor] Stopping motors...");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  Serial.println("[Motor] Moving FORWARD...");
  // Left motor forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  // Right motor forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveBackward() {
  Serial.println("[Motor] Moving BACKWARD...");
  // Left motor backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  // Right motor backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  Serial.println("[Motor] Turning LEFT...");
  // Left motor stop or backward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  // Right motor forward
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  Serial.println("[Motor] Turning RIGHT...");
  // Left motor forward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  // Right motor stop or backward
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// Ultrasonic distance measurement
long getDistanceCM() {
  // Clear trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Send 10us pulse to trigPin
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echoPin (with 30ms timeout)
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) {
    // No pulse or out of range
    Serial.println("[Ultrasonic] No valid echo received (distance > 999cm).");
    return 999; 
  }

  // Convert duration to distance in cm (round-trip time / 58)
  long distance = duration / 58;
  Serial.print("[Ultrasonic] Measured distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

// Check if an obstacle is within a certain threshold
bool obstacleDetected(int threshold = 20) {
  long distance = getDistanceCM();
  if (distance <= threshold) {
    Serial.print("[Obstacle] Detected obstacle within ");
    Serial.print(threshold);
    Serial.println(" cm!");
    return true;
  }
  return false;
}

// Handle manual mode commands from Bluetooth
void handleManualCommands(char cmd) {
  Serial.print("[Manual Command] Received: ");
  Serial.println(cmd);
  
  switch (cmd) {
    case 'F':  // Forward
      moveForward();
      break;
    case 'B':  // Backward
      moveBackward();
      break;
    case 'L':  // Left
      turnLeft();
      break;
    case 'R':  // Right
      turnRight();
      break;
    case 'S':  // Stop
      stopMotor();
      break;
    default:
      Serial.println("[Manual Command] Unknown command.");
      break;
  }
}

// Perform auto mode movement
// Simple example: cycle through forward/backward/left/right/stop
void autoModeMovement() {
  unsigned long currentTime = millis();

  // If enough time has passed, switch movement
  if (currentTime - lastMoveTime > 2000) {  // every 2 seconds
    currentMovement = (currentMovement + 1) % 5; // cycle 0..4
    lastMoveTime = currentTime;
    Serial.print("[Auto Mode] Changing movement to pattern index: ");
    Serial.println(currentMovement);
  }

  switch (currentMovement) {
    case 0: moveForward();  break;
    case 1: moveBackward(); break;
    case 2: turnLeft();     break;
    case 3: turnRight();    break;
    case 4: stopMotor();    break;
  }
}

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("Starting ESP32 Robot...");

  // Setup motor pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotor();

  // Setup ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initialize Bluetooth
  SerialBT.begin(btName);
  Serial.print("Bluetooth started. Device name: ");
  Serial.println(btName);
  Serial.println("Waiting for connections...");
}

void loop() {
  // Check if there is a Bluetooth command
  if (SerialBT.available()) {
    char incomingChar = SerialBT.read();
    Serial.print("[Bluetooth] Received char: ");
    Serial.println(incomingChar);

    // Check for mode switching
    if (incomingChar == 'A') {
      autoMode = true;
      stopMotor();
      Serial.println("[Mode] Switched to AUTO MODE");
    } else if (incomingChar == 'M') {
      autoMode = false;
      stopMotor();
      Serial.println("[Mode] Switched to MANUAL MODE");
    } 
    // If in manual mode, handle movement commands
    else if (!autoMode) {
      handleManualCommands(incomingChar);
    }
  }

  // Auto Mode Behavior
  if (autoMode) {
    if (obstacleDetected(20)) {
      // Detected an obstacle, pick a new random movement or change direction
      Serial.println("[Auto Mode] Obstacle detected! Avoiding...");
      stopMotor();
      delay(500);
      
      // For demonstration, let's turn left or right randomly
      int randomTurn = random(2); // 0 or 1
      if (randomTurn == 0) {
        turnLeft();
      } else {
        turnRight();
      }
      delay(700);
      stopMotor();
      
      // Move forward again
      moveForward();
      delay(1000);
      stopMotor();
      
      // Then continue normal pattern
      lastMoveTime = millis(); // reset the timer
      Serial.println("[Auto Mode] Obstacle avoided. Resuming pattern...");
    } else {
      autoModeMovement();
    }
  }
  // Manual Mode Behavior
  else {
    // Manual mode: If obstacle is detected, stop
    if (obstacleDetected(20)) {
      Serial.println("[Manual Mode] Obstacle detected! Stopping...");
      stopMotor();
    }
  }
}
