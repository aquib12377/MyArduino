/* ──────────────────────────────────────────────────────────────
   Simple Lawn‑Mower Robot
   -------------------------------------------------------------
   • High‑speed cutter motor -> ACTIVE‑LOW relay on D5
   • HC‑SR04 distance sensor -> TRIG D6, ECHO D7
   • Drive motors (differential) -> L298N IN1‑IN4 on D8‑D11
        IN1 D8   IN2 D9   (left wheel)
        IN3 D10  IN4 D11  (right wheel)
   Behaviour
   ----------
   1. Default: cutter ON, robot drives forward.
   2. If an obstacle is detected closer than OBSTACLE_CM:
        • Stop, turn cutter OFF.
        • Try turning RIGHT for TURN_MS.
              • If path clear -> go forward, cutter ON.
              • else turn LEFT for 2×TURN_MS (now facing left of original).
                    • If clear -> go, cutter ON.
                    • else stop and wait (keeps cutter OFF).
   ------------------------------------------------------------- */

#define CUTTER_RELAY_PIN 5      // ACTIVE‑LOW relay
#define TRIG_PIN         6
#define ECHO_PIN         7
#define IN1              8
#define IN2              9
#define IN3             10
#define IN4             11

/* ───── Tunables ───── */
const unsigned long TURN_MS      = 450;   // motor run time for a ~90° turn
const byte          OBSTACLE_CM  = 25;    // obstacle threshold
const unsigned long MEASURE_DELAY= 50;    // pause between distance checks (ms)
/* ──────────────────── */

void setup() {
  pinMode(CUTTER_RELAY_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  Serial.begin(9600);
  cutterOn();          // start cutter immediately
  driveForward();      // start moving
}

void loop() {
  unsigned int dist = readDistanceCM();
  Serial.print("Distance: "); Serial.print(dist); Serial.println(" cm");

  if (dist > 0 && dist < OBSTACLE_CM) {           // obstacle!
    stopMotors();
    cutterOff();
    delay(150);                                   // short settle

    // ── attempt RIGHT ──
    turnRight();
    delay(TURN_MS);
    stopMotors();
    delay(150);
    dist = readDistanceCM();

    if (dist > OBSTACLE_CM || dist == 0) {        // right is clear
      driveForward();
      cutterOn();
      return;
    }

    // ── attempt LEFT (turn back across original heading) ──
    turnLeft();
    delay(TURN_MS * 2);                           // ~90° left of start
    stopMotors();
    delay(150);
    dist = readDistanceCM();

    if (dist > OBSTACLE_CM || dist == 0) {        // left is clear
      driveForward();
      cutterOn();
      return;
    }

    // Neither side clear – stay put, cutter off, wait until path clears
    while (dist != 0 && dist < OBSTACLE_CM) {
      delay(300);
      dist = readDistanceCM();
    }
    driveForward();
    cutterOn();
  }

  delay(MEASURE_DELAY);   // small delay to keep loop tidy
}

/* ────────────────── Helper functions ────────────────── */
unsigned int readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // 30 ms timeout (~5 m)
  if (duration == 0) return 0;                               // timeout/no echo
  return duration / 58;                                      // µs→cm
}

/* Cutter control (ACTIVE LOW) */
inline void cutterOn()  { digitalWrite(CUTTER_RELAY_PIN, LOW);  }
inline void cutterOff() { digitalWrite(CUTTER_RELAY_PIN, HIGH); }

/* Drive helpers */
inline void driveForward() {
  digitalWrite(IN1, HIGH);  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);  digitalWrite(IN4, LOW);
}
inline void driveBackward() {
  digitalWrite(IN1, LOW);   digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);   digitalWrite(IN4, HIGH);
}
inline void turnRight() {           // left wheel fwd, right wheel back
  digitalWrite(IN1, HIGH);  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);   digitalWrite(IN4, HIGH);
}
inline void turnLeft() {            // left wheel back, right wheel fwd
  digitalWrite(IN1, LOW);   digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);  digitalWrite(IN4, LOW);
}
inline void stopMotors() {
  digitalWrite(IN1, LOW);   digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);   digitalWrite(IN4, LOW);
}
