#include <SoftwareSerial.h>
SoftwareSerial mySerial(2, 3);

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
}

void loop() {
  //updateSerial();
  String c = readSMS();
  if (c.length() > 0) {
    //Serial.println("Received SMS: " + c);
  }
}

void updateSerial() {
  delay(500);
  // while (Serial.available()) {
  //   mySerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  // }
  // while (mySerial.available()) {
  //   Serial.write(mySerial.read());  // Forward what Software Serial received to Serial Port
  // }
}

String readSMS() {
  mySerial.listen();
  String sms = "";
  delay(500);
  if (mySerial.available() > 0) {
    //Serial.println("SMS Starts");
    sms = mySerial.readString();
    sms.trim();
    //Serial.println("Unparsed SMS: "+String(sms));

    if (sms.startsWith("+CMT:")) {
      int lIdx = sms.indexOf("\n");
      //Serial.println("Last Index: " + String(lIdx));
      sms = sms.substring(lIdx);
      sms.replace("\n", "");
    }
    Serial.println(sms);  // Forward what Software Serial received to Serial Port
    //Serial.println("SMS Ends");
  }
  Serial.flush();
  mySerial.flush();
  return sms;
}
