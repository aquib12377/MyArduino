#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ESP32Servo.h>

BluetoothSerial SerialBT;
const char* BT_NAME = "ESP32_BOT";

// ========== L298N Pins (ENA/ENB tied to 5V) ==========
static const int IN1 = 33;
static const int IN2 = 26;
static const int IN3 = 25;
static const int IN4 = 27;

// ========== ESC / BLDC ==========
static const int ESC_PIN = 13;
Servo esc;

// Typical airplane ESC range
static const int ESC_MIN_US = 1000;
static const int ESC_MAX_US = 2000;

static const int FIXED_ESC_PERCENT = 100;   // <<< fixed 60%
static bool escArmed = false;
static bool escEnabled = true;             // allow EON/EOFF (optional)

// ========== Failsafe ==========
static unsigned long lastCmdAt = 0;
static const unsigned long FAILSAFE_MS = 10000;

// RX buffer
String line;
static unsigned long lastRxAt = 0;
static const unsigned long RX_IDLE_COMMIT_MS = 200;

// ---------- Helpers ----------
int clampi(int v, int lo, int hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }

void logBoth(const String& s) {
  Serial.println(s);
  SerialBT.println(s);
}

// ---------- Motors ----------
void setMotorA(int dir) {
  if (dir > 0)      { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  }
  else if (dir < 0) { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); }
  else              { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  }
}
void setMotorB(int dir) {
  if (dir > 0)      { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  }
  else if (dir < 0) { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
  else              { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  }
}
void stopDrive()     { setMotorA(0);  setMotorB(0); }
void driveForward()  { setMotorA(+1); setMotorB(+1); }
void driveBackward() { setMotorA(-1); setMotorB(-1); }
void pivotLeft()     { setMotorA(-1); setMotorB(+1); }
void pivotRight()    { setMotorA(+1); setMotorB(-1); }

// ---------- ESC ----------
int percentToUs(int pct) {
  pct = clampi(pct, 0, 100);
  return ESC_MIN_US + (ESC_MAX_US - ESC_MIN_US) * pct / 100;
}

void escWritePercent(int pct) {
  pct = clampi(pct, 0, 100);
  int us = percentToUs(pct);
  esc.writeMicroseconds(us);
  // debug (optional)
  // logBoth("ESC " + String(pct) + "% (" + String(us) + "us)");
}

void escStop() {
  escWritePercent(0);
}

void escRunFixed() {
  if (!escEnabled) return;
  if (!escArmed) return;
  escWritePercent(FIXED_ESC_PERCENT);
}

void armESC() {
  logBoth("Arming ESC: sending MIN throttle...");
  esc.writeMicroseconds(ESC_MIN_US);
  delay(2500);                 // many ESCs need 2s+
  escArmed = true;
  logBoth("ESC armed.");
  escStop();
}

// ---------- Command handler ----------
void handleCommand(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;

  lastCmdAt = millis();

  String up = cmd;
  up.toUpperCase();

  logBoth("RX: " + up);

  // Optional ESC enable/disable
  if (up == "EON")  { escEnabled = true;  logBoth("ESC enabled");  return; }
  if (up == "EOFF") { escEnabled = false; escStop(); logBoth("ESC disabled"); return; }

  if (up == "S") {
    stopDrive();

    escStop();
    return;
  }

  if (up == "F") { driveForward();  //escRunFixed(); 
  return; }
  if (up == "B") { driveBackward(); //escRunFixed(); 
  return; }
  if (up == "L") { pivotLeft();     //escRunFixed(); 
  return; }
  if (up == "R") { pivotRight();    //escRunFixed(); 
  return; }

  if (up == "O") {     escRunFixed(); 
  return; }

  logBoth("Unknown cmd. Use: F B L R S (optional: EON/EOFF)");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopDrive();

  // ESC setup
  esc.setPeriodHertz(50);
  esc.attach(ESC_PIN, 500, 2500);
  escStop();

  // Bluetooth
  bool ok = SerialBT.begin(BT_NAME);
  Serial.println("Boot OK");
  Serial.println(String("Bluetooth begin: ") + (ok ? "OK" : "FAILED"));
  SerialBT.println("Ready. Commands: F B L R S (ESC fixed 60%)");

  // Arm ESC on boot
  armESC();

  lastCmdAt = millis();
}

void loop() {
  // Read BT data
  while (SerialBT.available()) {
    char ch = (char)SerialBT.read();
    lastRxAt = millis();

    if (ch == '\n' || ch == '\r') {
      if (line.length()) {
        handleCommand(line);
        line = "";
      }
      continue;
    }

    // Immediate single-letter commands
    if (ch=='F'||ch=='f'||ch=='B'||ch=='b'||ch=='L'||ch=='l'||ch=='R'||ch=='r'||ch=='S'||ch=='s') {
      String one; one += ch;
      handleCommand(one);
      line = "";
      continue;
    }

    if (line.length() < 60) line += ch;
  }

  // Commit buffered command if no newline was sent
  if (line.length() && (millis() - lastRxAt) > RX_IDLE_COMMIT_MS) {
    handleCommand(line);
    line = "";
  }

  // Failsafe: stop everything
  if (millis() - lastCmdAt > FAILSAFE_MS) {
    stopDrive();
    escStop();
  }
}
