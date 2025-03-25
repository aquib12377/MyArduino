/*******************************************************
 *  Ultrasonic Obstacle Detection with SMS Notification
 *  
 *  This sketch reads distance from an HC-SR04 sensor.
 *  If the distance is under a threshold, it sends an 
 *  SMS using a GSM module (SIM900 / SIM800).
 *******************************************************/

// ------------- Pin Definitions -----------------
#define TRIG_PIN 4      // Trig pin of HC-SR04
#define ECHO_PIN 5     // Echo pin of HC-SR04

#include <SoftwareSerial.h>

// Set RX/TX for SoftwareSerial with the GSM module
SoftwareSerial gsmSerial(2, 3); // (RX, TX) -> Connect SIM module TX->7, RX->8

// ------------- Global Variables -----------------
const int DISTANCE_THRESHOLD = 20;  // in centimeters
long duration;
int distanceCm;

void setup() {
  // Initialize Serial for debugging (optional)
  Serial.begin(9600);
  Serial.println("Initializing...");

  // Initialize GSM serial (9600 is typical; verify your module's baud rate)
  gsmSerial.begin(9600);

  // Initialize Ultrasonic pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Give the GSM module some time to start up
  delay(1000);
  Serial.println("GSM Module setup...");
  
  // Optional: Check if the GSM module responds with "OK"
  sendATCommand("AT");
  delay(1000);

  // Set SMS mode to Text Mode
  sendATCommand("AT+CMGF=1");
  delay(1000);
    sendSMS("+917977845638", "Project Started"); // Replace with recipient phone number
    sendSMS("+919619700752", "Project Started"); // Replace with recipient phone number

  Serial.println("Setup complete.");
}

void loop() {
  // Get distance from ultrasonic sensor
  distanceCm = readUltrasonicDistance(TRIG_PIN, ECHO_PIN);

  // Print distance (for debugging)
  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  // If distance is below threshold, send SMS
  if (distanceCm > 0 && distanceCm <= DISTANCE_THRESHOLD) {
    Serial.println("Obstacle detected! Sending SMS...");
    sendSMS("+917977845638", "Obstacle Detected!"); // Replace with recipient phone number
    sendSMS("+919619700752", "Obstacle Detected!"); // Replace with recipient phone number
    delay(10000);  // Prevent spamming SMS; adjust as needed
  }

  delay(500); // General loop delay
}

/**
 * Reads distance (in cm) from an HC-SR04 ultrasonic sensor
 */
int readUltrasonicDistance(int triggerPin, int echoPin) {
  // Clear the trigger
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);

  // Send a 10us HIGH pulse on the trigger pin
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  // Read the echo pin
  long duration = pulseIn(echoPin, HIGH);

  // Convert time into distance (cm)
  int distance = duration * 0.034 / 2; // speed of sound ~ 343m/s
  return distance;
}

/**
 * Sends an AT command to the GSM module and prints the reply
 */
void sendATCommand(String cmd) {
  gsmSerial.println(cmd);
  delay(100);
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());
  }
}

/**
 * Sends an SMS to a given phone number with a specified message
 */
void sendSMS(String phoneNumber, String message) {
  // Enter Text Mode (redundant if already set, but safe to resend)
  gsmSerial.println("AT+CMGF=1");
  delay(500);

  // Begin sending to phone number
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(500);

  // Send the text message
  gsmSerial.print(message);
  delay(500);

  // End message with Ctrl+Z (ASCII code 26)
  gsmSerial.write(26);
  delay(500);

  // Read and print any reply from the GSM module
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());
  }
  Serial.println("Message sent.");
}
