#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"

// Shared control pins for both multiplexers
#define S0 2
#define S1 3
#define S2 4
#define S3 5

// Signal pins for the two multiplexers
#define MUX1_SIG 6 // Signal pin for first multiplexer
#define MUX2_SIG 7 // Signal pin for second multiplexer

// DFPlayer Mini setup
SoftwareSerial mySerial(10, 11); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

// Total number of buttons
#define NUM_BUTTONS 32
int lastButtonState[NUM_BUTTONS] = {0}; // Array to track previous button states

void setup() {
  Serial.begin(9600); // Initialize serial communication
  mySerial.begin(9600);

  Serial.println("Initializing DFPlayer Mini...");
  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("DFPlayer Mini initialization failed!");
    //while (true); // Stop execution if initialization fails
  }
  myDFPlayer.setTimeOut(500); // Set serial timeout
  myDFPlayer.volume(50);      // Set volume (range: 0-30)
  Serial.println("DFPlayer Mini initialized successfully!");
  
  pinMode(MUX1_SIG, INPUT_PULLUP);
  pinMode(MUX2_SIG, INPUT_PULLUP);
  // Set multiplexer control pins as outputs
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT);

  Serial.println("Setup complete. Starting loop...");
}

void loop() {
  // Read buttons on the first multiplexer
  for (int i = 0; i < 16; i++) {
    selectMuxChannel(i); // Select channel on the multiplexer
    int buttonState = digitalRead(MUX1_SIG); // Read button state from MUX1
    handleButtonPress(i, buttonState); // Process button press
  }

  // Read buttons on the second multiplexer
  for (int i = 0; i < 16; i++) {
    selectMuxChannel(i); // Select channel on the multiplexer
    int buttonState = digitalRead(MUX2_SIG); // Read button state from MUX2
    handleButtonPress(i + 16, buttonState); // Process button press (offset by 16)
  }
}

// Function to select a channel on the multiplexer
void selectMuxChannel(int channel) {
  digitalWrite(S0, channel & 1); // Set S0 (bit 0)
  digitalWrite(S1, channel & 2); // Set S1 (bit 1)
  digitalWrite(S2, channel & 4); // Set S2 (bit 2)
  digitalWrite(S3, channel & 8); // Set S3 (bit 3)
}

// Function to handle button press
void handleButtonPress(int buttonIndex, int buttonState) {
  if (buttonState == LOW && lastButtonState[buttonIndex] == HIGH) {
    // Button pressed
    Serial.print("Button ");
    Serial.print(buttonIndex);
    Serial.println(" pressed!");
    playAudioFile(buttonIndex + 1); // Play corresponding audio file
  }
  lastButtonState[buttonIndex] = buttonState; // Update last state
}

// Function to play an audio file based on the button index
void playAudioFile(int fileNumber) {
  Serial.print("Playing audio file: ");
  Serial.println(fileNumber);
  myDFPlayer.play(fileNumber); // Play the audio file
}
