#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 columns, 2 rows

// Motor Pins
#define MOTOR_A1 4 // Motor A forward
#define MOTOR_A2 3 // Motor A backward
#define MOTOR_B1 7 // Motor B forward
#define MOTOR_B2 8 // Motor B backward

// Ultrasonic Sensor Pins
#define TRIG_PIN A0
#define ECHO_PIN A1

// Distance Threshold (in cm)
#define OBSTACLE_DISTANCE 20

// Current movement state
bool isMovingForward = false;
String lastCommand = "Stop"; // Default command
	const int relayPin = 12;

void setup() {
  // Motor Pins Setup
  pinMode(MOTOR_A1, OUTPUT);
  pinMode(MOTOR_A2, OUTPUT);
  pinMode(MOTOR_B1, OUTPUT);
  pinMode(MOTOR_B2, OUTPUT);
  stopMotors(); // Initialize motors in a stopped state

  // Ultrasonic Sensor Pins Setup
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
pinMode(relayPin, OUTPUT);
	  digitalWrite(relayPin, HIGH);
  // Serial Setup for HC-05 and Debugging
  Serial.begin(9600);
  Serial.println("Bot Initialized. Awaiting commands...");

  // LCD Setup
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Bot Initialized");
  delay(2000);
  lcd.clear();
}

void loop() {
  static unsigned long lastCheckTime = 0;
  static unsigned long lastCommandTime = 0;

  // Ultrasonic distance check every 50ms
  if (millis() - lastCheckTime >= 50) {
    lastCheckTime = millis();
    float distance = getUltrasonicDistance();

    // Display distance on LCD
    lcd.setCursor(0, 1);
    lcd.print("Dist: ");
    lcd.print(distance);
    lcd.print(" cm  "); // Padding for clear display

    if (isMovingForward && distance > 0 && distance <= OBSTACLE_DISTANCE) {
      Serial.println("Obstacle detected! Stopping bot...");
      lcd.setCursor(0, 0);
      lcd.print("Obstacle Stop   ");
      stopMotors();
      isMovingForward = false; // Stop forward movement
    }
  }

  // Read Bluetooth commands if available
  if (Serial.available()) {
    lastCommandTime = millis();
    char command = Serial.read();
    processCommand(command);
  }


}

// Function to measure distance using ultrasonic sensor
float getUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2); // Non-blocking small delay
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); // Non-blocking small delay
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout after 30ms to avoid blocking
  if (duration == 0) {
    return 1000; // Assume no obstacle if no echo is received
  }
  float distance = duration * 0.034 / 2; // Convert duration to distance in cm
  return distance == 0 ? 100 : distance;
}

// Function to process Bluetooth commands
void processCommand(char command) {
  switch (command) {
    case 'F': // Move Forward
      Serial.println("Command: Forward");
      isMovingForward = true; // Set the forward movement flag
      updateLCD("Forward");
      moveForward();
      break;
    case 'B': // Move Backward
      Serial.println("Command: Backward");
      isMovingForward = false; // Not moving forward
      updateLCD("Backward");
      moveBackward();
      break;
    case 'L': // Turn Left
      Serial.println("Command: Left");
      isMovingForward = false; // Not moving forward
      updateLCD("Left");
      turnLeft();
      break;
    case 'R': // Turn Right
      Serial.println("Command: Right");
      isMovingForward = false; // Not moving forward
      updateLCD("Right");
      turnRight();
      break;
    case 'S': // Stop
      //Serial.println("Command: Stop");
      isMovingForward = false; // Not moving forward
      updateLCD("Stop");
      stopMotors();
      break;
    case 'U':  // Stop all motors
		  digitalWrite(relayPin, LOW);
		  break;
		  case 'u':  // Stop all motors
		  digitalWrite(relayPin, HIGH);
		  break;
    default:
      //Serial.println("Unknown Command");
      updateLCD("Unknown");
      break;
  }
}

// Function to update the LCD with the current command
void updateLCD(String command) {
  lastCommand = command; // Save the last command
  lcd.setCursor(0, 0);
  lcd.print("Cmd: ");
  lcd.print(command);
  lcd.print("          "); // Clear remaining spaces
}

// Motor Control Functions
void moveForward() {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
}

void moveBackward() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
}

void turnLeft() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
}

void turnRight() {
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
}

void stopMotors() {
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, LOW);
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, LOW);
}
