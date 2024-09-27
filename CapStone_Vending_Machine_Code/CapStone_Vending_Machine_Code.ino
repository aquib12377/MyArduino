#include <Servo.h>  // Include the Servo library

Servo myServo;                       // Create a servo object
const int servoPin = 9;              // Define the pin where the servo is connected
const int buttonPin = 2;             // Define the pin where the button is connected
byte lastState = HIGH;               // Track the last state of the button
unsigned long debounceDelay = 50;    // Set debounce delay (in milliseconds)
unsigned long lastDebounceTime = 0;  // Variable to store the last debounce time

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);  // Enable the internal pull-up resistor for the button
  myServo.attach(servoPin);          // Attach the servo to the pin
  myServo.write(90);                 // Set the initial position of the servo to 90 degrees
  Serial.begin(9600);                // Start serial communication
}

void loop() {
  int buttonState = digitalRead(buttonPin);  // Read the current state of the button

  if (buttonState == LOW && lastState == HIGH) {
    Serial.println("Button Pressed");
    myServo.write(0);   // Move servo to 0 degrees
    delay(5000);        // Keep the servo in position for 5 seconds
    myServo.write(90);  // Return servo to 90 degrees
  }
  lastState = buttonState;
  delay(100);
}
