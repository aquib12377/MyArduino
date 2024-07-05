
#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial_Black_16.h>
#include <SoftwareSerial.h>

#define DISPLAYS_WIDE 3
#define DISPLAYS_HIGH 1


SoftwareSerial mySerial(2, 3);

SoftDMD dmd(DISPLAYS_WIDE, DISPLAYS_HIGH);
DMD_TextBox box(dmd, 0, 0, 32, 16);

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  Serial.println("Initializing...");
  delay(1000);

  mySerial.println("AT");  // Once the handshake test is successful, it will back to OK
  updateSerial();

  mySerial.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
  mySerial.println("AT+CNMI=1,2,0,0,0");  // Decides how newly arrived SMS messages should be handled
  updateSerial();
  dmd.setBrightness(255);
  dmd.selectFont(Arial_Black_16);
  dmd.begin();
    dmd.drawString(0, 1, F("Waiting for SMS"));
  
}

int phase = 0;
void loop() {
  String c = readSMS();
  if (c.length() > 0) {
    Serial.println("Received SMS: " + c);
    dmd.clearScreen();
    dmd.drawString(0, 1, c);
  }
    dmd.marqueeScrollX(1);

}

void updateSerial() {
  delay(500);
  while (Serial.available()) {
    mySerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());  // Forward what Software Serial received to Serial Port
  }
}

String readSMS() {
  String sms = "";
  delay(50);
  if (mySerial.available() > 0) {
    Serial.println("SMS Starts");
    sms = mySerial.readString();
    Serial.println("Unparsed SMS: "+String(sms));
    sms.trim();
    if (sms.startsWith("+CMT:")) {
      int lIdx = sms.indexOf("\n");
      Serial.println("Last Index: " + String(lIdx));
      sms = sms.substring(lIdx);
      sms.replace("\n", "");
    }
    Serial.println(sms);  // Forward what Software Serial received to Serial Port
    Serial.println("SMS Ends");
  }
  return sms;
}
