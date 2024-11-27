/*
  Motor Speed Control using FSR and BTS7960

  This code reads the analog value from an FSR and controls the speed
  of a DC motor using the BTS7960 motor driver.

  Connections:
  - FSR connected in a voltage divider to Analog Pin A0
  - BTS7960 RPWM connected to Arduino PWM Pin 5
  - BTS7960 LPWM connected to Arduino PWM Pin 6
  - BTS7960 R_EN and L_EN connected to 5V
  - BTS7960 R_IS and L_IS connected to GND
  - BTS7960 GND connected to Arduino GND
  - BTS7960 VCC connected to Motor Power Supply Positive (+)
  - Motor connected between BTS7960 M+ and M-

  Created by [Your Name]
*/

const int fsrPin = A0;    // Analog input pin for FSR
const int RPWM_PIN = 5;   // PWM pin connected to BTS7960 RPWM
const int LPWM_PIN = 6;   // PWM pin connected to BTS7960 LPWM

void setup() {
  // Initialize serial communication for debugging (optional)
  Serial.begin(9600);

  // Set PWM pins as outputs
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);

  // Ensure motor is stopped at startup
  analogWrite(RPWM_PIN, 0);
  analogWrite(LPWM_PIN, 0);
}

void loop() {
  // Read the FSR value (0-1023)
  int fsrValue = analogRead(fsrPin);

  // Map the FSR value to PWM range (0-255)
  int pwmValue = map(fsrValue, 0, 1023, 0, 255);

  // Optionally, implement a threshold to ignore low pressures
  if (pwmValue < 10) {
    pwmValue = 0; // Ignore small values
  }

  // Control motor speed
  analogWrite(RPWM_PIN, pwmValue); // Set speed
  analogWrite(LPWM_PIN, 0);        // Ensure opposite pin is LOW

  // Debugging output (optional)
  Serial.print("FSR Value: ");
  Serial.print(fsrValue);
  Serial.print("\tPWM Value: ");
  Serial.println(pwmValue);

  delay(50); // Small delay for stability
}
