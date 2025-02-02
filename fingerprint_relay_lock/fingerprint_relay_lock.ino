#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

SoftwareSerial mySerial(2, 3);
LiquidCrystal_I2C lcd(0x27, 16, 2);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

bool lastState = -1;
int relay = 8;

void setup()  
{
  pinMode( relay, OUTPUT);
  digitalWrite(relay, HIGH);  
  lcd.begin();
  lcd.backlight();
  Serial.begin(9600);
  while (!Serial);  // For Yun/Leo/Micro/Zero/...
  delay(100);
  Serial.println("\n\nAdafruit finger detect test");
  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  finger.getTemplateCount();
  Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  Serial.println("Waiting for valid finger...");
  displayWaitFinger(); 
}



void loop()                     // run over and over again
{ 
  int id;
  id = getFingerprintIDez();
  if( id == -1 ){
     if( lastState == 0){
         lastState = -1;
         displayInvalidFinger();
         delay(2000);
         displayWaitFinger();
     }
  }
  else if( id != -1){
       digitalWrite(relay, LOW);
      displayFingerOK();
      delay(2000);
      displayWaitFinger();
       digitalWrite(relay, HIGH);
  }
  delay(50);            //don't need to run this at full speed.
}


uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence); 
  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #

int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  Serial.println(p);
   if( p == 0 ){
      lastState = 0;
   }

  if (p != FINGERPRINT_OK)  return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;
  lastState = 1;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID; 
}

void displayWaitFinger(){
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("PLACE FINGER");
   lcd.setCursor(0,1);
   lcd.print("ON SCANNER");
}

void displayInvalidFinger(){
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("NOT ENROLLED");
   lcd.setCursor(0,1);
   lcd.print("ACCESS DENIED!");
}

void displayFingerOK(){
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("WELCOME ");
   lcd.setCursor(0,1);
   lcd.print("DOOR UNLOCKED");
}