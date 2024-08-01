
#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial14.h>
#include <SoftwareSerial.h>

#define DISPLAYS_WIDE 3
#define DISPLAYS_HIGH 1


SoftwareSerial mySerial(2, 3);

SoftDMD dmd(DISPLAYS_WIDE, DISPLAYS_HIGH);
DMD_TextBox box(dmd, 0, 0, 32, 16);

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing...");
  delay(1000);
  dmd.setBrightness(255);
  dmd.selectFont(Arial14);
  dmd.begin();
  dmd.drawString(0, 1, F("Waiting for SMS"));
}

int phase = 0;
void loop() {
  String c = readSMS();
  if (c.length() > 0) {
    Serial.println("Received SMS: " + c);
    char char_array_txt_1[c.length() + 1];
    c.toCharArray(char_array_txt_1, c.length() + 1);
    dmd.clearScreen();
    delay(1000);
    dmd.drawString(-1, 0, char_array_txt_1);
  }
  dmd.marqueeScrollX(-1);
}

void updateSerial() {
  while (Serial.available()) {
    mySerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());  // Forward what Software Serial received to Serial Port
  }
}

String readSMS() {
  mySerial.listen();
  String sms = "";
  delay(50);
  if (Serial.available() > 0) {
    Serial.println("SMS Starts");
    sms = Serial.readString();
    sms.trim();
    Serial.println("Unparsed SMS: " + String(sms));

    if (sms.startsWith("+CMT:")) {
      int lIdx = sms.indexOf("\n");
      Serial.println("Last Index: " + String(lIdx));
      sms = sms.substring(lIdx);
      sms.replace("\n", "");
    }
    Serial.println(sms);  // Forward what Software Serial received to Serial Port
    Serial.println("SMS Ends");
  }
  Serial.flush();
  mySerial.flush();
  sms.trim();
  return sms;
}
