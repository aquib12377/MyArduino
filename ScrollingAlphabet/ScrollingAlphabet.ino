#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Arial_Black_16.h>

// Set Width to the number of displays wide you have
const int WIDTH = 3;

// You can change to a smaller font (two lines) by commenting this line,
// and uncommenting the line after it:
const uint8_t *FONT = Arial_Black_16;
//const uint8_t *FONT = SystemFont5x7;

SoftDMD dmd(WIDTH, 1);  // DMD controls the entire display
DMD_TextBox box(dmd);   // "box" provides a text box to automatically write to/scroll the display

String currentMessage = "hello";  // Variable to store the current message

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(9600);
  dmd.setBrightness(255);
  dmd.selectFont(FONT);
  dmd.begin();
}

// the loop routine runs over and over again forever:
void loop() {
  // Check if there is a new message available
  if (Serial.available()) {
    currentMessage = Serial.readString(); // Read input from Serial Monitor

    // Clear the display before showing new input
    dmd.clearScreen();
  }

  // Scroll the current message
  for (int i = 0; i < currentMessage.length(); i++) {
    box.print(currentMessage[i]);
    delay(500); // Adjust delay as needed
  }
  dmd.clearScreen();
}
