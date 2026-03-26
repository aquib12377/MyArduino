// ============================================================
//  BT_Motor_Control.ino
//  Control 2 DC Motors via L293D Motor Shield
//  Bluetooth Module : HC-05  (RX → Pin 10 | TX → Pin 9)
//
//  Works with BT apps that send WITHOUT \n or \r terminators.
//  Commands: "forward" "backward" "left" "right" "stop"
//            OR single letters: F B L R S
//            OR digits 0-9 for speed
// ============================================================

#include <SoftwareSerial.h>
#include <AFMotor.h>

// ── Bluetooth ─────────────────────────────────────────────────
SoftwareSerial BT(10, 9);    // RX=10 (← HC-05 TX), TX=9 (→ HC-05 RX)

// ── Motors ────────────────────────────────────────────────────
AF_DCMotor motorLeft(3);     // M3 on L293D shield
AF_DCMotor motorRight(4);    // M4 on L293D shield

// ── State ─────────────────────────────────────────────────────
int    motorSpeed  = 200;
String lastCommand = "stop";
String inputBuffer = "";

// How long (ms) to wait after last byte before forcing a parse
// Only used as fallback if nothing matched yet
#define IDLE_TIMEOUT 80

unsigned long lastByteTime = 0;

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  motorLeft.setSpeed(motorSpeed);
  motorRight.setSpeed(motorSpeed);
  motorLeft.run(RELEASE);
  motorRight.run(RELEASE);

  Serial.println("=== BT Motor Controller Ready ===");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  while (BT.available()) {
    char c = (char)BT.read();
    lastByteTime = millis();

    // Ignore newlines/returns if they do arrive
    if (c == '\n' || c == '\r') continue;

    inputBuffer += (char)tolower(c);

    Serial.print("Buffer: ");
    Serial.println(inputBuffer);

    // ── Try to match after every new character ──
    if (tryMatch()) {
      inputBuffer = "";   // Clear after a successful match
    }

    // Safety: clear if buffer gets too long
    if (inputBuffer.length() > 10) {
      inputBuffer = "";
    }
  }

  // ── Idle timeout fallback ─────────────────────────────────
  // If bytes stopped arriving and buffer still has something unmatched
  if (inputBuffer.length() > 0 &&
      (millis() - lastByteTime) > IDLE_TIMEOUT) {
    Serial.print("Timeout flush: ");
    Serial.println(inputBuffer);
    inputBuffer = "";
  }
}

// ─────────────────────────────────────────────────────────────
// tryMatch() — Check if buffer equals a known command RIGHT NOW
// Returns true if a command was found and executed
// ─────────────────────────────────────────────────────────────
bool tryMatch() {
  // Single-character commands
  if (inputBuffer.length() == 1) {
    char c = inputBuffer[0];
    if (c >= '0' && c <= '9') {
      motorSpeed = map(c - '0', 0, 9, 0, 255);
      motorLeft.setSpeed(motorSpeed);
      motorRight.setSpeed(motorSpeed);
      Serial.print("Speed: "); Serial.println(motorSpeed);
      BT.print("Speed: "); BT.println(motorSpeed);
      applyCommand(lastCommand);
      return true;
    }
    if (c == 'f') { applyCommand("forward");  lastCommand = "forward";  return true; }
    if (c == 'b') { applyCommand("backward"); lastCommand = "backward"; return true; }
    if (c == 'l') { applyCommand("left");     lastCommand = "left";     return true; }
    if (c == 'r') { applyCommand("right");    lastCommand = "right";    return true; }
    if (c == 's') { applyCommand("stop");     lastCommand = "stop";     return true; }
  }

  // Full-word commands — check as soon as buffer length matches
  if (inputBuffer == "forward")  { applyCommand("forward");  lastCommand = "forward";  return true; }
  if (inputBuffer == "backward") { applyCommand("backward"); lastCommand = "backward"; return true; }
  if (inputBuffer == "left")     { applyCommand("left");     lastCommand = "left";     return true; }
  if (inputBuffer == "right")    { applyCommand("right");    lastCommand = "right";    return true; }
  if (inputBuffer == "stop")     { applyCommand("stop");     lastCommand = "stop";     return true; }

  return false;   // No match yet — keep buffering
}

// ─────────────────────────────────────────────────────────────
// applyCommand() — Drive the motors
// ─────────────────────────────────────────────────────────────
void applyCommand(String cmd) {
  if (cmd == "forward") {
    motorLeft.setSpeed(motorSpeed);
    motorRight.setSpeed(motorSpeed);
    motorLeft.run(FORWARD);
    motorRight.run(FORWARD);
    Serial.println(">> FORWARD");
    BT.println(">> FORWARD");
  }
  else if (cmd == "backward") {
    motorLeft.setSpeed(motorSpeed);
    motorRight.setSpeed(motorSpeed);
    motorLeft.run(BACKWARD);
    motorRight.run(BACKWARD);
    Serial.println(">> BACKWARD");
    BT.println(">> BACKWARD");
  }
  else if (cmd == "left") {
    motorLeft.setSpeed(motorSpeed / 2);
    motorRight.setSpeed(motorSpeed);
    motorLeft.run(BACKWARD);
    motorRight.run(FORWARD);
    Serial.println(">> TURN LEFT");
    BT.println(">> TURN LEFT");
    motorLeft.setSpeed(motorSpeed);
  }
  else if (cmd == "right") {
    motorLeft.setSpeed(motorSpeed);
    motorRight.setSpeed(motorSpeed / 2);
    motorLeft.run(FORWARD);
    motorRight.run(BACKWARD);
    Serial.println(">> TURN RIGHT");
    BT.println(">> TURN RIGHT");
    motorRight.setSpeed(motorSpeed);
  }
  else {  // stop
    motorLeft.run(RELEASE);
    motorRight.run(RELEASE);
    Serial.println(">> STOP");
    BT.println(">> STOP");
  }
}
