#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// ----- Pin Definitions -----

// Motor Pins
const int motorA_IN1 = 7;
const int motorA_IN2 = 8;
const int motorB_IN1 = 9;
const int motorB_IN2 = 10;

// Bluetooth Pins
const int BT_RX = 2; // Arduino RX
const int BT_TX = 3; // Arduino TX

// LED Pins
const int vacuumLED = A3;
const int mopLED = A2;

// Relay Pins
const int relayVacuum = 4;
const int relayMop = 5;
const int relayPump = 6;

// Ultrasonic Sensor Pins
const int trigPin = A1;
const int echoPin = A0;

// Voltage Sensor Pin
const int voltagePin = A7;

// LCD Setup (Address 0x27 or 0x3F depending on your module)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Bluetooth Setup
SoftwareSerial bluetooth(BT_RX, BT_TX); // RX, TX

// Voltage Sensor Constants
const float R1 = 30000.0;      // Resistor R1 in ohms
const float R2 = 7500.0;       // Resistor R2 in ohms
const float ref_voltage = 5.0; // Reference voltage for Arduino

// Variables
long duration;
int distance;
float batteryVoltage;

// Additional Variables for Voltage Calculation
float adc_voltage = 0.0;
float in_voltage = 0.0;
int adc_value = 0;

void setup() {
  // Initialize Serial for debugging
  Serial.begin(9600);
  
  // Initialize Bluetooth Serial
  bluetooth.begin(9600);
  
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Vacuum Mop Bot");
  lcd.setCursor(0,1);
  lcd.print("BT Controlled");
  
  // Initialize Motor Pins
  pinMode(motorA_IN1, OUTPUT);
  pinMode(motorA_IN2, OUTPUT);
  pinMode(motorB_IN1, OUTPUT);
  pinMode(motorB_IN2, OUTPUT);
  
  // Initialize LED Pins
  pinMode(vacuumLED, OUTPUT);
  pinMode(mopLED, OUTPUT);
  
  // Initialize Relay Pins
  pinMode(relayVacuum, OUTPUT);
  pinMode(relayMop, OUTPUT);
  pinMode(relayPump, OUTPUT);
  
  // Initialize Ultrasonic Sensor Pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Initialize Voltage Sensor Pin
  pinMode(voltagePin, INPUT);
  
  // Turn off all motors and relays initially
  stopMotors();
  digitalWrite(relayVacuum, HIGH);
  digitalWrite(relayMop, HIGH);
  digitalWrite(relayPump, HIGH);
  
  // Turn off LEDs
  digitalWrite(vacuumLED, LOW);
  digitalWrite(mopLED, LOW);
  
  delay(2000); // Wait for 2 seconds before starting
  lcd.clear();
}

void loop() {
  // Check for Bluetooth commands
  if (bluetooth.available()) {
    char command = bluetooth.read();
    handleCommand(command);
  }
  
  // Obstacle Detection
  distance = getDistance();
  if (distance > 0 && distance < 40) { // If obstacle is closer than 20 cm
    stopMotors();
    delay(10);
    moveBackward();
    delay(1000);
    turnRight();
    delay(5000);
    moveForward();
    lcd.setCursor(0,1);
    lcd.print("Obstacle Detected");
    delay(1000); // Wait for a second
    // Implement additional obstacle avoidance if needed
  } else {
    lcd.setCursor(0,1);
    if (distance > 0) {
      lcd.print("Distance: " + String(distance) + " cm ");
    } else {
      lcd.print("Distance: -- cm ");
    }
  }
  
  // Battery Voltage Monitoring
  batteryVoltage = readBatteryVoltage();
  lcd.setCursor(0,0);
  lcd.print("Voltage: " + String(batteryVoltage, 2) + " V ");
  
  // Optional: Print to Serial Monitor for debugging
  // Serial.print("Battery Voltage: ");
  // Serial.print(batteryVoltage, 2);
  // Serial.println(" V");
  
  delay(10); // Update every 200 ms
}

// Function to handle Bluetooth commands
void handleCommand(char cmd) {
  switch(cmd) {
    case 'F': // Move Forward
      moveForward();
      lcd.setCursor(0,1);
      lcd.print("Moving Forward    ");
      break;
    case 'B': // Move Backward
      moveBackward();
      lcd.setCursor(0,1);
      lcd.print("Moving Backward   ");
      break;
    case 'L': // Turn Left
      turnLeft();
      lcd.setCursor(0,1);
      lcd.print("Turning Left      ");
      break;
    case 'R': // Turn Right
      turnRight();
      lcd.setCursor(0,1);
      lcd.print("Turning Right     ");
      break;
    case 'S': // Stop
      stopMotors();
      lcd.setCursor(0,1);
      lcd.print("Motors Stopped    ");
      break;
    case 'V': // Toggle Vacuum
      toggleVacuum();
      break;
    case 'P': // Toggle Mop
      toggleMop();
      break;
    case 'M': // Toggle Pump
      togglePump();
      break;
    default:
      // Unknown command
      lcd.setCursor(0,1);
      lcd.print("Unknown Command   ");
      break;
  }
  delay(1000);
}

// Motor Control Functions
void moveForward() {
  digitalWrite(motorA_IN1, HIGH);
  digitalWrite(motorA_IN2, LOW);
  digitalWrite(motorB_IN1, HIGH);
  digitalWrite(motorB_IN2, LOW);
}

void moveBackward() {
  digitalWrite(motorA_IN1, LOW);
  digitalWrite(motorA_IN2, HIGH);
  digitalWrite(motorB_IN1, LOW);
  digitalWrite(motorB_IN2, HIGH);
}

void turnLeft() {
  digitalWrite(motorA_IN1, LOW);
  digitalWrite(motorA_IN2, HIGH);
  digitalWrite(motorB_IN1, HIGH);
  digitalWrite(motorB_IN2, LOW);
}

void turnRight() {
  digitalWrite(motorA_IN1, HIGH);
  digitalWrite(motorA_IN2, LOW);
  digitalWrite(motorB_IN1, LOW);
  digitalWrite(motorB_IN2, HIGH);
}

void stopMotors() {
  digitalWrite(motorA_IN1, LOW);
  digitalWrite(motorA_IN2, LOW);
  digitalWrite(motorB_IN1, LOW);
  digitalWrite(motorB_IN2, LOW);
}

// Relay Control Functions
void toggleVacuum() {
  static bool vacuumOn = false;
  vacuumOn = !vacuumOn;
  digitalWrite(relayVacuum, !vacuumOn ? HIGH : LOW);
  digitalWrite(vacuumLED, vacuumOn ? HIGH : LOW);
  lcd.setCursor(0,1);
  lcd.print(vacuumOn ? "Vacuum: ON       " : "Vacuum: OFF      ");
}

void toggleMop() {
  static bool mopOn = false;
  mopOn = !mopOn;
  digitalWrite(relayMop, !mopOn ? HIGH : LOW);
  digitalWrite(mopLED, mopOn ? HIGH : LOW);
  lcd.setCursor(0,1);
  lcd.print(mopOn ? "Mop: ON          " : "Mop: OFF         ");
}

void togglePump() {
  static bool pumpOn = false;
  pumpOn = !pumpOn;
  digitalWrite(relayPump, !pumpOn ? HIGH : LOW);
  lcd.setCursor(0,1);
  lcd.print(pumpOn ? "Pump: ON         " : "Pump: OFF        ");
}

// Ultrasonic Sensor Function
int getDistance() {
  // Clear the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Set the trigPin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echoPin
  duration = pulseIn(echoPin, HIGH, 30000); // Timeout after 30ms
  if (duration == 0) {
    return 100; // No echo received
  }
  
  // Calculate distance in cm
  int distanceCm = duration * 0.034 / 2;
  return distanceCm;
}

// Battery Voltage Reading Function
float readBatteryVoltage() {
  // Read the Analog Input from voltage sensor
  adc_value = analogRead(voltagePin);
  
  // Determine voltage at ADC input
  adc_voltage  = (adc_value * ref_voltage) / 1024.0;
  
  // Calculate voltage at divider input
  in_voltage = adc_voltage * (R1 + R2) / R2;
  
  return in_voltage;
}
