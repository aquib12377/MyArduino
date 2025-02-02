#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Create PCA9685 instance
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo configuration
#define SERVOMIN 150  // Minimum pulse length count
#define SERVOMAX 600  // Maximum pulse length count
#define SERVO_FREQ 50 // Servo frequency

// Servo channels
#define BASE_SERVO 0
#define MIDDLE_SERVO_1 1
#define MIDDLE_SERVO_2 2
#define GRIPPER_SERVO 3

// Current angles
int base_angle = 90;
int middle1_angle = 90;
int middle2_angle = 90;
int gripper_angle = 90;

// Function to convert angle to pulse width
int angleToPulse(int angle) {
  return map(angle, 0, 180, SERVOMIN, SERVOMAX);
}

// Function to move a servo slowly
void moveServoSlowly(uint8_t channel, int &currentAngle, int targetAngle) {
  int step = (targetAngle > currentAngle) ? 1 : -1;
  for (int angle = currentAngle; angle != targetAngle; angle += step) {
    pwm.setPWM(channel, 0, angleToPulse(angle));
    delay(15); // Adjust speed
  }
  pwm.setPWM(channel, 0, angleToPulse(targetAngle));
  currentAngle = targetAngle;
}

void setup() {
  Serial.begin(9600);
  Serial.println("Robotic Arm Controller with PCA9685");

  pwm.begin();
  pwm.setOscillatorFrequency(27000000); // Optional calibration
  pwm.setPWMFreq(SERVO_FREQ);

  delay(10);

  // Set initial positions
  pwm.setPWM(BASE_SERVO, 0, angleToPulse(base_angle));
  pwm.setPWM(MIDDLE_SERVO_1, 0, angleToPulse(middle1_angle));
  pwm.setPWM(MIDDLE_SERVO_2, 0, angleToPulse(middle2_angle));
  pwm.setPWM(GRIPPER_SERVO, 0, angleToPulse(gripper_angle));
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() == 1) {
      // Single-character commands
      char command = input.charAt(0);

      switch (command) {
        case 'B': // Rotate base servo right
          moveServoSlowly(BASE_SERVO, base_angle, base_angle + 10);
          Serial.println("Base Servo Rotated");
          break;

        case 'U': // Middle servos up
          moveServoSlowly(MIDDLE_SERVO_1, middle1_angle, middle1_angle - 10);
          moveServoSlowly(MIDDLE_SERVO_2, `middle2_angle, middle2_angle - 10);
          Serial.println("Middle Servos Up");
          break;

        case 'D': // Middle servos down
          moveServoSlowly(MIDDLE_SERVO_1, middle1_angle, middle1_angle + 10);
          moveServoSlowly(MIDDLE_SERVO_2, middle2_angle, middle2_angle + 10);
          Serial.println("Middle Servos Down");
          break;

        case 'O': // Gripper open
          moveServoSlowly(GRIPPER_SERVO, gripper_angle, 0);
          Serial.println("Gripper Opened");
          break;

        case 'C': // Gripper close
          moveServoSlowly(GRIPPER_SERVO, gripper_angle, 180);
          Serial.println("Gripper Closed");
          break;

        case 'R': // Reset to default
          moveServoSlowly(BASE_SERVO, base_angle, 90);
          moveServoSlowly(MIDDLE_SERVO_1, middle1_angle, 90);
          moveServoSlowly(MIDDLE_SERVO_2, middle2_angle, 90);
          moveServoSlowly(GRIPPER_SERVO, gripper_angle, 90);
          Serial.println("Arm Reset");
          break;

        default:
          Serial.println("Invalid Command");
          break;
      }
    } else if (input.startsWith("T")) {
      // Test command: "T <servo_number> <angle>"
      int servoNumber = input.substring(2, input.indexOf(' ', 2)).toInt();
      int angle = input.substring(input.indexOf(' ', 2) + 1).toInt();

      switch (servoNumber) {
        case 0:
          moveServoSlowly(BASE_SERVO, base_angle, angle);
          Serial.println("Base Servo Set to " + String(angle) + " degrees");
          break;

        case 1:
          moveServoSlowly(MIDDLE_SERVO_1, middle1_angle, angle);
          Serial.println("Middle Servo 1 Set to " + String(angle) + " degrees");
          break;

        case 2:
          moveServoSlowly(MIDDLE_SERVO_2, middle2_angle, angle);
          Serial.println("Middle Servo 2 Set to " + String(angle) + " degrees");
          break;

        case 3:
          moveServoSlowly(GRIPPER_SERVO, gripper_angle, angle);
          Serial.println("Gripper Servo Set to " + String(angle) + " degrees");
          break;

        default:
          Serial.println("Invalid Servo Number");
          break;
      }
    } else {
      Serial.println("Invalid Input");
    }
  }
}
