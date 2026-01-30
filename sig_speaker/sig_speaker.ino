#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"

SoftwareSerial mp3Serial(10, 11); // RX, TX
DFRobotDFPlayerMini mp3;

const int triggerPin = 12;
unsigned long lastPlayTime = 0;
bool cooldownActive = false;

void setup()
{
  pinMode(triggerPin, INPUT);

  Serial.begin(9600);
  mp3Serial.begin(9600);

  Serial.println("Initializing DFPlayer...");

  if (!mp3.begin(mp3Serial)) {
    Serial.println("DFPlayer not found!");
    while (true);
  }

  Serial.println("DFPlayer Mini Online.");
  mp3.volume(20);
}

void loop()
{
  int signal = digitalRead(triggerPin);

  unsigned long currentTime = millis();

  // -------- Cooldown Monitoring --------
  if (cooldownActive && (currentTime - lastPlayTime >= 15000)) {
    cooldownActive = false;  // 15 seconds passed → enable playback again
    Serial.println("Cooldown finished. Ready to play again.");
  }

  // -------- Play Audio When HIGH AND no cooldown --------
  if (signal == HIGH && !cooldownActive) {
    Serial.println("Trigger HIGH → Playing 0001.mp3");

    mp3.play(3);  // play 0001.mp3

    cooldownActive = true;        // start blocking future plays
    lastPlayTime = currentTime;   // remember play time
  }
}
