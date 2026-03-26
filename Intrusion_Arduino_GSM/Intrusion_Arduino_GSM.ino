/*
 * Intrusion Detection System with GSM SMS Alert
 * Components:
 * - Piezo Sensor on A0 (detects vibration/knock)
 * - LCD I2C Display (shows system status)
 * - Buzzer on Pin 2 (alarm sound)
 * - Servo Motor on Pin 9 (controls gate)
 * - SIM800L GSM Module (TX → Pin 10, RX → Pin 11)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ── Pin Definitions ──────────────────────────────────────────
const int PIEZO_PIN  = A0;
const int BUZZER_PIN = 2;
const int SERVO_PIN  = 9;
const int GSM_TX_PIN = 10; // Arduino TX → SIM800L RX
const int GSM_RX_PIN = 11; // Arduino RX → SIM800L TX

// ── GSM Settings ─────────────────────────────────────────────
// !!! CHANGE THIS to the phone number that should receive alerts !!!
#define ALERT_PHONE_NUMBER "+919673643387"

SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN);

// ── Sensor Thresholds ────────────────────────────────────────
const int KNOCK_THRESHOLD     = 900; // Adjust based on piezo sensitivity
const int INTRUSION_THRESHOLD = 3;   // Number of knocks to trigger alarm

// ── LCD Setup ────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change 0x27 if your LCD uses a different I2C address

// ── Servo Setup ──────────────────────────────────────────────
Servo gateServo;
const int GATE_OPEN_ANGLE   = 90;
const int GATE_CLOSED_ANGLE = 0;

// ── System Variables ─────────────────────────────────────────
int           knockCount      = 0;
unsigned long lastKnockTime   = 0;
unsigned long alarmStartTime  = 0;
bool          alarmActive     = false;
bool          gateOpen        = true;
bool          smsSent         = false; // Prevents sending multiple SMS per alarm event

const unsigned long KNOCK_TIMEOUT  = 2000;  // ms — reset knock count after inactivity
const unsigned long ALARM_DURATION = 10000; // ms — alarm auto-resets after 10 seconds

// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  // ── LCD Init ──
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Intrusion Detect");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // ── Buzzer Init ──
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // ── Servo Init ──
  gateServo.attach(SERVO_PIN);
  gateServo.write(GATE_OPEN_ANGLE);

  // ── GSM Init ──
  gsmSerial.begin(9600);
  delay(1000);
  initGSM();

  // ── Startup Complete ──
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Status: SECURE");

  Serial.println("Intrusion Detection System Started");
}

// ─────────────────────────────────────────────────────────────

void loop() {
  int piezoValue = analogRead(PIEZO_PIN);
  Serial.println(piezoValue);

  // ── Detect Knock ──
  if (piezoValue > KNOCK_THRESHOLD && !alarmActive) {
    knockCount++;
    lastKnockTime = millis();

    Serial.print("Knock detected! Count: ");
    Serial.println(knockCount);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Vibration Det!");
    lcd.setCursor(0, 1);
    lcd.print("Count: ");
    lcd.print(knockCount);

    tone(BUZZER_PIN, 2000, 100); // Brief beep feedback
    delay(200); // Debounce
  }

  // ── Reset knock count after timeout ──
  if (millis() - lastKnockTime > KNOCK_TIMEOUT && knockCount > 0 && !alarmActive) {
    knockCount = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    lcd.setCursor(0, 1);
    lcd.print("Status: SECURE");
  }

  // ── Trigger Alarm ──
  if (knockCount >= INTRUSION_THRESHOLD && !alarmActive) {
    triggerAlarm();
  }

  // ── Handle Active Alarm ──
  if (alarmActive) {
    handleAlarm();
  }

  delay(50);
}

// ─────────────────────────────────────────────────────────────

void triggerAlarm() {
  alarmActive    = true;
  knockCount     = 0;
  alarmStartTime = millis();
  smsSent        = false; // Allow a fresh SMS for this alarm event

  Serial.println("INTRUSION DETECTED!");

  closeGate();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!!! ALERT !!!");
  lcd.setCursor(0, 1);
  lcd.print("INTRUSION!");

  // ── Send SMS immediately on alarm trigger ──
  if (!smsSent) {
    sendSMS("ALERT: Intrusion detected! Gate has been closed. Check your premises immediately.");
    smsSent = true;
  }
}

// ─────────────────────────────────────────────────────────────

void handleAlarm() {
  // ── Alarm Buzzer Pattern ──
  tone(BUZZER_PIN, 2500, 200);
  delay(200);
  tone(BUZZER_PIN, 2000, 200);
  delay(200);

  // ── LCD Backlight Flash ──
  static unsigned long lastBlink = 0;
  static bool backlightState = true;

  if (millis() - lastBlink > 500) {
    backlightState = !backlightState;
    backlightState ? lcd.backlight() : lcd.noBacklight();
    lastBlink = millis();
  }

  // ── Auto-reset after ALARM_DURATION ──
  if (millis() - alarmStartTime >= ALARM_DURATION) {
    resetSystem();
  }
}

// ─────────────────────────────────────────────────────────────

void closeGate() {
  if (gateOpen) {
    Serial.println("Closing gate...");
    gateServo.write(GATE_CLOSED_ANGLE);
    gateOpen = false;
    delay(500);
  }
}

void openGate() {
  if (!gateOpen) {
    Serial.println("Opening gate...");
    gateServo.write(GATE_OPEN_ANGLE);
    gateOpen = true;
    delay(500);
  }
}

// ─────────────────────────────────────────────────────────────

void resetSystem() {
  Serial.println("System reset");

  alarmActive    = false;
  knockCount     = 0;
  alarmStartTime = 0;
  smsSent        = false;

  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Reset");
  lcd.setCursor(0, 1);
  lcd.print("Status: SECURE");

  openGate();
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Status: SECURE");
}

// ─────────────────────────────────────────────────────────────
// GSM Functions
// ─────────────────────────────────────────────────────────────

void initGSM() {
  Serial.println("Initializing GSM...");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GSM Init...");

  sendATCommand("AT", 1000);           // Basic handshake
  sendATCommand("AT+CMGF=1", 1000);   // Set SMS to text mode
  sendATCommand("AT+CNMI=1,2,0,0,0", 1000); // New SMS notification

  Serial.println("GSM Ready");
  lcd.setCursor(0, 1);
  lcd.print("GSM Ready!  ");
  delay(1000);
}

void sendSMS(String message) {
  Serial.println("Sending SMS...");
  Serial.println("To: " + String(ALERT_PHONE_NUMBER));
  Serial.println("Message: " + message);

  // Set text mode
  sendATCommand("AT+CMGF=1", 1000);

  // Set recipient number
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(ALERT_PHONE_NUMBER);
  gsmSerial.println("\"");
  delay(1000);

  // Send message body
  gsmSerial.print(message);
  delay(500);

  // Send Ctrl+Z (ASCII 26) to finalize SMS
  gsmSerial.write(26);
  delay(3000); // Wait for send confirmation

  Serial.println("SMS Sent!");
}

void sendATCommand(String command, int timeout) {
  gsmSerial.println(command);
  unsigned long startTime = millis();
  while (millis() - startTime < (unsigned long)timeout) {
    if (gsmSerial.available()) {
      Serial.write(gsmSerial.read()); // Echo GSM response to Serial Monitor
    }
  }
}
