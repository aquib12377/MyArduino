/******************************************************************************
 * ESP32 Motor & Steering Control with BTS7960, TB6600 and Bluetooth
 * 
 * This sketch controls 4 motors (via BTS7960 modules) and 4 active low relays.
 * By default each motor is connected to a 12V, 7A supply. When a relay is triggered,
 * an additional 12V, 7A battery is connected to that motor.
 * A TB6600 driver is used to control a stepper motor for steering.
 *
 * Communication is done via the ESP32 built-in Bluetooth using the BluetoothSerial
 * library.
 *
 * Command Formats:
 *   - Individual Motor Control:  "M<id> <F/B> <speed>"
 *         Example: "M1 F 200" --> Motor 1 forward at PWM speed 200 (0-255)
 *   - Relay Control:               "R<id> <ON/OFF>"
 *         Example: "R2 ON"   --> Activate (set LOW) relay for Motor 2
 *   - Individual Stepper Control:  "S <L/R> <steps>"
 *         Example: "S R 100" --> Move the stepper motor 100 steps to the right
 *   - Combined Commands:
 *         "FORWARD [speed]"  --> Drives ALL motors forward (default speed = 200)
 *         "BACKWARD [speed]" --> Drives ALL motors backward (default speed = 200)
 *         "LEFT [steps]"     --> Moves stepper motor left (default steps = 150)
 *         "RIGHT [steps]"    --> Moves stepper motor right (default steps = 150)
 *   - Stop All Motors:             "STOP"
 *
 ******************************************************************************/

#include <BluetoothSerial.h>

// Create a BluetoothSerial object
BluetoothSerial SerialBT;

// ==========================================
// Pin Definitions
// ------------------------------------------
// BTS7960 Motor Driver pins for 4 motors (adjust as needed)
// Motor 1
const int M1_RPWM = 16;      // RPWM for Motor 1
const int M1_LPWM = 17;      // LPWM for Motor 1
const int M1_relay = 5;      // Relay for Motor 1

// Motor 2
const int M2_RPWM = 18;      // RPWM for Motor 2
const int M2_LPWM = 19;      // LPWM for Motor 2
const int M2_relay = 25;     // Relay for Motor 2

// Motor 3
const int M3_RPWM = 21;      // RPWM for Motor 3
const int M3_LPWM = 22;      // LPWM for Motor 3
const int M3_relay = 26;     // Relay for Motor 3

// Motor 4
const int M4_RPWM = 23;      // RPWM for Motor 4
const int M4_LPWM = 32;      // LPWM for Motor 4
const int M4_relay = 33;     // Relay for Motor 4

// TB6600 Stepper Motor Control Pins
const int STEPPER_STEP_PIN = 27;  // Step pulse pin for stepper
const int STEPPER_DIzR_PIN  = 14;   // Direction pin for stepper

// Arrays grouping motor pins for convenience
const int motorRPWM[4] = { M1_RPWM, M2_RPWM, M3_RPWM, M4_RPWM };
const int motorLPWM[4] = { M1_LPWM, M2_LPWM, M3_LPWM, M4_LPWM };
const int relayPins[4] = { M1_relay, M2_relay, M3_relay, M4_relay };

// ==========================================
// Function Prototypes
// ------------------------------------------
void setMotorSpeed(int motorIndex, char direction, int speed);
void controlRelay(int motorIndex, bool activate);
void moveStepper(char direction, int steps);
void processCommand(String command);
void allMotorsForward(int speed);
void allMotorsBackward(int speed);

// ==========================================
// Setup Function
// ------------------------------------------
void setup() {
  // Initialize serial for debugging.
  Serial.begin(115200);
  Serial.println("ESP32 Motor & Steering Controller Starting...");

  // Initialize Bluetooth Serial (device name "ESP32_BT" can be changed).
  if (!SerialBT.begin("ESP32_BT")) {  
    Serial.println("An error occurred initializing Bluetooth");
  } else {
    Serial.println("Bluetooth initialized. Waiting for connections...");
  }

  // Set motor control pins as OUTPUT and initialize PWM duty to zero.
  for (int i = 0; i < 4; i++) {
    pinMode(motorRPWM[i], OUTPUT);
    pinMode(motorLPWM[i], OUTPUT);
    analogWrite(motorRPWM[i], 0);
    analogWrite(motorLPWM[i], 0);
  }

  // Configure relay pins as outputs.
  // Since relays are active LOW, writing HIGH keeps them deactivated.
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH); // Default state: relay OFF
  }

  // Set up TB6600 stepper pins.
  pinMode(STEPPER_STEP_PIN, OUTPUT);
  pinMode(STEPPER_DIR_PIN, OUTPUT);
  digitalWrite(STEPPER_STEP_PIN, LOW);
  digitalWrite(STEPPER_DIR_PIN, LOW);
}

// ==========================================
// Main Loop
// ------------------------------------------
void loop() {
  // Check if Bluetooth data is available.
  if (SerialBT.available()) {
    // Read incoming command until a newline character.
    String command = SerialBT.readStringUntil('\n');
    command.trim();
    if (command.length() > 0) {
      Serial.print("Received Command: ");
      Serial.println(command);
      processCommand(command);
    }
  }
  // Add any additional periodic tasks here if needed.
}

// ==========================================
// Process a received command string.
// ------------------------------------------
void processCommand(String command) {
  command.trim();

  // ----- Individual Commands -----
  // Motor Control: "M<id> <F/B> <speed>" e.g., "M1 F 200"
  if (command.charAt(0) == 'M' || command.charAt(0) == 'm') {
    int motorIndex = command.substring(1,2).toInt() - 1; // convert id to 0-index
    if (motorIndex < 0 || motorIndex >= 4) {
      Serial.println("Invalid motor id");
      return;
    }
    char direction = command.substring(3,4).charAt(0);
    int speed = command.substring(5).toInt();
    Serial.print("Setting Motor ");
    Serial.print(motorIndex + 1);
    Serial.print(" Direction: ");
    Serial.print(direction);
    Serial.print(" Speed: ");
    Serial.println(speed);
    setMotorSpeed(motorIndex, direction, speed);
    return;
  }

  // Relay Control: "R<id> <ON/OFF>" e.g., "R2 ON"
  if (command.charAt(0) == 'R' || command.charAt(0) == 'r') {
    int motorIndex = command.substring(1,2).toInt() - 1;
    if (motorIndex < 0 || motorIndex >= 4) {
      Serial.println("Invalid relay/motor id");
      return;
    }
    String relayState = command.substring(3);
    relayState.trim();
    Serial.print("Setting Relay for Motor ");
    Serial.print(motorIndex + 1);
    Serial.print(" to ");
    Serial.println(relayState);
    if (relayState.equalsIgnoreCase("ON")) {
      controlRelay(motorIndex, true);  // Activate relay (active LOW)
    } else if (relayState.equalsIgnoreCase("OFF")) {
      controlRelay(motorIndex, false);
    } else {
      Serial.println("Invalid relay command. Use ON or OFF.");
    }
    return;
  }

  // Stepper Control: "S <L/R> <steps>" e.g., "S R 100"
  if ((command.charAt(0) == 'S' || command.charAt(0) == 's') && command != "STOP") {
    char steerDir = command.substring(2,3).charAt(0);
    int steps = command.substring(4).toInt();
    Serial.print("Moving Stepper ");
    Serial.print((steerDir == 'L' || steerDir == 'l') ? "Left" : "Right");
    Serial.print(" by ");
    Serial.print(steps);
    Serial.println(" steps");
    moveStepper(steerDir, steps);
    return;
  }
  
  // Stop command: "STOP" stops all motors.
  if (command.equalsIgnoreCase("STOP")) {
    Serial.println("Stopping all motors");
    for (int i = 0; i < 4; i++) {
      analogWrite(motorRPWM[i], 0);
      analogWrite(motorLPWM[i], 0);
    }
    return;
  }

  // ----- Combined High-Level Commands -----
  // FORWARD [speed]: Drives all motors forward.
  if (command.startsWith("FORWARD") || command.startsWith("forward")) {
    int speed = 200;  // default speed
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1) {
      speed = command.substring(spaceIndex + 1).toInt();
      if (speed <= 0) speed = 200;
    }
    Serial.print("Executing FORWARD command with speed: ");
    Serial.println(speed);
    allMotorsForward(speed);
    return;
  }

  // BACKWARD [speed]: Drives all motors backward.
  if (command.startsWith("BACKWARD") || command.startsWith("backward")) {
    int speed = 200;  // default speed
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1) {
      speed = command.substring(spaceIndex + 1).toInt();
      if (speed <= 0) speed = 200;
    }
    Serial.print("Executing BACKWARD command with speed: ");
    Serial.println(speed);
    allMotorsBackward(speed);
    return;
  }

  // LEFT [steps]: Steers left by moving the stepper motor.
  if (command.startsWith("LEFT") || command.startsWith("left")) {
    int steps = 150;  // default step count
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1) {
      steps = command.substring(spaceIndex + 1).toInt() * 10;
      if (steps <= 0) steps = 150;
    }
    Serial.print("Executing LEFT command with steps: ");
    Serial.println(steps);
    moveStepper('L', steps);
    return;
  }

  // RIGHT [steps]: Steers right by moving the stepper motor.
  if (command.startsWith("RIGHT") || command.startsWith("right")) {
    int steps = 150;  // default step count
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1) {
      steps = command.substring(spaceIndex + 1).toInt() * 10;
      if (steps <= 0) steps = 150;
    }
    Serial.print("Executing RIGHT command with steps: ");
    Serial.println(steps);
    moveStepper('R', steps);
    return;
  }

  Serial.println("Command not recognized");
}

// ==========================================
// Function: setMotorSpeed
// ------------------------------------------
// Sets the PWM speed for a given motor.
//   - direction: 'F' for forward, 'B' for backward (any other value stops the motor)
//   - speed: PWM value (0-255)
// ------------------------------------------
void setMotorSpeed(int motorIndex, char direction, int speed) {
  speed = constrain(speed, 0, 255);
  if (direction == 'F' || direction == 'f') {
    // Forward: set RPWM to speed, LPWM to 0.
    analogWrite(motorRPWM[motorIndex], speed);
    analogWrite(motorLPWM[motorIndex], 0);
  } else if (direction == 'B' || direction == 'b') {
    // Backward: set LPWM to speed, RPWM to 0.
    analogWrite(motorRPWM[motorIndex], 0);
    analogWrite(motorLPWM[motorIndex], speed);
  } else {
    // Invalid direction: stop the motor.
    analogWrite(motorRPWM[motorIndex], 0);
    analogWrite(motorLPWM[motorIndex], 0);
  }
}

// ==========================================
// Function: controlRelay
// ------------------------------------------
// Controls the relay for a given motor.
//   - If 'activate' is true, sets the pin LOW (triggering relay).
//   - Otherwise, sets the pin HIGH.
// ------------------------------------------
void controlRelay(int motorIndex, bool activate) {
  digitalWrite(relayPins[motorIndex], activate ? LOW : HIGH);
}

// ==========================================
// Function: moveStepper
// ------------------------------------------
// Moves the stepper motor a given number of steps in the specified direction.
//   - 'L' or 'l' for left, 'R' or 'r' for right.
//   - Adjust delay values as necessary for your TB6600/stepper motor.
// ------------------------------------------
void moveStepper(char direction, int steps) {
  // Set the stepper direction based on the command.
  if (direction == 'L' || direction == 'l') {
    digitalWrite(STEPPER_DIR_PIN, LOW);  // Define one polarity for left
  } else if (direction == 'R' || direction == 'r') {
    digitalWrite(STEPPER_DIR_PIN, HIGH); // The opposite polarity for right
  } else {
    Serial.println("Invalid stepper direction");
    return;
  }
  
  // Pulse the step pin for the specified number of steps.
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEPPER_STEP_PIN, HIGH);
    delayMicroseconds(100);  // High pulse duration (adjust if necessary)
    digitalWrite(STEPPER_STEP_PIN, LOW);
    delayMicroseconds(100);  // Low pulse duration (adjust if necessary)
  }
}

// ==========================================
// Helper: Drive all motors forward.
// ------------------------------------------
void allMotorsForward(int speed) {
  for (int i = 0; i < 4; i++) {
    setMotorSpeed(i, 'F', speed);
  }
}

// ==========================================
// Helper: Drive all motors backward.
// ------------------------------------------
void allMotorsBackward(int speed) {
  for (int i = 0; i < 4; i++) {
    setMotorSpeed(i, 'B', speed);
  }
}