#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Create an instance of the PCA9685 driver (default I2C address = 0x40).
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Servo pulse range values (tweak for your specific servos if needed)
#define SERVOMIN  150 // 'minimum' pulse length count (out of 4096)
#define SERVOMAX  600 // 'maximum' pulse length count (out of 4096)
#define SERVO_FREQ 50 // servos typically run at 50 Hz

// Arrays to identify which channels are inner arms vs outer limbs.
// If you physically have them in a different arrangement, feel free to reorder.
int innerArms[4] = {0, 2, 4, 6};  // Channels for inner arms
int outerArms[4] = {1, 3, 5, 7};  // Channels for outer arms

//---------------------------------------------------------
// Convert an angle (0-180) to the corresponding PCA9685 pulse count.
// This uses SERVOMIN as 0 degrees and SERVOMAX as 180 degrees.
int angleToPulse(int angle) {
  // Use Arduino's built-in 'map()' function
  int pulse = map(angle, 0, 180, SERVOMIN, SERVOMAX);
  return pulse;
}

//---------------------------------------------------------
void setup() {
  Serial.begin(9600);
  Serial.println("Spider Robot - Basic 8 Servo Control");

  // Initialize PCA9685
  pwm.begin();
  // Use 27,000,000 if your PCA9685 is closer to 27MHz, or adjust to calibrate.
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ); // 50 Hz
  delay(10);
  
  // Optional: Move all servos to a "neutral" (e.g., 90 degrees) to start
  for (int i = 0; i < 8; i++) {
    pwm.setPWM(i, 0, angleToPulse(90));
  }
  delay(500);
}

//---------------------------------------------------------
void loop() {
  // This example just demonstrates a simple pattern of angles.
  // You can replace this logic with your own walking/stepping algorithm.

  // 1) Move all inner arms up (angle = 45), outer arms down (angle = 135)
  moveServos(innerArms, 4, 45);
  moveServos(outerArms, 4, 135);
  delay(1000);

  // 2) Move all inner arms down (135), outer arms up (45)
  moveServos(innerArms, 4, 135);
  moveServos(outerArms, 4, 45);
  delay(1000);

  // 3) Move all servos to “neutral” (90)
  moveServos(innerArms, 4, 90);
  moveServos(outerArms, 4, 90);
  delay(1000);

  // Repeat forever
}

//---------------------------------------------------------
// Helper function to move an array of servo channels to a given angle.
void moveServos(int servoChannels[], int count, int angle) {
  int pulse = angleToPulse(angle);
  for (int i = 0; i < count; i++) {
    int channel = servoChannels[i];
    pwm.setPWM(channel, 0, pulse);
  }
}
