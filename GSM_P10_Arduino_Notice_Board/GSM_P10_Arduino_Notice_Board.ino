#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial14.h>
#include <SoftwareSerial.h>

#define DISPLAYS_WIDE 3
#define DISPLAYS_HIGH 1

SoftwareSerial mySerial(2, 3);

SoftDMD dmd(5, DISPLAYS_HIGH);
DMD_TextBox box(dmd, 0, 0, 96, 16);  // Adjust width to fit your display configuration

void setup() {
  Serial.begin(9600);
  Serial.println("Initializing...");
  delay(1000);
  dmd.setBrightness(255);
  dmd.selectFont(Arial14);
  dmd.begin();
  box.print(F("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
}

void loop() {
  String c = readSMS();
  if (c.length() > 0) {
    Serial.println("Received SMS: " + c);
    char char_array_txt_1[c.length() + 1];
    c.toCharArray(char_array_txt_1, c.length() + 1);
    box.clear();
    delay(1000);
    dmd.drawString(0,0.);  // Use the text box to handle the text display
  }
  dmd.marqueeScrollX(-1);
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
    Serial.println(sms);
    Serial.println("SMS Ends");
  }
  Serial.flush();
  mySerial.flush();
  sms.trim();
  return sms;
}
