#include <Servo.h>

Servo horizontal;  // Base servo
int servoh = 180;
int servohLimitHigh = 175;
int servohLimitLow = 5;

Servo vertical;  // vertical servo
int servov = 45;
int servovLimitHigh = 60;
int servovLimitLow = 1;

// LDR pin connections
int ldrlt = A2;  // LDR top left
int ldrrt = A3;  // LDR top right
int ldrld = A4;  // LDR down left
int ldrrd = A5;  // LDR down right

void setup() {
  Serial.begin(9600);  // Initialize serial communication
  horizontal.attach(9);
  vertical.attach(10);
  horizontal.write(180);
  vertical.write(45);
  delay(2500);
}

void loop() {
  int lt = analogRead(ldrlt);  // Top left
  int rt = analogRead(ldrrt);  // Top right
  int ld = analogRead(ldrld);  // Down left
  int rd = analogRead(ldrrd);  // Down right

  int dtime = 10;
  int tol = 90;             // Tolerance
  int avt = (lt + rt) / 2;  // Average value top
  int avd = (ld + rd) / 2;  // Average value down
  int avl = (lt + ld) / 2;  // Average value left
  int avr = (rt + rd) / 2;  // Average value right
  int dvert = avt - avd;    // Difference up and down
  int dhoriz = avl - avr;   // Difference left and right

  // Print sensor readings and average values
  // Serial.print("LDR Readings - LT: "); Serial.print(lt);
  // Serial.print(", RT: "); Serial.print(rt);
  // Serial.print(", LD: "); Serial.print(ld);
  // Serial.print(", RD: "); Serial.println(rd);

  // Serial.print("Averages - AVT: "); Serial.print(avt);
  // Serial.print(", AVD: "); Serial.print(avd);
  // Serial.print(", AVL: "); Serial.print(avl);
  // Serial.print(", AVR: "); Serial.println(avr);

  // // Print differences
  // Serial.print("Differences - DVERT: "); Serial.print(dvert);
  // Serial.print(", DHORIZ: "); Serial.println(dhoriz);

  // Adjust vertical servo
  if (-1 * tol > dvert || dvert > tol) {
    if (avt > avd) {
      servov = ++servov;
      if (servov > servovLimitHigh) { servov = servovLimitHigh; }
    } else if (avt < avd) {
      servov = --servov;
      if (servov < servovLimitLow) { servov = servovLimitLow; }
    }
    vertical.write(servov);
    Serial.print("Vertical Servo Position: "); Serial.println(servov);
  }

  // Adjust horizontal servo
  if (-1 * tol > dhoriz || dhoriz > tol) {
    if (avl > avr) {
      servoh = --servoh;
      if (servoh < servohLimitLow) { servoh = servohLimitLow; }
    } else if (avl < avr) {
      servoh = ++servoh;
      if (servoh > servohLimitHigh) { servoh = servohLimitHigh; }
    }
    horizontal.write(servoh);
    Serial.print("Horizontal Servo Position: "); Serial.println(servoh);
  }

  delay(dtime);
}
