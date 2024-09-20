#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>

#define SD_ChipSelectPin 10  // SD card select pin
#define RELAY1_PIN A0        // Pin for Relay 1
#define RELAY2_PIN A1        // Pin for Relay 2
#define RELAY3_PIN A2        // Pin for Relay 3
#define RELAY4_PIN A3        // Pin for Relay 4
#define BUTTON1_PIN 2        // Button 1 pin
#define BUTTON2_PIN 3        // Button 2 pin
#define BUTTON3_PIN 4        // Button 3 pin
#define BUTTON4_PIN 5        // Button 4 pin
#define RESET_BUTTON_PIN 6   // Reset button pin

TMRpcm tmrpcm;

void setup() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  tmrpcm.speakerPin = 9;
  Serial.begin(9600);
  
  if (!SD.begin(SD_ChipSelectPin)) {
    Serial.println("SD card initialization failed!");
    return;
  } else {
    Serial.println("SD card initialization success!");
  }
}

void loop() {
  if (digitalRead(BUTTON1_PIN) == LOW) {
    Serial.println("Button 1 Pressed");
    playSoundAndActivateRelay("1.wav", RELAY1_PIN);
  } 
  else if (digitalRead(BUTTON2_PIN) == LOW) {
    Serial.println("Button 2 Pressed");
    playSoundAndActivateRelay("2.wav", RELAY2_PIN);
  } 
  else if (digitalRead(BUTTON3_PIN) == LOW) {
    Serial.println("Button 3 Pressed");
    playSoundAndActivateRelay("3.wav", RELAY3_PIN);
  } 
  else if (digitalRead(BUTTON4_PIN) == LOW) {
    Serial.println("Button 4 Pressed");
    playSoundAndActivateRelay("4.wav", RELAY4_PIN);
  }
  
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    Serial.println("Reset Button Pressed");
    resetGame();
  }
}

void playSoundAndActivateRelay(const char* filename, int relayPin) {
  Serial.print("Activating relay pin: ");
  Serial.println(relayPin);
  
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);
  digitalWrite(relayPin, LOW);
  
  Serial.print("Playing sound: ");
  Serial.println(filename);
  
  tmrpcm.play(filename);          
  delay(5000);                    

  Serial.print("Deactivating relay pin: ");
  Serial.println(relayPin);
  digitalWrite(relayPin, HIGH);
}

void resetGame() {
  Serial.println("Resetting game");
  tmrpcm.stopPlayback();          
  digitalWrite(RELAY1_PIN, HIGH);  
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);
}
