// Arduino Nano + PCA9685 (Adafruit 16-Channel PWM) + Bluetooth (HC-05/HC-06)

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SoftwareSerial.h>

// =================== DEBUG CONFIG ===================
#define DEBUG 1                 // 1=enable logs, 0=mute
#define DEBUG_ECHO_CHARS 1      // 1=echo every received char from Serial/BT
// ====================================================

#if DEBUG
  #define DPRINT(x)        Serial.print(x)
  #define DPRINTLN(x)      Serial.println(x)
#else
  #define DPRINT(x)
  #define DPRINTLN(x)
#endif

// Simple timestamped logger
void log(const String& msg) {
#if DEBUG
  Serial.print('['); Serial.print(millis()); Serial.print("] ");
  Serial.println(msg);
#endif
}

//////////////////// USER TWEAKS ////////////////////
// Bluetooth pins (Nano): RX=D10, TX=D11
SoftwareSerial BT(10, 11);

// PCA9685 I2C address (default 0x40 unless A0..A5 changed)
constexpr uint8_t PCA_ADDR = 0x40;

// Servo pulse range (microseconds). Adjust if your servos need different endpoints.
constexpr int SERVO_MIN_US = 500;   // try 1000 if needed
constexpr int SERVO_MAX_US = 2500;  // try 2000 if needed

// Control only channels 0..3
constexpr uint8_t FIRST_CH = 0;
constexpr uint8_t LAST_CH  = 3;

// Smooth stepping config
constexpr uint8_t DEG_STEP = 5;           // 1 degree per step (as requested)
constexpr uint16_t STEP_DELAY_MS = 10;    // delay between degree steps

// Failsafe: if no command for this many ms, move to HOLD_ANGLE (0=disabled)
constexpr unsigned long FAILSAFE_MS = 0;
constexpr int HOLD_ANGLE = 90;
////////////////////////////////////////////////////

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA_ADDR);
unsigned long lastCmdMs = 0;

static const float SERVO_FREQ = 50.0f;  // Hz (standard)
float usPerBit = 0.0f;                  // microseconds per PCA step at 12-bit

// Track current angles so we can step smoothly
int currentAngle[16] = {HOLD_ANGLE};     // initialize all to HOLD_ANGLE

// For “no-newline” phone apps: commit after input idle
unsigned long lastCharMs = 0;
const unsigned long LINE_IDLE_MS = 120;  // parse after 120ms of silence

// --- helpers ---
uint16_t microsecondsToTicks(int us) {
  float cycleUs = 1e6f / SERVO_FREQ;      // e.g., 20000 us at 50 Hz
  float ticksPerUs = 4096.0f / cycleUs;   // ≈ 0.2048 ticks/us
  int ticks = (int)(us * ticksPerUs + 0.5f);
  if (ticks < 0) ticks = 0;
  if (ticks > 4095) ticks = 4095;
  return (uint16_t)ticks;
}

uint16_t angleToTicks(int angle) {
  int clamped = angle;
  if (clamped < 0) clamped = 0;
  if (clamped > 180) clamped = 180;
  int us = SERVO_MIN_US + (int)((SERVO_MAX_US - SERVO_MIN_US) * (clamped / 180.0f));
  uint16_t t = microsecondsToTicks(us);
  DPRINT("[angleToTicks] angle="); DPRINT(angle);
  DPRINT(" clamped="); DPRINT(clamped);
  DPRINT(" us="); DPRINT(us);
  DPRINT(" ticks="); DPRINTLN(t);
  return t;
}

void setServoAngle(uint8_t ch, int angle) {
  if (ch < FIRST_CH || ch > LAST_CH) {
    log("setServoAngle: IGNORE ch out of range: " + String(ch));
    return;
  }
  uint16_t t = angleToTicks(angle);
  pwm.setPWM(ch, 0, t);
  currentAngle[ch] = constrain(angle, 0, 180);
  log("setServoAngle: ch=" + String(ch) + " angle=" + String(angle) + " ticks=" + String(t));
}

void setAllServosImmediate(int angle) {  // instant jump (used at startup)
  log("setAllServosImmediate: angle=" + String(angle));
  for (uint8_t ch = FIRST_CH; ch <= LAST_CH; ++ch) {
    setServoAngle(ch, angle);
  }
}

// === NEW: smooth stepping ===
void stepServoTo(uint8_t ch, int target, uint16_t stepDelay = STEP_DELAY_MS) {
  if (ch < FIRST_CH || ch > LAST_CH) return;
  target = constrain(target, 0, 180);
  int cur = currentAngle[ch];
  log("stepServoTo: ch=" + String(ch) + " from=" + String(cur) + " to=" + String(target));

  if (cur == target) {
    setServoAngle(ch, target);  // ensure PWM matches
    return;
  }

  int dir = (target > cur) ? 1 : -1;
  while (cur != target) {
    cur += dir * DEG_STEP;
    if ((dir > 0 && cur > target) || (dir < 0 && cur < target)) cur = target;
    setServoAngle(ch, cur);
    delay(stepDelay);
  }
  log("stepServoTo: done ch=" + String(ch) + " final=" + String(currentAngle[ch]));
}

// Move ALL channels simultaneously, 1° per loop
void stepAllServosTo(int target, uint16_t stepDelay = STEP_DELAY_MS) {
  target = constrain(target, 0, 180);
  log("stepAllServosTo: target=" + String(target));
  bool anyMoving = true;
  while (anyMoving) {
    anyMoving = false;
    for (uint8_t ch = FIRST_CH; ch <= LAST_CH; ++ch) {
      int cur = currentAngle[ch];
      if (cur == target) continue;
      anyMoving = true;
      if (cur < target) cur += DEG_STEP; else cur -= DEG_STEP;
      // clamp overshoot
      if (cur < 0) cur = 0; if (cur > 180) cur = 180;
      // prevent step past target
      if ((currentAngle[ch] < target && cur > target) ||
          (currentAngle[ch] > target && cur < target)) {
        cur = target;
      }
      setServoAngle(ch, cur);
    }
    delay(stepDelay);
  }
  log("stepAllServosTo: done");
}

void reply(const String& s) {
  Serial.println(s);
  BT.println(s);
  DPRINTLN(String("reply -> ") + s);
}

// --- command parsing ---
String inBuf;

void handleLine(const String& lineRaw) {
  String line = lineRaw;
  String dbgLine = lineRaw;
  line.trim();
  line.toUpperCase();
  dbgLine.trim();

  if (line.length() == 0) return;

  lastCmdMs = millis();
  log("handleLine: raw='" + dbgLine + "' upper='" + line + "'");

  if (line.startsWith("S,")) {
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      String chStr = line.substring(c1 + 1, c2);
      String angStr = line.substring(c2 + 1);
      int ch = chStr.toInt();
      int ang = angStr.toInt();

      DPRINT("[parse] S cmd: chStr="); DPRINT(chStr);
      DPRINT(" angStr="); DPRINTLN(angStr);

      if (ch < (int)FIRST_CH || ch > (int)LAST_CH) {
        reply("ERR: channel out of range (0-3).");
        return;
      }
      if (ang < 0 || ang > 180) {
        reply("ERR: angle out of range (0-180).");
        return;
      }
      stepServoTo((uint8_t)ch, ang);                 // <<< smooth 1° stepping
      reply("OK: S," + String(ch) + "," + String(ang));
      return;
    }
    reply("ERR: format S,<ch>,<angle>");
    return;
  }

  if (line.startsWith("ALL,")) {
    int c1 = line.indexOf(',');
    if (c1 > 0) {
      int ang = line.substring(c1 + 1).toInt();
      DPRINT("[parse] ALL cmd: angStr="); DPRINTLN(line.substring(c1 + 1));
      if (ang < 0 || ang > 180) {
        reply("ERR: angle out of range (0-180).");
        return;
      }
      stepAllServosTo(ang);                           // <<< smooth 1° stepping for all
      reply("OK: ALL," + String(ang));
      return;
    }
    reply("ERR: format ALL,<angle>");
    return;
  }

  if (line == "TEST") {
    reply("TEST: sweeping 0->180->0 (smooth)");
    stepAllServosTo(0);
    delay(200);
    stepAllServosTo(180);
    delay(200);
    stepAllServosTo(0);
    reply("TEST: done");
    return;
  }

  reply("ERR: unknown command");
}

// read from both Serial and BT (with idle-commit)
void pumpSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
#if DEBUG && DEBUG_ECHO_CHARS
    Serial.print("[SERIAL IN] '"); Serial.print(c); Serial.println("'");
#endif
    if (c == '\r' || c == '\n') {
      if (inBuf.length()) { handleLine(inBuf); inBuf = ""; }
    } else {
      inBuf += c;
      lastCharMs = millis();
      if (inBuf.length() > 80) {
        log("pumpSerial: input buffer overflow -> cleared");
        inBuf = "";
      }
    }
  }
  while (BT.available()) {
    char c = (char)BT.read();
#if DEBUG && DEBUG_ECHO_CHARS
    Serial.print("[BT IN] '"); Serial.print(c); Serial.println("'");
#endif
    if (c == '\r' || c == '\n') {
      if (inBuf.length()) { handleLine(inBuf); inBuf = ""; }
    } else {
      inBuf += c;
      lastCharMs = millis();
      if (inBuf.length() > 80) {
        log("pumpSerial(BT): input buffer overflow -> cleared");
        inBuf = "";
      }
    }
  }

  // No newline? parse after brief silence
  if (inBuf.length() && (millis() - lastCharMs > LINE_IDLE_MS)) {
    log("idle-commit line: '" + inBuf + "'");
    handleLine(inBuf);
    inBuf = "";
  }
}

// Optional: scan I2C for devices at startup for quick sanity check
void i2cScan() {
  log("I2C scan start");
  byte count = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("  found device @ 0x");
      Serial.println(addr, HEX);
      count++;
    }
  }
  if (count == 0) log("I2C scan: no devices found");
  else log("I2C scan: total devices=" + String(count));
}

// --- setup/loop ---
void setup() {
  Serial.begin(9600);
  BT.begin(9600);
  delay(50);

  log("BOOT");
  log("Config: PCA=0x" + String(PCA_ADDR, HEX) +
      " freq=" + String(SERVO_FREQ) + "Hz range=" +
      String(SERVO_MIN_US) + "-" + String(SERVO_MAX_US) + "us");

  Wire.begin();
  i2cScan();

  log("Init PCA9685...");
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);
  delay(10);

  float cycleUs = 1e6f / SERVO_FREQ;
  usPerBit = cycleUs / 4096.0f;
  log("us/bit ≈ " + String(usPerBit, 6));

  // Initialize actual positions and state
  setAllServosImmediate(HOLD_ANGLE);              // physical center
  for (uint8_t ch = FIRST_CH; ch <= LAST_CH; ++ch) currentAngle[ch] = HOLD_ANGLE;

  reply("READY: PCA9685 @0x" + String(PCA_ADDR, HEX) +
        ", 50Hz, range " + String(SERVO_MIN_US) + "-" + String(SERVO_MAX_US) + "us, " +
        "us/bit≈" + String(usPerBit, 4));
  reply("CMDS: S,<ch>,<angle> | ALL,<angle> | TEST");
}

void loop() {
  BT.listen();     // ensure BT is the active SoftwareSerial
  pumpSerial();

  if (FAILSAFE_MS > 0) {
    unsigned long now = millis();
    if (now - lastCmdMs > FAILSAFE_MS) {
      stepAllServosTo(HOLD_ANGLE);
      lastCmdMs = now;
      reply("FAILSAFE: ALL set to " + String(HOLD_ANGLE));
    }
  }
}
