#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>
#include <fonts/Arial14.h>
#include <SoftwareSerial.h>

// Set Width to the number of displays wide you have
const int WIDTH = 3;
const int HEIGHT = 1;

// DMD setup
SoftDMD dmd(WIDTH, HEIGHT);  // DMD controls the entire display
DMD_TextBox box(dmd);        // "box" provides a text box to automatically write to/scroll the display

// GSM setup
SoftwareSerial gsmSerial(7, 8);  // RX, TX pins

void setup() {
  Serial.begin(9600);
  gsmSerial.begin(9600);
  
Serial.println("Initialize....");
  gsmSerial.println("AT");  // Once the handshake test is successful, it will back to OK
  updateSerial();

  gsmSerial.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
  gsmSerial.println("AT+CNMI=1,2,0,0,0");  // Decides how newly arrived SMS messages should be handled
  updateSerial();
  dmd.setBrightness(255);
  dmd.selectFont(SystemFont5x7);
  dmd.begin();
}
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    gsmSerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  }
  while (gsmSerial.available()) {
    Serial.write(gsmSerial.read());  // Forward what Software Serial received to Serial Port
  }
}
void loop() {
  // Request the module to read all SMS
  //gsmSerial.println("AT+CMGL=\"REC UNREAD\"");

    String message = readSMS();
    
      

      // Display SMS content on the DMD display
      dmd.clearScreen();
      box.print(message.c_str());
      
      long startTime = millis();
      long timer = startTime;
      while ((timer - startTime) < 10000) {  // Display the message for 10 seconds
        timer = millis();
        if ((timer % 500) == 0) {
          box.print(message.c_str());
          
        }
      }}  
String readSMS() {
  String sms = "";
  delay(800);
  while (Serial.available()) {
    gsmSerial.write(Serial.read());  // Forward what Serial received to Software Serial Port
  }
  while (gsmSerial.available()) {
    Serial.println("SMS Starts");
    sms = gsmSerial.readString();
    Serial.println(sms);
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
