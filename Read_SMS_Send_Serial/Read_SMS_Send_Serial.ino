#include <SoftwareSerial.h>

/* ===== GSM ===== */
SoftwareSerial gsm(7, 8);   // DO NOT CHANGE

/* ===== GPS ===== */
SoftwareSerial gps(4, 5);

/* ===== Pins ===== */
#define BUTTON_PIN 2
#define TRIG_PIN   9
#define ECHO_PIN   10
#define BUZZER_PIN 11

bool smsSent = false;
String gpsData = "GPS not fixed";

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  gsm.begin(115200);   // SAME AS YOUR WORKING CODE
  gps.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("System Init");
  gsm.listen();
  delay(2000);
  gsm.println("AT");
  updateGSM();

  gsm.println("AT+CMGF=1");
  updateGSM();
}

/* ================= LOOP ================= */
void loop() {

  readGPS();
  obstacleCheck();
  updateGSM();

  if (digitalRead(BUTTON_PIN) == LOW && !smsSent) {
    sendHelpSMS();
    smsSent = true;
  }
}

/* ================= GSM ================= */
void sendHelpSMS() {

  Serial.println("Sending SMS...");

  gsm.print("AT+CMGS=\"+91\"\r");
  delay(1500);
  updateGSM();   // WAIT FOR '>'

  gsm.print("HELP! I need assistance.\nhttps://maps.app.goo.gl/dESg7tPaaRqBoE6R9");
  //gsm.print(gpsData);
  delay(500);

  gsm.write(26); // CTRL+Z
  delay(3000);
  updateGSM();
}

void updateGSM() {
  while (gsm.available()) {
    Serial.write(gsm.read());   // PRINT GSM RESPONSE
  }
}

/* ================= GPS ================= */
void readGPS() {
  static String line = "";

  while (gps.available()) {
    char c = gps.read();
    if (c == '\n') {
      if (line.startsWith("$GPRMC") || line.startsWith("$GPGGA")) {
        gpsData = line;
        Serial.println("GPS: " + gpsData);
      }
      line = "";
    } else {
      line += c;
    }
  }
}

/* ================= ULTRASONIC ================= */
void obstacleCheck() {
  long duration, distance;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 25000);
  distance = duration * 0.034 / 2;
Serial.println(distance);
  if (distance > 0 && distance < 40 && distance != 0) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}
