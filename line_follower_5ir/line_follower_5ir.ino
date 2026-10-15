/*
  5-Sensor IR Line Follower (PD control) - Arduino Uno + L298N
  Sensors  : S1..S5 (left -> right) on A0..A4
  Handles  : digital OR analog sensor modules, black-on-white OR white-on-black,
             lost line (search), intersection / all-sensors-on, finish stop.
  Tune     : see CONFIG section. Open Serial Monitor @115200 for debug.
*/

// ======================= CONFIG =======================
#define USE_ANALOG        1     // 0 = digital module output (DO), 1 = analog output (AO)
#define LINE_READS_HIGH   0     // 1 = sensor reads HIGH (or above threshold) over the LINE
                                // 0 = reads LOW over the line. Test with DEBUG_SERIAL and flip if inverted.
#define DEBUG_SERIAL      1

const int  BASE_SPEED     = 150;   // 0-255 straight speed (start low: 100-130, then raise)
const int  MAX_SPEED      = 230;   // clamp
const int  SEARCH_SPEED   = 120;   // spin speed when line is lost
const float KP            = 0.060; // error range is -2000..2000
const float KD            = 0.100;
const unsigned long LOST_TIMEOUT_MS   = 3000;  // stop if line not found within this time
const unsigned long FINISH_HOLD_MS    = 300;   // all sensors ON for this long = finish box -> stop
const bool STOP_ON_ALL_ON = true;              // false = just drive straight over crossings/wide bars
   
const bool LEFT_MOTOR_INVERT  = false;   // flip if left wheel spins backwards
const bool RIGHT_MOTOR_INVERT = false;   // flip if right wheel spins backwards
// ======================================================

// Pins
const uint8_t SENSOR_PIN[5] = {A0, A1, A2, A3, A4};   // S1 (far left) ... S5 (far right)
const uint8_t ENA = 3;   // left motor PWM
const uint8_t IN1 = 4;
const uint8_t IN2 = 5;
const uint8_t IN3 = 7;
const uint8_t IN4 = 8;
const uint8_t ENB = 6;  // right motor PWM

const int WEIGHT[5] = {-2000, -1000, 0, 1000, 2000};

int  threshold[5]  = {512, 512, 512, 512, 512};
bool onLine[5];
int  lastError     = 0;
int  lastPosition  = 0;
unsigned long lostSince = 0;
unsigned long allOnSince = 0;

// ---------- motors ----------
void driveMotor(uint8_t inA, uint8_t inB, uint8_t en, int speed, bool invert) {
  if (invert) speed = -speed;
  speed = constrain(speed, -255, 255);
  if (speed >= 0) { digitalWrite(inA, HIGH); digitalWrite(inB, LOW); }
  else            { digitalWrite(inA, LOW);  digitalWrite(inB, HIGH); }
  analogWrite(en, abs(speed));
}

void setMotors(int left, int right) {
  driveMotor(IN1, IN2, ENA, left,  LEFT_MOTOR_INVERT);
  driveMotor(IN3, IN4, ENB, right, RIGHT_MOTOR_INVERT);
}

void stopMotors() { setMotors(0, 0); }

// ---------- sensors ----------
#if USE_ANALOG
void calibrate() {
  // Slide the robot left-right over the line and background for ~4 s
  int mn[5], mx[5];
  for (int i = 0; i < 5; i++) { mn[i] = 1023; mx[i] = 0; }
  unsigned long t = millis();
  while (millis() - t < 4000) {
    for (int i = 0; i < 5; i++) {
      int v = analogRead(SENSOR_PIN[i]);
      if (v < mn[i]) mn[i] = v;
      if (v > mx[i]) mx[i] = v;
    }
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
  }
  for (int i = 0; i < 5; i++) {
    threshold[i] = (mn[i] + mx[i]) / 2;
#if DEBUG_SERIAL
    Serial.print(F("S")); Serial.print(i + 1);
    Serial.print(F(" min=")); Serial.print(mn[i]);
    Serial.print(F(" max=")); Serial.print(mx[i]);
    Serial.print(F(" thr=")); Serial.println(threshold[i]);
    if (mx[i] - mn[i] < 100) Serial.println(F("  WARNING: weak contrast, check height/wiring"));
#endif
  }
  digitalWrite(LED_BUILTIN, LOW);
}
#endif

int readSensors() {            // returns number of sensors on the line
  int count = 0;
  for (int i = 0; i < 5; i++) {
#if USE_ANALOG
    bool above = analogRead(SENSOR_PIN[i]) > threshold[i];
    onLine[i] = LINE_READS_HIGH ? above : !above;
#else
    bool high = digitalRead(SENSOR_PIN[i]);
    onLine[i] = LINE_READS_HIGH ? high : !high;
#endif
    if (onLine[i]) count++;
  }
  return count;
}

// ---------- setup ----------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 5; i++) pinMode(SENSOR_PIN[i], INPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();
#if DEBUG_SERIAL
  Serial.begin(115200);
#endif
#if USE_ANALOG
  calibrate();
#endif
  delay(1000);   // time to place the robot on the line
}

// ---------- main loop ----------
void loop() {
  int count = readSensors();

  // Case 1: line lost -> spin toward the side where it was last seen
  if (count == 0) {
    if (lostSince == 0) lostSince = millis();
    if (millis() - lostSince > LOST_TIMEOUT_MS) { stopMotors(); return; }
    if (lastPosition < 0) setMotors(-SEARCH_SPEED,  SEARCH_SPEED);   // line was on left
    else                  setMotors( SEARCH_SPEED, -SEARCH_SPEED);   // line was on right
    return;
  }
  lostSince = 0;

  // Case 2: all five on line -> crossing OR finish box
  if (count == 5) {
    if (allOnSince == 0) allOnSince = millis();
    if (STOP_ON_ALL_ON && millis() - allOnSince > FINISH_HOLD_MS) { stopMotors(); return; }
    setMotors(BASE_SPEED, BASE_SPEED);   // cross straight over a junction
    return;
  }
  allOnSince = 0;

  // Case 3: normal following - weighted position, PD control
  long sum = 0;
  for (int i = 0; i < 5; i++) if (onLine[i]) sum += WEIGHT[i];
  int position = sum / count;            // -2000 (far left) ... +2000 (far right)
  lastPosition = position;

  int error = position;
  int correction = (int)(KP * error + KD * (error - lastError));
  lastError = error;

  // Positive error = line to the right -> turn right (left faster, right slower)
  int leftSpeed  = constrain(BASE_SPEED + correction, -MAX_SPEED, MAX_SPEED);
  int rightSpeed = constrain(BASE_SPEED - correction, -MAX_SPEED, MAX_SPEED);
  setMotors(leftSpeed, rightSpeed);

#if DEBUG_SERIAL
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 100) {
    lastPrint = millis();
    for (int i = 0; i < 5; i++) Serial.print(onLine[i] ? '1' : '0');
    Serial.print(F("  pos=")); Serial.print(position);
    Serial.print(F("  L=")); Serial.print(leftSpeed);
    Serial.print(F("  R=")); Serial.println(rightSpeed);
  }
#endif
}
