// Include Required Libraries
#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial14.h>

// Define IR sensor pin
const int IR_SENSOR_PIN = 2; // Digital input pin

// Variables for counting
int count = 0;

// Sensor state variables
bool currentState = HIGH;
bool previousState = HIGH;
int IrCount = 0;
// Create DMD object
SoftDMD dmd(1,1);  // DMD controls the entire display
DMD_TextBox box(dmd, 0, 0);  // "box" provides a text box to automatically write to/scroll the display

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Initialize DMD
  dmd.setBrightness(255);
  dmd.selectFont(Arial14);
  dmd.begin();

  // Set up IR sensor pin
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  // Display initial count
  displayCount();
}

void loop() {
  // Read the current state of the sensor
  currentState = digitalRead(IR_SENSOR_PIN);

  // Check for state change from HIGH to LOW (sensor activated)
  if (previousState == HIGH && currentState == LOW) {
    IrCount++; 
    //Serial.println("Sensor activated");
  }

  // Check for state change from LOW to HIGH (sensor deactivated)
  if (IrCount == 2) {
    // Increment count on complete activation cycle
    count++;
    IrCount = 0;
    //Serial.print("Count: ");
    //Serial.println(count);
    displayCount();
  }
  previousState = currentState;

  // Small delay to prevent bouncing issues
  delay(2);
}

void displayCount() {
  // Clear the display
  dmd.clearScreen();

  // Convert count to string
  char countStr[10];
  sprintf(countStr, "  %d", count);

  // Display count on P10 display
  box.clear();
  dmd.drawString( -20, 0, countStr );
}
