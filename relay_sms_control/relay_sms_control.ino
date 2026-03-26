/*
 * SMS Controlled Relay using GSM SIM800L
 * 
 * Hardware Connections:
 * SIM800L TX  -> Arduino Pin 10 (Software Serial RX)
 * SIM800L RX  -> Arduino Pin 11 (Software Serial TX)
 * SIM800L GND -> Arduino GND
 * SIM800L VCC -> 3.7V-4.2V (Use external power supply, NOT Arduino 5V)
 * 
 * Relay Module:
 * Relay IN   -> Arduino Pin 7
 * Relay VCC  -> Arduino 5V
 * Relay GND  -> Arduino GND
 * 
 * SMS Commands:
 * Send "ON" or "on" to turn relay ON
 * Send "OFF" or "off" to turn relay OFF
 * Send "STATUS" to check current relay state
 */

#include <SoftwareSerial.h>

// Define pins
#define SIM800L_TX 8
#define SIM800L_RX 7
#define RELAY_PIN 9

// Create software serial for SIM800L
SoftwareSerial sim800l(SIM800L_TX, SIM800L_RX);

// Variables
String incomingSMS = "";
bool relayState = false;

void setup() {
  // Initialize serial communications
  Serial.begin(115200);
  sim800l.begin(115200);
  
  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Start with relay OFF
  
  Serial.println("Initializing SIM800L...");
  delay(3000);  // Wait for module to start
  
  // Configure SIM800L
  sendATCommand("AT", 1000);              // Check if module responds
  sendATCommand("AT+CMGF=1", 1000);       // Set SMS to text mode
  sendATCommand("AT+CNMI=2,2,0,0,0", 1000); // Configure to receive SMS directly
  sendATCommand("AT+CMGD=1,4", 2000);     // Delete all SMS to free memory
  
  Serial.println("System Ready!");
  Serial.println("Waiting for SMS commands...");
}

void loop() {
  // Check if data available from SIM800L
  if (sim800l.available()) {
    char c = sim800l.read();
    incomingSMS += c;
    
    // Check if we received complete SMS
    if (incomingSMS.indexOf("\n") > 0) {
      processSMS(incomingSMS);
      incomingSMS = "";  // Clear buffer
    }
  }
  
  // Echo any serial input to SIM800L for debugging
  if (Serial.available()) {
    sim800l.write(Serial.read());
  }
}

void processSMS(String message) {
  Serial.println("Received: " + message);
  
  // Convert to uppercase for easier comparison
  message.toUpperCase();
  
  // Check for relay ON command
  if (message.indexOf("ON") > 0 && message.indexOf("OFF") < 0) {
    turnRelayOn();
    sendSMS("Relay turned ON");
  }
  // Check for relay OFF command
  else if (message.indexOf("OFF") > 0) {
    turnRelayOff();
    sendSMS("Relay turned OFF");
  }
  // Check for status command
  else if (message.indexOf("STATUS") > 0) {
    String status = relayState ? "Relay is ON" : "Relay is OFF";
    sendSMS(status);
  }
}

void turnRelayOn() {
  digitalWrite(RELAY_PIN, HIGH);
  relayState = true;
  Serial.println("Relay ON");
}

void turnRelayOff() {
  digitalWrite(RELAY_PIN, LOW);
  relayState = false;
  Serial.println("Relay OFF");
}

void sendSMS(String message) {
  // Note: Replace with your phone number
  // Format: "+1234567890" (include country code)
  String phoneNumber = "+1234567890";  // CHANGE THIS!
  
  sim800l.print("AT+CMGS=\"");
  sim800l.print(phoneNumber);
  sim800l.println("\"");
  delay(500);
  
  sim800l.print(message);
  delay(500);
  
  sim800l.write(26);  // Send Ctrl+Z to send SMS
  delay(2000);
  
  Serial.println("SMS Sent: " + message);
}

void sendATCommand(String command, int timeout) {
  sim800l.println(command);
  long int time = millis();
  String response = "";
  
  while ((time + timeout) > millis()) {
    while (sim800l.available()) {
      char c = sim800l.read();
      response += c;
    }
  }
  
  Serial.print(command);
  Serial.print(" -> ");
  Serial.println(response);
}
