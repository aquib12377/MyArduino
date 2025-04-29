#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// ——— Pin Definitions ———
// Ultrasonic
#define trigPin     7
#define echoPin     6

// Motors (stop on crack)
#define motor1Pin1  3
#define motor1Pin2  5
#define motor2Pin1  9
#define motor2Pin2 10

// Buzzer
#define buzzerPin  11

// GPS (NEO‑6M) on pins 2/4
SoftwareSerial gpsSerial(A0,A1); // RX = pin 4 (GPS TX), TX = pin 2 (GPS RX)
TinyGPSPlus gps;

// GSM (SIM800) on pins 8/12
SoftwareSerial gsmSerial(A2,A3); // RX = pin 8 (SIM TX), TX = pin 12 (SIM RX)
const char* phoneNumber = "+917977845638";

// Threshold & speed
const int crackThreshold = 4;    // cm
const int motorSpeed     = 100;  // 0–255

// Hard‑coded fallback
const char* fallbackURL = "https://maps.app.goo.gl/7iyREkmqttMR8ReYA";

long measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.0343 / 2;
}

bool networkAvailable() {
  // Ask SIM800 for registration status
  gsmSerial.println("AT+CREG?");
  delay(500);
  while (gsmSerial.available()) {
    String r = gsmSerial.readString();
    if (r.indexOf("+CREG: 0,1") >= 0 || r.indexOf("+CREG: 0,5") >= 0) {
      return true;
    }
  }
  return false;
}

void sendSMS(const String& msg) {
  gsmSerial.println("AT");               // wake
  delay(200);
  gsmSerial.println("AT+CMGF=1");        // text mode
  delay(200);
  gsmSerial.print  ("AT+CMGS=\"");
  gsmSerial.print  (phoneNumber);
  gsmSerial.println("\"");
  delay(200);
  gsmSerial.print(msg);
  delay(200);
  gsmSerial.write (26);                  // CTRL+Z to send
  delay(1000);
}

void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  // Start motors forward
  analogWrite(motor1Pin1, motorSpeed);
  analogWrite(motor1Pin2, 0);
  analogWrite(motor2Pin1, motorSpeed);
  analogWrite(motor2Pin2, 0);

  // Serial modules
  gpsSerial.begin(9600);
  gsmSerial.begin(9600);
  Serial.println("System Initialized");
}

void loop() {
  long distance = measureDistance();
  Serial.print("Distance: "); Serial.print(distance); Serial.println(" cm");

  if (distance > crackThreshold) {
    // Stop everything
    analogWrite(motor1Pin1, 0);
    analogWrite(motor1Pin2, 0);
    analogWrite(motor2Pin1, 0);
    analogWrite(motor2Pin2, 0);
    digitalWrite(buzzerPin, HIGH);
    Serial.println("Crack detected!");

    // Try to get GPS fix (2 s timeout)
    unsigned long start = millis();
    while (millis() - start < 2000) {
      while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
      }
    }

    String link;
    if (gps.location.isValid()) {
      // live link
      link  = "http://maps.google.com/?q=";
      link += String(gps.location.lat(), 6);
      link += ",";
      link += String(gps.location.lng(), 6);
    } else {
      // no GPS fix → fallback
      link = fallbackURL;
    }

    // Send SMS if network; otherwise fallback link
    if (networkAvailable()) {
      sendSMS(link);
      Serial.println("SMS sent: " + link);
    } else {
      sendSMS(fallbackURL);
      Serial.println("No network → fallback SMS");
    }

    // pause so we don’t spam
    delay(10000);
    digitalWrite(buzzerPin, LOW);
    // you can restart motors here if needed
  }
  else {
    // Normal operation
    analogWrite(motor1Pin1, motorSpeed);
    analogWrite(motor1Pin2, 0);
    analogWrite(motor2Pin1, motorSpeed);
    analogWrite(motor2Pin2, 0);
    digitalWrite(buzzerPin, LOW);
  }

  delay(200);
}
