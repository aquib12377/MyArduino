/*******************************************************
 *  Dual‑Event Relay Controller ⸺ E18‑D80 + Ultrasonic
 *  ----------------------------------------------------
 *  • E18‑D80NK IR sensor (pin 6)  
 *      ▸ object → R1 ON / R2 OFF (10 s) → R1 OFF / R2 ON (10 s) → both OFF
 *
 *  • HC‑SR04  (Trig A2, Echo A1)  
 *      ▸ distance < 10 cm → R3 ON / R4 OFF (10 s) → R3 OFF / R4 ON (10 s) → both OFF  
 *      ▸ sends one SMS with GPS position when sequence starts
 *
 *  • GPS (Neo‑6M, etc.)  TX→A4  RX→A3  – SoftwareSerial + TinyGPS++  
 *  • GSM SIM800L on hardware TX/RX (Serial) at 9600 baud
 *
 *  • Relay pins: 2, 3, 4, 5   (active‑HIGH → change LOGIC_HIGH/LOW if your module is active‑LOW)
 *******************************************************/

#include <SoftwareSerial.h>
#include <TinyGPS++.h>

/* ---- pins ---- */
const uint8_t RELAY1 = 2;
const uint8_t RELAY2 = 3;
const uint8_t RELAY3 = 4;
const uint8_t RELAY4 = 5;

const uint8_t E18_PIN   = 6;      // E18‑D80NK output (LOW = object)
const uint8_t TRIG_PIN  = A2;     // HC‑SR04
const uint8_t ECHO_PIN  = A1;

/* ---- GPS ---- */
SoftwareSerial gpsSS(A4, A3);     // RX (A4) ← GPS‑TX,  TX (A3) → GPS‑RX
TinyGPSPlus  gps;

/* ---- GSM ---- */
const unsigned long GSM_BAUD = 9600;
const char PHONE_NUMBER[]    = "+917977845638";     // << change

/* ---- constants ---- */
const unsigned long PHASE_MS  = 10000UL;    // 10 s each phase
const int  FULL_CM_THRESHOLD  = 10;          // <10 cm = full

/* ---- relay logic level ---- */
const uint8_t ON  = HIGH;
const uint8_t OFF = LOW;

/* ---- helper structure for 10 + 10 s sequences ---- */
struct Seq {
  uint8_t phase = 0;             // 0 = idle, 1 = first 10 s, 2 = second 10 s
  unsigned long t0 = 0;
  bool smsDone = false;          // only for garbage sequence
};

Seq objSeq, garbSeq;

/* ------------------------------------------------------------------ */
void setup() {
  /* relays */
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  digitalWrite(RELAY1, OFF);
  digitalWrite(RELAY2, OFF);
  digitalWrite(RELAY3, OFF);
  digitalWrite(RELAY4, OFF);

  /* sensors */
  pinMode(E18_PIN,  INPUT_PULLUP);   // E18‑D80NK open‑collector
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  /* serials */
  Serial.begin(GSM_BAUD);            // GSM – hardware UART
  gpsSS.begin(9600);                 // GPS

  delay(3000);                       // give modules time
  initSIM800();
}

/* ------------------------------------------------------------------ */
void loop() {
  readGPS();               // keep gps data fresh
  handleObjectSeq();       // E18‑D80 sequence
  handleGarbageSeq();      // ultrasonic + SMS sequence
}

/* ==================================================================
 *  Sensor + sequence handlers
 * ================================================================== */
void handleObjectSeq() {
  bool detected = digitalRead(E18_PIN) == LOW;   // object present?

  if (objSeq.phase == 0 && detected) {           // start sequence
    objSeq.phase = 1;
    objSeq.t0    = millis();
    digitalWrite(RELAY1, ON);
    digitalWrite(RELAY2, OFF);
  }
  else if (objSeq.phase == 1 && millis() - objSeq.t0 >= PHASE_MS) {
    objSeq.phase = 2;
    objSeq.t0    = millis();
    digitalWrite(RELAY1, OFF);
    digitalWrite(RELAY2, ON);
  }
  else if (objSeq.phase == 2 && millis() - objSeq.t0 >= PHASE_MS) {
    objSeq.phase = 0;
    digitalWrite(RELAY1, OFF);
    digitalWrite(RELAY2, OFF);
  }
}

void handleGarbageSeq() {
  static unsigned long lastPing = 0;
  static int distanceCm = 100;                   // start far

  /* sample ultrasonic every 250 ms */
  if (millis() - lastPing >= 250) {
    lastPing = millis();
    distanceCm = readDistanceCm();
  }

  bool binFull = (distanceCm > 0 && distanceCm < FULL_CM_THRESHOLD);

  if (garbSeq.phase == 0 && binFull) {           // start sequence
    garbSeq.phase  = 1;
    garbSeq.t0     = millis();
    digitalWrite(RELAY3, ON);
    digitalWrite(RELAY4, OFF);
    if (!garbSeq.smsDone) {
      sendFullSMS();
      garbSeq.smsDone = true;
    }
  }
  else if (garbSeq.phase == 1 && millis() - garbSeq.t0 >= PHASE_MS) {
    garbSeq.phase = 2;
    garbSeq.t0    = millis();
    digitalWrite(RELAY3, OFF);
    digitalWrite(RELAY4, ON);
  }
  else if (garbSeq.phase == 2 && millis() - garbSeq.t0 >= PHASE_MS) {
    garbSeq.phase = 0;
    digitalWrite(RELAY3, OFF);
    digitalWrite(RELAY4, OFF);
  }

  /* reset SMS flag when bin no longer full */
  if (!binFull && garbSeq.phase == 0) garbSeq.smsDone = false;
}

/* ==================================================================
 *  Ultrasonic helper
 * ================================================================== */
int readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000);   // 30 ms timeout (≈5 m)
  if (dur == 0) return -1;                               // timeout
  return int(dur * 0.0343 / 2.0);                        // cm
}

/* ==================================================================
 *  GPS helpers
 * ================================================================== */
double  currLat  = 0.0,  currLon = 0.0;
bool    locValid = false;

void readGPS() {
  while (gpsSS.available()) {
    if (gps.encode(gpsSS.read())) {
      if (gps.location.isValid()) {
        currLat  = gps.location.lat();
        currLon  = gps.location.lng();
        locValid = true;
      }
    }
  }
}

/* ==================================================================
 *  GSM / SMS helpers
 * ================================================================== */
void initSIM800() {
  sendCmd("AT");                delay(500);
  sendCmd("ATE0");              delay(300);
  sendCmd("AT+CMGF=1");         delay(300);    // text mode
  sendCmd("AT+CSCS=\"GSM\"");   delay(300);
}

void sendFullSMS() {
  char msg[80];
  if (locValid) {
    snprintf(msg, sizeof(msg), "Bin full! Location: %.6f,%.6f", currLat, currLon);
  } else {
    strcpy(msg, "Bin full! (GPS unavailable)");
  }

  sendCmd("AT+CMGF=1");
  delay(200);
  Serial.print("AT+CMGS=\"");
  Serial.print(PHONE_NUMBER);
  Serial.println("\"");
  delay(200);
  Serial.print(msg);          // message body
  Serial.write(26);           // Ctrl‑Z to send
  delay(5000);                // give the modem time
}

void sendCmd(const char *cmd) {
  Serial.print(cmd);
  Serial.print("\r");
}

/* ================================================================== */
