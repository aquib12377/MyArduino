#include <Arduino.h>
#include "PinDefinitionsAndMore.h"
#include "TinyIRReceiver.hpp"

int redPin = 9;
int greenPin = 3;
int bluePin = 11;
bool smooth = false;
void setup() {
  Serial.begin(115200);
#if defined(_AVR_ATmega32U4_) || defined(SERIAL_PORT_USBVIRTUAL) || defined(SERIAL_USB) || defined(USBCON) || defined(SERIALUSB_PID) || defined(ARDUINO_attiny3217)
  delay(400);
#endif
#if defined(ESP8266) || defined(ESP32)
  Serial.println();
#endif
  //Serial.println(F("START " (_FILE_) " from " _DATE_ "\r\nUsing library version " VERSION_TINYIR));

  if (!initPCIInterruptForTinyReceiver()) {
    Serial.println(F("No interrupt available for pin " STR(IR_RECEIVE_PIN)));
  }

#if defined(USE_FAST_PROTOCOL)
  Serial.println(F("Ready to receive Fast IR signals at pin " STR(IR_RECEIVE_PIN)));
#else
  Serial.println(F("Ready to receive NEC IR signals at pin " STR(IR_RECEIVE_PIN)));
#endif

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  digitalWrite(redPin, LOW);
  digitalWrite(greenPin, LOW);
  digitalWrite(greenPin, LOW);
  smoothTransition();
}

void setColor(int redValue, int greenValue, int blueValue) {

  //Serial.println("R: " + String(redValue) + " | G: " + String(greenValue) + " | B: " + String(blueValue));
  analogWrite(redPin, redValue);
  analogWrite(greenPin, greenValue);
  analogWrite(bluePin, blueValue);
}

void smoothTransition() {
  Serial.println("Smooth 1");
  for (int i = 0; i < 255; ++i) {
    setColor(i, 255 - i, 0);
    delay(10);
  }
  Serial.println("Smooth 2");
  for (int i = 0; i < 255; ++i) {
    setColor(255 - i, 0, i);
    delay(10);
  }
  Serial.println("Smooth 3");
  for (int i = 0; i < 255; ++i) {
    setColor(0, i, 255 - i);
    delay(10);
  }
  Serial.println("Smooth End");
  smooth = true;
}

void loop() {
  if (smooth == true) {
      Serial.println("Smooth Loop");
      smoothTransition();
    }
  if (TinyIRReceiverData.justWritten) {
    TinyIRReceiverData.justWritten = false;


    switch (TinyIRReceiverData.Command) {
      case 4:
        smooth = false;
        setColor(255, 0, 0);
        break;

      case 5:
        smooth = false;
        setColor(0, 255, 0);
        break;

      case 6:
        smooth = false;
        setColor(0, 0, 255);
        break;

      case 17:
        smooth = false;
        setColor(162, 185, 255);
        break;

      case 7:
        smooth = false;
        setColor(230, 225, 80);
        break;

      case 10:
        smooth = false;
        setColor(0, 87, 100);
        break;

      case 13:
        smooth = false;
        setColor(0, 87, 100);
        break;

      case 16:
        smooth = false;
        setColor(185, 125, 0);
        break;

      case 20:
        smooth = false;
        setColor(185, 225, 0);
        break;

      case 18:
        smooth = false;
        setColor(103, 0, 255);
        break;

      case 22:
        smooth = false;
        setColor(255, 0, 228);
        break;

      case 3:
      case 2:
        smooth = false;
        setColor(0, 0, 0);
        break;

      case 23:
        smooth = true;
        smoothTransition();
        break;

      default:
        break;
    }
    
    Serial.println();
  }
}