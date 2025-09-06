/* =======================================================================
   Alpha Electronz | alphaelectronz.tech
   Sketch: Ultrasonic Proximity (Red/Green LED)   Version: v1.0
   Board:  Arduino Nano/Uno                       Date: 2025-09-05
   Purpose: Turn GREEN LED on when clear; RED when object is near.
   ======================================================================= */

#define TRIG_PIN 9
#define ECHO_PIN 8
#define LED_RED  13
#define LED_GRN  12

// Distance threshold in centimeters
const int SAFE_CM = 5;       // >= SAFE_CM → GREEN,  < SAFE_CM → RED
const int HYST_CM = 3;        // hysteresis to prevent flicker

// simple state memory for hysteresis
bool nearState = false;

long readDistanceCm() {
  // trigger ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // echo time in microseconds; timeout ~30 ms (~5 m range)
  long us = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (us == 0) return -1;          // timeout/no reading
  return us / 58;                  // convert to cm
}

int smoothDistance() {
  // take 3 quick samples and keep the median (robust against spikes)
  long a = readDistanceCm(); delay(10);
  long b = readDistanceCm(); delay(10);
  long c = readDistanceCm();

  // replace any timeouts with a large value (treat as "far")
  if (a < 0) a = 9999;
  if (b < 0) b = 9999;
  if (c < 0) c = 9999;

  // median-of-three
  long mx = max(a, max(b, c));
  long mn = min(a, min(b, c));
  return (int)(a + b + c - mx - mn);
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GRN, OUTPUT);

  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GRN, LOW);

  Serial.begin(9600);
}

void loop() {
  int cm = smoothDistance();
  Serial.print("Distance: "); Serial.print(cm); Serial.println(" cm");

  // hysteresis: once "near", require SAFE_CM + HYST_CM to flip back to "far"
  if (!nearState && cm >= 0 && cm < SAFE_CM) {
    nearState = true;
  } else if (nearState && (cm >= SAFE_CM + HYST_CM || cm == 9999)) {
    nearState = false;
  }

  if (nearState) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GRN, LOW);
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GRN, HIGH);
  }

  delay(60); // update rate
}
