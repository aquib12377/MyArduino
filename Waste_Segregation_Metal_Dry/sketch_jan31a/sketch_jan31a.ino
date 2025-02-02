#include <CheapStepper.h>
#include <Servo.h>

Servo servo1;
Servo dryBinServo;
Servo wetBinServo;
Servo metalBinServo;

#define ir 5
#define proxi 6
#define buzzer 12
int potPin = A0; //input pin
int soil = 0;
int fsoil;

CheapStepper stepper (8, 9, 10, 11);

void setup() {
  Serial.begin(9600);
  pinMode(proxi, INPUT_PULLUP);
  pinMode(ir, INPUT);
  pinMode(buzzer, OUTPUT);
  
  servo1.attach(7);
  dryBinServo.attach(3);
  wetBinServo.attach(4);
  metalBinServo.attach(2);

  stepper.setRpm(17);
  moveServoSlowly(70, 180, 10); // Move slowly to 180 degrees
  delay(1000);
  moveServoSlowly(180, 70, 10); // Move slowly back to 70 degrees
  delay(1000);
}

void loop() {
  fsoil = 0;
  int L = digitalRead(proxi);
  Serial.print(L);
  if (L == 0) {
    tone(buzzer, 1000, 1000);
    metalBinServo.write(90);
    stepper.moveDegreesCW(220);
    delay(1000);
    moveServoSlowly(70, 180, 10);
    delay(1000);
    moveServoSlowly(180, 70, 10);
    delay(1000);
    stepper.moveDegreesCCW(200);
    delay(1000);
    metalBinServo.write(0);
  }

  if (digitalRead(ir) == 0) {
    tone(buzzer, 1000, 500);
    delay(1000);
    soil = 0;
    for (int i = 0; i < 3; i++) {
      soil = analogRead(potPin);
      soil = constrain(soil, 485, 1023);
      fsoil = (map(soil, 485, 1023, 100, 0)) + fsoil;
      delay(75);
    }
    fsoil = fsoil / 3;
    Serial.print(fsoil);
    Serial.print("%"); Serial.print("\n");

    if (fsoil > 20) {
      wetBinServo.write(90);
      stepper.moveDegreesCW(120);
      delay(1000);
      moveServoSlowly(70, 180, 10);
      delay(1000);
      moveServoSlowly(180, 70, 10);
      delay(1000);
      stepper.moveDegreesCCW(120);
      delay(1000);
//      openContainer(wetBinServo);
    wetBinServo.write(0);
    } else {
      dryBinServo.write(90);
      tone(buzzer, 1000, 500);
      delay(1000);
      moveServoSlowly(70, 180, 10);
      delay(1000);
      moveServoSlowly(180, 70, 10);
      delay(1000);
      dryBinServo.write(0);
    }
  }
}

// Function to move the servo slowly
void moveServoSlowly(int startAngle, int endAngle, int delayTime) {
  if (startAngle < endAngle) {
    for (int pos = startAngle; pos <= endAngle; pos++) {
      servo1.write(pos);
      delay(delayTime);
    }
  } else {
    for (int pos = startAngle; pos >= endAngle; pos--) {
      servo1.write(pos);
      delay(delayTime);
    }
  }
}

void openContainer(Servo &servo) {
    delay(5000);
    servo.write(90);
    delay(2000);
    servo.write(0);
}
