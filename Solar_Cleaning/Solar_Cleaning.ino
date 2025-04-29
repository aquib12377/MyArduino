#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// Motor Pins (via L298)
#define MOTOR_IN1 25
#define MOTOR_IN2 33

// Pump Pins (via L298)
#define PUMP_IN3 27
#define PUMP_IN4 26

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_MotorPump");  // Bluetooth name
  Serial.println("Bluetooth started. Pair with 'ESP32_MotorPump'");

  // Set all motor/pump pins as output
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(PUMP_IN3, OUTPUT);
  pinMode(PUMP_IN4, OUTPUT);

  stopMotor();
  stopPump();
}

void loop() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();
    Serial.println("Received: " + command);

    if (command == "1") {
      startMotor();
            startPump();
    } else if (command == "0") {
      stopMotor();
            stopPump();

    }
    else if (command == "2") {
      startMotorR();
            startPump();

    } else if (command == "P_ON") {
    } else if (command == "P_OFF") {
    } else {
      SerialBT.println("Unknown command!");
    }
  }
}

// Motor Control
void startMotor() {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  SerialBT.println("Motor ON");
}

void startMotorR() {
  digitalWrite(MOTOR_IN2, HIGH);
  digitalWrite(MOTOR_IN1, LOW);
  SerialBT.println("Motor ON");
}

void stopMotor() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  SerialBT.println("Motor OFF");
}

// Pump Control
void startPump() {
  digitalWrite(PUMP_IN3, HIGH);
  digitalWrite(PUMP_IN4, LOW);
  SerialBT.println("Pump ON");
}

void stopPump() {
  digitalWrite(PUMP_IN3, LOW);
  digitalWrite(PUMP_IN4, LOW);
  SerialBT.println("Pump OFF");
}
