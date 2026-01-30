/*******************************************************
 * ESP32 2-Wheel Bot (BluetoothSerial)
 * L298N ENA/ENB -> 5V (no PWM)
 * Ultrasonic obstacle avoidance
 * Flame sensor triggers relay + emergency stop
 *******************************************************/

#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

// ------------------- CONFIG: PINS -------------------
// L298N direction pins (ENA/ENB tied to 5V)
const int IN1 = 26;   // Motor A
const int IN2 = 27;
const int IN3 = 14;   // Motor B
const int IN4 = 12;

// Ultrasonic (HC-SR04)
const int TRIG_PIN = 18;
const int ECHO_PIN = 19; // IMPORTANT: use divider if ECHO is 5V

// Flame sensor (Digital DO OR Analog AO)
const bool USE_FLAME_ANALOG = false; // false = DO pin, true = AO pin
const int FLAME_DO_PIN = 33;         // DO output (often LOW when flame detected)
const int FLAME_AO_PIN = 34;         // AO (ESP32 ADC input only)

// Relay
const int RELAY_PIN = 23;
const bool RELAY_ACTIVE_LOW = true;  // most relay modules: LOW=ON

// ------------------- BEHAVIOR -------------------
const int OBSTACLE_CM = 20;          // obstacle threshold
const int CLEAR_CM    = 28;          // clear hysteresis threshold

const unsigned long ULTRA_PERIOD_MS = 120;
const unsigned long FLAME_PERIOD_MS = 60;

const unsigned long AVOID_BACK_MS   = 350;
const unsigned long AVOID_TURN_MS   = 450;

const unsigned long FLAME_LATCH_MS  = 5000; // keep relay ON for 5s after flame last seen

// Analog flame threshold (0..4095). Tune if using analog.
const int FLAME_ANALOG_TRIGGER = 1700;

// ------------------- STATE -------------------
unsigned long lastUltraMs = 0;
unsigned long lastFlameMs = 0;

int lastDistanceCm = 999;
bool obstacle = false;

bool flameDetected = false;
unsigned long flameLastSeenMs = 0;

bool avoiding = false;
unsigned long avoidStepStartMs = 0;
int avoidStep = 0;

// Bluetooth command state
enum MoveCmd { CMD_STOP, CMD_FWD, CMD_BACK, CMD_LEFT, CMD_RIGHT };
MoveCmd cmd = CMD_STOP;

// ------------------- MOTOR HELPERS -------------------
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
void turnLeft() {
  // spin left: left motor back, right motor forward
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void turnRight() {
  // spin right: left motor forward, right motor back
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

// ------------------- RELAY -------------------
void relaySet(bool on) {
  if (RELAY_ACTIVE_LOW) digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  else                 digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

// ------------------- SENSORS -------------------
int readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 25000UL); // 25ms timeout
  if (duration == 0) return 999;
  return (int)(duration / 58UL);
}

bool readFlame() {
  if (!USE_FLAME_ANALOG) {
    int v = digitalRead(FLAME_DO_PIN);
    // Most modules: DO = LOW when flame detected (potentiometer adjustable).
    // If your module is opposite, change LOW to HIGH.
    return (v == LOW);
  } else {
    int a = analogRead(FLAME_AO_PIN); // 0..4095
    // Many modules: lower value when flame detected (depends). Tune as needed.
    return (a <= FLAME_ANALOG_TRIGGER);
  }
}

// ------------------- AVOIDANCE -------------------
void startAvoidSequence() {
  avoiding = true;
  avoidStep = 0;
  avoidStepStartMs = millis();
  stopMotors();
}

void runAvoidSequence() {
  unsigned long now = millis();

  if (avoidStep == 0) {
    backward();
    if (now - avoidStepStartMs >= AVOID_BACK_MS) {
      avoidStep = 1;
      avoidStepStartMs = now;
      stopMotors();
    }
    return;
  }

  if (avoidStep == 1) {
    // You can swap to turnLeft() if you want
    turnRight();
    if (now - avoidStepStartMs >= AVOID_TURN_MS) {
      stopMotors();
      avoiding = false;
      avoidStep = 0;
      avoidStepStartMs = now;
    }
    return;
  }
}

// ------------------- BLUETOOTH -------------------
void handleBtCommand(char c) {
  c = toupper((unsigned char)c);

  switch (c) {
    case 'F': cmd = CMD_FWD;   break;
    case 'B': cmd = CMD_BACK;  break;
    case 'L': cmd = CMD_LEFT;  break;
    case 'R': cmd = CMD_RIGHT; break;
    case 'S': cmd = CMD_STOP;  break;

    // optional: manual relay control
    case '1': relaySet(true);  break; // force ON
    case '0': relaySet(false); break; // force OFF

    default: break;
  }

  Serial.print("BT cmd: "); Serial.println(c);
}

// ------------------- SETUP/LOOP -------------------
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  if (!USE_FLAME_ANALOG) {
    pinMode(FLAME_DO_PIN, INPUT); // if needed: INPUT_PULLUP (depends on module)
  } else {
    pinMode(FLAME_AO_PIN, INPUT);
    analogReadResolution(12);
  }

  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false); // relay OFF at boot

  // Bluetooth
  const char* btName = "ESP32_BOT";
  bool ok = SerialBT.begin(btName);
  Serial.println(ok ? "Bluetooth started." : "Bluetooth start FAILED!");
  Serial.print("Pair with: "); Serial.println(btName);

  Serial.println("Commands: F,B,L,R,S (and optional relay: 1=ON,0=OFF)");
}

void loop() {
  unsigned long now = millis();

  // ---- Read Bluetooth ----
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    if (c == '\n' || c == '\r') continue;
    handleBtCommand(c);
  }

  // ---- Flame check ----
  if (now - lastFlameMs >= FLAME_PERIOD_MS) {
    lastFlameMs = now;

    bool f = readFlame();
    if (f) {
      flameDetected = true;
      flameLastSeenMs = now;
    } else {
      if (flameDetected && (now - flameLastSeenMs > FLAME_LATCH_MS)) {
        flameDetected = false;
      }
    }

    // Flame overrides relay (automatic)
    relaySet(flameDetected);
  }

  // ---- Ultrasonic check ----
  if (now - lastUltraMs >= ULTRA_PERIOD_MS) {
    lastUltraMs = now;

    int d = readDistanceCm();
    lastDistanceCm = d;

    if (!obstacle && d <= OBSTACLE_CM) obstacle = true;
    if (obstacle && d >= CLEAR_CM) obstacle = false;

    // Debug (optional)
    // Serial.printf("D=%d cm, obstacle=%d, flame=%d, cmd=%d\n", d, obstacle, flameDetected, cmd);
  }

  // ---- Safety: flame detected => STOP BOT ----
  if (flameDetected) {
    stopMotors();
    avoiding = false;
    return;
  }

  // ---- Obstacle logic: block forward and auto-avoid ----
  // If user asks forward and obstacle exists -> avoidance
  if (!avoiding && cmd == CMD_FWD && obstacle) {
    startAvoidSequence();
  }

  if (avoiding) {
    runAvoidSequence();
    return;
  }

  // ---- Execute Bluetooth command ----
  switch (cmd) {
    case CMD_FWD:
      // if obstacle, we already started avoiding above; else go forward
      if (!obstacle) forward();
      else stopMotors();
      break;

    case CMD_BACK:  backward();   break;
    case CMD_LEFT:  turnLeft();   break;
    case CMD_RIGHT: turnRight();  break;
    case CMD_STOP:  stopMotors(); break;
  }
}
