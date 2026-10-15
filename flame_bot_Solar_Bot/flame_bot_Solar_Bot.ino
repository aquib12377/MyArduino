/*
 * ============================================================
 *  Fire-Fighting 2-Wheel Bot — Arduino Nano  (v2.1 — verbose)
 * ============================================================
 *
 *  Same behavior as v2. Only addition: detailed Serial-only
 *  diagnostics (USB @ 115200 baud). BT stays light.
 *
 *  HARDWARE:
 *    2× Johnson Motors via L298N motor driver
 *    3× Flame Sensors  (Digital, Active-LOW output)
 *    1× DHT11          (Temperature & Humidity)
 *    1× SG90 Servo     (HC-SR04 pan mount)
 *    1× HC-SR04        (Obstacle detection)
 *    1× Active-LOW Relay (Water pump)
 *    1× HC-05          (Bluetooth – SoftwareSerial)
 *
 * ── PIN MAP ──────────────────────────────────────────────────
 *    IN1 → D4   IN2 → D5   IN3 → D6   IN4 → D7
 *    FLAME_L → A2   FLAME_C → A1   FLAME_R → A0
 *    DHT11 → A5     SG90 → D9
 *    HC-SR04 → TRIG D2 / ECHO D3
 *    Relay → D13 (Active-LOW)
 *    HC-05 → SoftwareSerial(10, 11)  RX=D10  TX=D11
 *
 * ── SERIAL LOG TAGS ──────────────────────────────────────────
 *    [BOOT]    one-shot startup info
 *    [INIT]    success/failure of each init step
 *    [CMD]     bluetooth byte received + parsed action
 *    [STATE]   mode / pump / drive state transitions
 *    [SENSOR]  periodic sensor values (throttled)
 *    [LOOP]    per-loop decision tree in auto/manual
 *    [INFO]    passive info
 *    [WARN]    non-fatal issues
 *    [ERROR]   fatal / near-fatal
 * ============================================================
 */

#include <Servo.h>
#include <DHT.h>
#include <SoftwareSerial.h>

// ── Bluetooth (SoftwareSerial) ────────────────────────────────
SoftwareSerial BT(10, 11);   // RX = D10, TX = D11

// ── L298N Motor Pins ──────────────────────────────────────────
constexpr uint8_t IN1 = 4;
constexpr uint8_t IN2 = 5;
constexpr uint8_t IN3 = 6;
constexpr uint8_t IN4 = 7;

// ── Flame Sensor Pins (Active-LOW digital output) ─────────────
constexpr uint8_t FLAME_L = A2;
constexpr uint8_t FLAME_C = A1;
constexpr uint8_t FLAME_R = A0;

// ── DHT11 ─────────────────────────────────────────────────────
#define DHT_PIN   A5
#define DHT_TYPE  DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ── SG90 Servo ────────────────────────────────────────────────
constexpr uint8_t SERVO_PIN = 9;
Servo scanServo;

// ── HC-SR04 ───────────────────────────────────────────────────
constexpr uint8_t TRIG_PIN = 2;
constexpr uint8_t ECHO_PIN = 3;

// ── Relay (Active-LOW) ────────────────────────────────────────
constexpr uint8_t RELAY_PIN = 13;
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// ── Tuning Parameters ─────────────────────────────────────────
constexpr uint8_t  DEFAULT_SPEED     = 255;
constexpr uint16_t OBSTACLE_CM       = 20;
constexpr uint16_t DHT_INTERVAL_MS   = 2000;
constexpr float    TEMP_ALARM_C      = 50.0;
constexpr uint16_t BT_LOG_THROTTLE_MS     = 500;   // BT slow
constexpr uint16_t SENSOR_DUMP_MS         = 200;   // Serial verbose throttle
constexpr uint16_t LOOP_TRACE_MS          = 200;   // Serial loop-trace throttle

// ── Runtime State ─────────────────────────────────────────────
uint8_t motorSpeed  = DEFAULT_SPEED;
bool    autoMode    = false;
bool    pumpActive  = false;
bool    flameLatch  = false;

unsigned long lastDHTTime    = 0;
unsigned long lastBtLogTime  = 0;
unsigned long lastSensorDump = 0;
unsigned long lastLoopTrace  = 0;
unsigned long loopCounter    = 0;

float lastTemp = 0.0f;
float lastHum  = 0.0f;
long  lastDist = -1;

enum class DriveState : uint8_t { STOP, FWD, BWD, LEFT, RIGHT };
DriveState lastDrive = DriveState::STOP;

const __FlashStringHelper* driveName(DriveState d) {
  switch (d) {
    case DriveState::STOP:  return F("STOP");
    case DriveState::FWD:   return F("FWD");
    case DriveState::BWD:   return F("BWD");
    case DriveState::LEFT:  return F("LEFT");
    case DriveState::RIGHT: return F("RIGHT");
  }
  return F("?");
}

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  BT.begin(9600);

  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("[BOOT] Flame Bot v2.1 — verbose diagnostics"));
  Serial.println(F("[BOOT] Board: Arduino Nano (ATmega328P)"));
  Serial.println(F("[BOOT] USB Serial @ 115200   BT @ 9600"));
  Serial.println(F("============================================"));

  Serial.print(F("[INIT] Motor pins D")); Serial.print(IN1);
  Serial.print(F(",D")); Serial.print(IN2);
  Serial.print(F(",D")); Serial.print(IN3);
  Serial.print(F(",D")); Serial.println(IN4);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  Serial.print(F("[INIT] Flame sensors A"));
  Serial.print(FLAME_L - A0); Serial.print(F(" (L), A"));
  Serial.print(FLAME_C - A0); Serial.print(F(" (C), A"));
  Serial.print(FLAME_R - A0); Serial.println(F(" (R)"));
  pinMode(FLAME_L, INPUT);
  pinMode(FLAME_C, INPUT);
  pinMode(FLAME_R, INPUT);

  Serial.print(F("[INIT] HC-SR04 TRIG=D")); Serial.print(TRIG_PIN);
  Serial.print(F(" ECHO=D")); Serial.println(ECHO_PIN);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.print(F("[INIT] Relay D")); Serial.print(RELAY_PIN);
  Serial.println(F(" (active-LOW) — OFF"));
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Serial.print(F("[INIT] Servo D")); Serial.print(SERVO_PIN);
  Serial.println(F(" → center 90°"));
  scanServo.attach(SERVO_PIN);
  scanServo.write(90);
  delay(400);

  Serial.println(F("[INIT] DHT11 begin"));
  dht.begin();

  stopMotors();
  Serial.println(F("[INIT] Motors stopped"));

  Serial.print(F("[INIT] Thresholds: OBSTACLE=")); Serial.print(OBSTACLE_CM);
  Serial.print(F("cm  TEMP_ALARM=")); Serial.print(TEMP_ALARM_C, 0);
  Serial.println(F("C"));

  Serial.println(F("[BOOT] Ready. Default mode: MANUAL. Pump: OFF."));
  Serial.println(F("[BOOT] Commands: A/M  F/B/L/R/S  P/p  0-9"));
  Serial.println();

  BT.println(F("[BOOT] Flame Bot v2.1 ready"));
  BT.println(F("A=Auto  M=Manual  F/B/L/R/S  P/p=Pump  0-9=Speed"));
}

// =============================================================
//  MAIN LOOP  (lightweight scheduler)
// =============================================================
void loop() {
  loopCounter++;

  handleBluetooth();
  pollDHT();
  dumpSensorsThrottled();

  if (autoMode) {
    autoFlameMode();
  } else {
    manualSafety();
  }
}

// =============================================================
//  BLUETOOTH COMMAND HANDLER
// =============================================================
void handleBluetooth() {
  if (!BT.available()) return;
  char cmd = (char)BT.read();

  Serial.print(F("[CMD] RX byte='"));
  Serial.print(cmd);
  Serial.print(F("' (0x"));
  Serial.print((uint8_t)cmd, HEX);
  Serial.println(F(")"));

  // Speed control: '0' (stop) to '9' (full)
  if (cmd >= '0' && cmd <= '9') {
    motorSpeed = map(cmd - '0', 0, 9, 0, 255);
    Serial.print(F("[CMD] Speed digit=")); Serial.print(cmd);
    Serial.print(F(" → PWM=")); Serial.println(motorSpeed);
    BT.print(F("[INFO] Speed: ")); BT.println(motorSpeed);
    return;
  }

  switch (cmd) {
    case 'F':
      if (!autoMode) {
        if (obstacleAhead()) {
          Serial.println(F("[CMD] F rejected — obstacle ahead"));
          stopMotors();
          BT.println(F("[WARN] FWD blocked — obstacle"));
        } else {
          Serial.println(F("[CMD] F → moveForward()"));
          moveForward();
          BT.println(F("[STATE] FWD"));
        }
      } else {
        Serial.println(F("[CMD] F ignored — AUTO mode"));
      }
      break;

    case 'B':
      if (!autoMode) {
        Serial.println(F("[CMD] B → moveBackward()"));
        moveBackward();
        BT.println(F("[STATE] BWD"));
      } else {
        Serial.println(F("[CMD] B ignored — AUTO mode"));
      }
      break;

    case 'L':
      if (!autoMode) {
        Serial.println(F("[CMD] L → turnLeft()"));
        turnLeft();
        BT.println(F("[STATE] LEFT"));
      } else {
        Serial.println(F("[CMD] L ignored — AUTO mode"));
      }
      break;

    case 'R':
      if (!autoMode) {
        Serial.println(F("[CMD] R → turnRight()"));
        turnRight();
        BT.println(F("[STATE] RIGHT"));
      } else {
        Serial.println(F("[CMD] R ignored — AUTO mode"));
      }
      break;

    case 'S':
      if (!autoMode) {
        Serial.println(F("[CMD] S → stopMotors()"));
        stopMotors();
        BT.println(F("[STATE] STOP"));
      } else {
        Serial.println(F("[CMD] S ignored — AUTO mode"));
      }
      break;

    case 'P':
      if (!autoMode) {
        Serial.println(F("[CMD] P → pump ON (manual)"));
        setPump(true);
        BT.println(F("[STATE] Pump ON (manual)"));
      } else {
        Serial.println(F("[CMD] P ignored — AUTO mode"));
        BT.println(F("[WARN] Pump cmd ignored in AUTO"));
      }
      break;

    case 'p':
      if (!autoMode) {
        if (flameLatch) {
          Serial.println(F("[CMD] p rejected — pump latched by flame"));
          BT.println(F("[WARN] Pump locked — flame present"));
        } else {
          Serial.println(F("[CMD] p → pump OFF (manual)"));
          setPump(false);
          BT.println(F("[STATE] Pump OFF (manual)"));
        }
      } else {
        Serial.println(F("[CMD] p ignored — AUTO mode"));
        BT.println(F("[WARN] Pump cmd ignored in AUTO"));
      }
      break;

    case 'A':
      Serial.println(F("[CMD] A → switching to AUTO"));
      autoMode = true;
      flameLatch = false;
      stopMotors();
      setPump(false);
      Serial.println(F("[STATE] Mode=AUTO  motors=STOP  pump=OFF  latch=cleared"));
      BT.println(F("[STATE] >> AUTO"));
      break;

    case 'M':
      Serial.println(F("[CMD] M → switching to MANUAL"));
      autoMode = false;
      flameLatch = false;
      stopMotors();
      setPump(false);
      Serial.println(F("[STATE] Mode=MANUAL  motors=STOP  pump=OFF  latch=cleared"));
      BT.println(F("[STATE] >> MANUAL"));
      break;

    default:
      Serial.print(F("[WARN] Unknown command: '"));
      Serial.print(cmd);
      Serial.println(F("'"));
      break;
  }
}

// =============================================================
//  DHT11 POLLING (non-blocking, every 2 s)
// =============================================================
void pollDHT() {
  if (millis() - lastDHTTime < DHT_INTERVAL_MS) return;
  lastDHTTime = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println(F("[WARN] DHT read failed — NaN"));
    BT.println(F("[WARN] DHT read failed"));
    return;
  }

  lastTemp = t;
  lastHum  = h;

  Serial.print(F("[SENSOR] DHT T=")); Serial.print(t, 1);
  Serial.print(F("C  H=")); Serial.print(h, 1); Serial.println(F("%"));

  BT.print(F("[INFO] T=")); BT.print(t, 1);
  BT.print(F("C H="));      BT.print(h, 1);
  BT.println(F("%"));

  if (t >= TEMP_ALARM_C && !pumpActive) {
    Serial.print(F("[WARN] TEMP_ALARM triggered (T="));
    Serial.print(t, 1); Serial.println(F("C) → Pump ON"));
    setPump(true);
    BT.println(F("[WARN] TEMP ALARM — Pump ON"));
  }
}

// =============================================================
//  VERBOSE SENSOR DUMP (every SENSOR_DUMP_MS)
// =============================================================
//  Prints raw flame sensor states, distance, pump state,
//  drive state, mode, latch — a full snapshot for debugging.
// =============================================================
void dumpSensorsThrottled() {
  if (millis() - lastSensorDump < SENSOR_DUMP_MS) return;
  lastSensorDump = millis();

  bool fl = (digitalRead(FLAME_L) == LOW);
  bool fc = (digitalRead(FLAME_C) == LOW);
  bool fr = (digitalRead(FLAME_R) == LOW);
  long d  = measureDistance();
  lastDist = d;

  Serial.print(F("[SENSOR] t="));   Serial.print(millis());
  Serial.print(F("ms  loops="));    Serial.print(loopCounter);
  Serial.print(F("  FL=")); Serial.print(fl);
  Serial.print(F(" FC=")); Serial.print(fc);
  Serial.print(F(" FR=")); Serial.print(fr);
  Serial.print(F("  dist=")); Serial.print(d); Serial.print(F("cm"));
  Serial.print(F("  mode=")); Serial.print(autoMode ? F("AUTO") : F("MAN"));
  Serial.print(F("  drive=")); Serial.print(driveName(lastDrive));
  Serial.print(F("  pump=")); Serial.print(pumpActive ? F("ON") : F("OFF"));
  Serial.print(F("  latch=")); Serial.println(flameLatch ? F("Y") : F("N"));
}

// =============================================================
//  AUTO FLAME-SEEK MODE
// =============================================================
void autoFlameMode() {
  bool fl = (digitalRead(FLAME_L) == LOW);
  bool fc = (digitalRead(FLAME_C) == LOW);
  bool fr = (digitalRead(FLAME_R) == LOW);

  bool traceThisLoop = (millis() - lastLoopTrace >= LOOP_TRACE_MS);
  if (traceThisLoop) {
    lastLoopTrace = millis();
    Serial.print(F("[LOOP] AUTO decision: FL=")); Serial.print(fl);
    Serial.print(F(" FC=")); Serial.print(fc);
    Serial.print(F(" FR=")); Serial.println(fr);
  }

  // No flame → idle
  if (!fl && !fc && !fr) {
    if (traceThisLoop) Serial.println(F("[LOOP] AUTO → no flame → idle"));
    stopMotors();
    setPump(false);
    btThrottledLog(F("[STATE] AUTO idle"));
    return;
  }

  // Flame present → check obstacle
  const bool blocked = obstacleAhead();
  if (traceThisLoop) {
    Serial.print(F("[LOOP] AUTO obstacle check: dist="));
    Serial.print(lastDist); Serial.print(F("cm blocked="));
    Serial.println(blocked ? F("Y") : F("N"));
  }

  if (fc) {
    setPump(true);
    if (blocked) {
      if (traceThisLoop) Serial.println(F("[LOOP] AUTO → FC + blocked → STOP + PUMP"));
      stopMotors();
      btThrottledLog(F("[STATE] FC+obs → STOP+PUMP"));
    } else {
      if (traceThisLoop) Serial.println(F("[LOOP] AUTO → FC → FWD + PUMP"));
      moveForward();
      btThrottledLog(F("[STATE] FC → FWD+PUMP"));
    }
    return;
  }

  // No centre flame → pump OFF while re-aiming
  setPump(false);

  if (blocked) {
    if (traceThisLoop) Serial.println(F("[LOOP] AUTO → side flame + blocked → STOP"));
    stopMotors();
    btThrottledLog(F("[STATE] side+obs → STOP"));
    return;
  }

  if (fl && !fr) {
    if (traceThisLoop) Serial.println(F("[LOOP] AUTO → FL only → turn LEFT"));
    turnLeft();
    btThrottledLog(F("[STATE] FL → LEFT"));
  } else if (fr && !fl) {
    if (traceThisLoop) Serial.println(F("[LOOP] AUTO → FR only → turn RIGHT"));
    turnRight();
    btThrottledLog(F("[STATE] FR → RIGHT"));
  } else {
    if (traceThisLoop) Serial.println(F("[LOOP] AUTO → FL+FR (no FC) → STOP"));
    stopMotors();
    btThrottledLog(F("[STATE] FL+FR → STOP"));
  }
}

// =============================================================
//  MANUAL MODE SAFETY LAYER
// =============================================================
void manualSafety() {
  bool traceThisLoop = (millis() - lastLoopTrace >= LOOP_TRACE_MS);
  if (traceThisLoop) {
    lastLoopTrace = millis();
  }

  // Obstacle protection: only cancel forward motion
  if (lastDrive == DriveState::FWD && obstacleAhead()) {
    Serial.print(F("[WARN] MANUAL FWD auto-stopped — obstacle at "));
    Serial.print(lastDist); Serial.println(F("cm"));
    stopMotors();
    btThrottledLog(F("[WARN] FWD stopped — obstacle"));
  }

  // Flame auto-pump latch
  bool fl = (digitalRead(FLAME_L) == LOW);
  bool fc = (digitalRead(FLAME_C) == LOW);
  bool fr = (digitalRead(FLAME_R) == LOW);
  bool anyFlame = fl || fc || fr;

  if (traceThisLoop) {
    Serial.print(F("[LOOP] MANUAL: FL=")); Serial.print(fl);
    Serial.print(F(" FC="));  Serial.print(fc);
    Serial.print(F(" FR="));  Serial.print(fr);
    Serial.print(F(" any=")); Serial.print(anyFlame);
    Serial.print(F(" latch=")); Serial.println(flameLatch);
  }

  if (anyFlame) {
    if (!flameLatch) {
      flameLatch = true;
      Serial.print(F("[STATE] Flame detected (FL=")); Serial.print(fl);
      Serial.print(F(" FC=")); Serial.print(fc);
      Serial.print(F(" FR=")); Serial.print(fr);
      Serial.println(F(") → Pump auto-ON, latched"));
      setPump(true);
      BT.println(F("[WARN] Flame — Pump auto-ON"));
    }
  } else if (flameLatch) {
    Serial.println(F("[STATE] All flames cleared → latch released, pump OFF"));
    flameLatch = false;
    setPump(false);
    BT.println(F("[INFO] Flame cleared"));
  }
}

// =============================================================
//  OBSTACLE CHECK
// =============================================================
bool obstacleAhead() {
  long d = measureDistance();
  lastDist = d;
  bool blocked = (d > 0 && d < OBSTACLE_CM);
  return blocked;
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long dur = pulseIn(ECHO_PIN, HIGH, 25000UL);
  return (dur == 0) ? 999L : (dur / 58L);
}

// =============================================================
//  PUMP CONTROL (Active-LOW relay)
// =============================================================
void setPump(bool on) {
  if (pumpActive == on) return;
  pumpActive = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
  Serial.print(F("[STATE] Relay → "));
  Serial.print(on ? F("ON (pin LOW)") : F("OFF (pin HIGH)"));
  Serial.print(F("  pumpActive="));
  Serial.println(on ? F("true") : F("false"));
}

// =============================================================
//  MOTOR PRIMITIVES
// =============================================================
void setMotorA(int8_t dir) {
  digitalWrite(IN1, dir > 0 ? HIGH : LOW);
  digitalWrite(IN2, dir < 0 ? HIGH : LOW);
}
void setMotorB(int8_t dir) {
  digitalWrite(IN3, dir > 0 ? HIGH : LOW);
  digitalWrite(IN4, dir < 0 ? HIGH : LOW);
}

static void logDrive(DriveState from, DriveState to) {
  if (from == to) return;
  Serial.print(F("[STATE] Drive "));
  Serial.print(driveName(from));
  Serial.print(F(" → "));
  Serial.println(driveName(to));
}

void moveForward()  {
  DriveState prev = lastDrive;
  setMotorA( 1); setMotorB( 1);
  lastDrive = DriveState::FWD;
  logDrive(prev, lastDrive);
}
void moveBackward() {
  DriveState prev = lastDrive;
  setMotorA(-1); setMotorB(-1);
  lastDrive = DriveState::BWD;
  logDrive(prev, lastDrive);
}
void turnLeft() {
  DriveState prev = lastDrive;
  setMotorA(-1); setMotorB( 1);
  lastDrive = DriveState::LEFT;
  logDrive(prev, lastDrive);
}
void turnRight() {
  DriveState prev = lastDrive;
  setMotorA( 1); setMotorB(-1);
  lastDrive = DriveState::RIGHT;
  logDrive(prev, lastDrive);
}
void stopMotors() {
  DriveState prev = lastDrive;
  setMotorA( 0); setMotorB( 0);
  lastDrive = DriveState::STOP;
  logDrive(prev, lastDrive);
}

// =============================================================
//  BT LOG THROTTLE — keep phone app snappy
// =============================================================
void btThrottledLog(const __FlashStringHelper* msg) {
  if (millis() - lastBtLogTime < BT_LOG_THROTTLE_MS) return;
  lastBtLogTime = millis();
  BT.println(msg);
}