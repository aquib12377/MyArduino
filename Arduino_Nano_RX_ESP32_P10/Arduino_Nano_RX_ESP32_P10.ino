#include <SPI.h>
#include <DMD2.h>
#include <fonts/Arial14.h>

SoftDMD dmd(3, 1);

String line = "";

void setup() {
  Serial.begin(9600);
  dmd.setBrightness(255);
  dmd.selectFont(Arial14);
  dmd.begin();
  dmd.clearScreen();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      dmd.clearScreen();
      DMD_TextBox box(dmd, 0, 2);
      box.print(line);
      line = "";
    } else {
      line += c;
    }
  }
}