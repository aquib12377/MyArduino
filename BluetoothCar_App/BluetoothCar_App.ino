  // Pins for motor control
  const int motor1Pin1 = 4;
  const int motor1Pin2 = 5;
  const int motor2Pin1 = 6;
  const int motor2Pin2 = 7;
  const int enablePin1 = 9;
  const int enablePin2 = 10;

  #define debug true

  // Pins for HC-05
  const int bluetoothRx = 2;
  const int bluetoothTx = 3;

  // Speed variables
  int speedMotor1 = 0;
  int speedMotor2 = 0;
  const int rampDelay = 20; // Delay for speed ramping

  void LOG(String message)
  {
    if(debug)
      {
        Serial.println(message);
      }
  }

  void setup() {
    // Motor control pins as output
    pinMode(motor1Pin1, OUTPUT);
    pinMode(motor1Pin2, OUTPUT);
    pinMode(motor2Pin1, OUTPUT);
    pinMode(motor2Pin2, OUTPUT);
    pinMode(enablePin1, OUTPUT);
    pinMode(enablePin2, OUTPUT);
analogWrite(enablePin1, 150);
    analogWrite(enablePin2, 150);
    // HC-05 communication
    Serial.begin(9600); // Communication with HC-05
    LOG("HC-05 ready");
  }

  void loop() {
    if (Serial.available()) {
      char command = Serial.read();
      LOG("Received Command: "+String(command));
      switch (command) {
        case 'F': // Move forward
          //rampMotor(enablePin1, enablePin2, 255);
          setMotorDirection(motor1Pin1, motor1Pin2, true);
          setMotorDirection(motor2Pin1, motor2Pin2, true);
          break;
        case 'B': // Move backward
          //rampMotor(enablePin1, enablePin2, 255);
          setMotorDirection(motor1Pin1, motor1Pin2, false);
          setMotorDirection(motor2Pin1, motor2Pin2, false);
          break;
        case 'L': // Turn left
          //rampMotor(enablePin1, enablePin2, 200);
          setMotorDirection(motor1Pin1, motor1Pin2, false);
          setMotorDirection(motor2Pin1, motor2Pin2, true);
          break;
        case 'R': // Turn right
          //rampMotor(enablePin1, enablePin2, 200);
          setMotorDirection(motor1Pin1, motor1Pin2, true);
          setMotorDirection(motor2Pin1, motor2Pin2, false);
          break;
        case 'S': // Stop
          stopMotor();
          break;
      }
    }
  }

  void rampMotor(int enablePin1, int enablePin2, int targetSpeed) {
    for (int speed = 0; speed <= targetSpeed; speed += 5) {
      analogWrite(enablePin1, speed);
      analogWrite(enablePin2, speed);
      delay(rampDelay);
    }
  }

  void setMotorDirection(int pin1, int pin2, bool forward) {
    if (forward) {
      digitalWrite(pin1, HIGH);
      digitalWrite(pin2, LOW);
    } else {
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, HIGH);
    }
  }

  void stopMotor() {
    analogWrite(enablePin1, 0);
    analogWrite(enablePin2, 0);
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    digitalWrite(motor2Pin1, LOW);
    digitalWrite(motor2Pin2, LOW);
  }
