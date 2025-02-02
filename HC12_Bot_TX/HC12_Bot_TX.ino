#include <SoftwareSerial.h>

// Define pins for HC-12 communication
#define HC12_RX 2 // Arduino RX pin connected to HC-12 TX
#define HC12_TX 3 // Arduino TX pin connected to HC-12 RX

// Button pins
#define BUTTON_FORWARD 4 // Forward
#define BUTTON_BACKWARD 5 // Backward
#define BUTTON_LEFT 11 // Left
#define BUTTON_RIGHT 12 // Right

// Define commands to send via HC-12
#define CMD_FORWARD "FORWARD"
#define CMD_BACKWARD "BACKWARD"
#define CMD_LEFT "LEFT"
#define CMD_RIGHT "RIGHT"

// Initialize SoftwareSerial for HC-12
SoftwareSerial HC12(HC12_RX, HC12_TX);

void setup() {
  // Start the serial communication
  Serial.begin(9600);
  HC12.begin(9600);

  // Set button pins as input with pull-up resistors
  pinMode(BUTTON_FORWARD, INPUT_PULLUP);
  pinMode(BUTTON_BACKWARD, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  // Print ready message to serial monitor
  Serial.println("HC-12 BOT Controller Ready!");
}

void loop() {
  // Check if Forward button is pressed
  if (digitalRead(BUTTON_FORWARD) == LOW) {
    sendCommand(CMD_FORWARD);
    delay(50); // Debounce delay
  }

  // Check if Backward button is pressed
  else if (digitalRead(BUTTON_BACKWARD) == LOW) {
    sendCommand(CMD_BACKWARD);
    delay(50); // Debounce delay
  }

  // Check if Left button is pressed
  else if (digitalRead(BUTTON_LEFT) == LOW) {
    sendCommand(CMD_LEFT);
    delay(50); // Debounce delay
  }

  // Check if Right button is pressed
  else if (digitalRead(BUTTON_RIGHT) == LOW) {
    sendCommand(CMD_RIGHT);
    delay(50); // Debounce delay
  }
  else{
    sendCommand("STOP");
    delay(50);
  }
}

// Function to send a command via HC-12
void sendCommand(const char* command) {
  HC12.println(command); // Send command via HC-12
  Serial.print("Sent: "); // Log to serial monitor
  Serial.println(command);
}
