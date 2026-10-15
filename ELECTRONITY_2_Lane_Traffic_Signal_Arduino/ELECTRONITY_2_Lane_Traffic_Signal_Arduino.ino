// ============================================================
//  Traffic Light Controller — 2 Lane
// ============================================================
//  Author  : Electronixity
//  Website : https://electronixity.com
//  License : MIT — free to use, modify, and share with credit
// ============================================================
//  Wiring:
//    TL1 → R:2  Y:3  G:4
//    TL2 → R:5  Y:6  G:7
// ============================================================

#define TL1_R 2
#define TL1_Y 3
#define TL1_G 4
#define TL2_R 5
#define TL2_Y 6
#define TL2_G 7

#define GREEN_TIME  3000
#define YELLOW_TIME  3000

void allOff() {
  digitalWrite(TL1_R, LOW); digitalWrite(TL1_Y, LOW); digitalWrite(TL1_G, LOW);
  digitalWrite(TL2_R, LOW); digitalWrite(TL2_Y, LOW); digitalWrite(TL2_G, LOW);
}

void setup() {
  for (int p = 2; p <= 7; p++) pinMode(p, OUTPUT);
  allOff();
}

void loop() {
  // Phase 1: Lane 1 GREEN, Lane 2 RED
  allOff();
  digitalWrite(TL1_G, HIGH);
  digitalWrite(TL2_R, HIGH);
  delay(GREEN_TIME);

  // Phase 2: Lane 1 YELLOW, Lane 2 RED
  allOff();
  digitalWrite(TL1_Y, HIGH);
  digitalWrite(TL2_R, HIGH);
  delay(YELLOW_TIME);

  // Phase 3: Lane 2 GREEN, Lane 1 RED
  allOff();
  digitalWrite(TL1_R, HIGH);
  digitalWrite(TL2_G, HIGH);
  delay(GREEN_TIME);

  // Phase 4: Lane 2 YELLOW, Lane 1 RED
  allOff();
  digitalWrite(TL1_R, HIGH);
  digitalWrite(TL2_Y, HIGH);
  delay(YELLOW_TIME);
}