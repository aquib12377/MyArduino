#define LIMIT_SWITCH_1 2  // Pin for the first limit switch
#define LIMIT_SWITCH_2 3  // Pin for the second limit switch
#define MOTOR_IN1 4       // Pin for motor driver IN1
#define MOTOR_IN2 5       // Pin for motor driver IN2
#define MOTOR_ENA 9       // Pin for motor driver ENA (PWM control)

// Motor control states
enum MotorState { STOP, UP, DOWN };
MotorState motorState = STOP;

void setup() {
  pinMode(LIMIT_SWITCH_1, INPUT_PULLUP);  // Set limit switch pins as input with internal pull-up resistors
  pinMode(LIMIT_SWITCH_2, INPUT_PULLUP);
  pinMode(MOTOR_IN1, OUTPUT);             // Set motor driver pins as output
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_ENA, OUTPUT);

  Serial.begin(9600);  // Initialize serial communication for debugging
}

void loop() {
  bool switch1Pressed = !digitalRead(LIMIT_SWITCH_1);  // Read the state of the limit switches
  bool switch2Pressed = !digitalRead(LIMIT_SWITCH_2);

  // If the motor is not moving, start moving in one direction
  if (motorState == STOP) {
    motorState = UP;
  }
  // If the first limit switch is pressed, change direction
  if (switch1Pressed) {
    motorState = DOWN;
  }
  // If the second limit switch is pressed, change direction
  else if (switch2Pressed) {
    motorState = UP;
  }

  controlMotor(motorState);  // Control the motor based on the current state

  Serial.print("Switch 1: ");
  Serial.print(switch1Pressed);
  Serial.print(" Switch 2: ");
  Serial.print(switch2Pressed);
  Serial.print(" Motor State: ");
  Serial.println(motorState == UP ? "UP" : motorState == DOWN ? "DOWN" : "STOP");

  delay(100);  // Small delay for debounce
}

void controlMotor(MotorState state) {
  switch (state) {
    case UP:
      digitalWrite(MOTOR_IN1, HIGH);
      digitalWrite(MOTOR_IN2, LOW);
      analogWrite(MOTOR_ENA, 255);  // Full speed upward
      break;
    case DOWN:
      digitalWrite(MOTOR_IN1, LOW);
      digitalWrite(MOTOR_IN2, HIGH);
      analogWrite(MOTOR_ENA, 255);  // Full speed downward
      break;
    case STOP:
    default:
      digitalWrite(MOTOR_IN1, LOW);
      digitalWrite(MOTOR_IN2, LOW);
      analogWrite(MOTOR_ENA, 0);    // Stop the motor
      break;
  }
}
