/*
 *  TinyReceiver.cpp
 *
 *  Small memory footprint and no timer usage!
 *
 *  Receives IR protocol data of NEC protocol using pin change interrupts.
 *  On complete received IR command the function handleReceivedIRData(uint16_t aAddress, uint8_t aCommand, uint8_t aFlags)
 *  is called in Interrupt context but with interrupts being enabled to enable use of delay() etc.
 *  !!!!!!!!!!!!!!!!!!!!!!
 *  Functions called in interrupt context should be running as short as possible,
 *  so if you require longer action, save the data (address + command) and handle it in the main loop.
 *  !!!!!!!!!!!!!!!!!!!!!
 *
 * The FAST protocol is a proprietary modified JVC protocol without address, with parity and with a shorter header.
 *  FAST Protocol characteristics:
 * - Bit timing is like NEC or JVC
 * - The header is shorter, 3156 vs. 12500
 * - No address and 16 bit data, interpreted as 8 bit command and 8 bit inverted command,
 *     leading to a fixed protocol length of (6 + (16 * 3) + 1) * 526 = 55 * 526 = 28930 microseconds or 29 ms.
 * - Repeats are sent as complete frames but in a 50 ms period / with a 21 ms distance.
 *
 *
 *  This file is part of IRMP https://github.com/IRMP-org/IRMP.
 *  This file is part of Arduino-IRremote https://github.com/Arduino-IRremote/Arduino-IRremote.
 *
 ************************************************************************************
 * MIT License
 *
 * Copyright (c) 2022-2024 Armin Joachimsmeyer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ************************************************************************************
 */

#include <Arduino.h>

#include "PinDefinitionsAndMore.h"  // Set IR_RECEIVE_PIN for different CPU's

//#define DEBUG // to see if attachInterrupt is used
//#define TRACE // to see the state of the ISR state machine

/*
 * Protocol selection
 */
//#define DISABLE_PARITY_CHECKS // Disable parity checks. Saves 48 bytes of program memory.
//#define USE_EXTENDED_NEC_PROTOCOL // Like NEC, but take the 16 bit address as one 16 bit value and not as 8 bit normal and 8 bit inverted value.
//#define USE_ONKYO_PROTOCOL    // Like NEC, but take the 16 bit address and command each as one 16 bit value and not as 8 bit normal and 8 bit inverted value.
//#define USE_FAST_PROTOCOL     // Use FAST protocol instead of NEC / ONKYO.
//#define ENABLE_NEC2_REPEATS // Instead of sending / receiving the NEC special repeat code, send / receive the original frame for repeat.
/*
 * Set compile options to modify the generated code.
 */
//#define DISABLE_PARITY_CHECKS // Disable parity checks. Saves 48 bytes of program memory.
//#define USE_CALLBACK_FOR_TINY_RECEIVER  // Call the fixed function "void handleReceivedTinyIRData()" each time a frame or repeat is received.

#include "TinyIRReceiver.hpp"  // include the code

/*
 * Helper macro for getting a macro definition as string
 */
#if !defined(STR_HELPER)
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

uint8_t previousCommand = 0;
bool runSmoothTransition = true;
const int redPin = 9;
const int greenPin = 10;
const int bluePin = 11;
void setup() {
  Serial.begin(115200);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_PORT_USBVIRTUAL) || defined(SERIAL_USB) /*stm32duino*/ || defined(USBCON) /*STM32_stm32*/ || defined(SERIALUSB_PID) || defined(ARDUINO_attiny3217)
  delay(4000);  // To be able to connect Serial monitor after reset or power up and before first print out. Do not wait for an attached Serial Monitor!
#endif
  // Just to know which program is running on my Arduino
#if defined(ESP8266) || defined(ESP32)
  Serial.println();
#endif
  Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_TINYIR));

  // Enables the interrupt generation on change of IR input signal
  if (!initPCIInterruptForTinyReceiver()) {
    Serial.println(F("No interrupt available for pin " STR(IR_RECEIVE_PIN)));  // optimized out by the compiler, if not required :-)
  }
#if defined(true)
  Serial.println(F("Ready to receive Fast IR signals at pin " STR(IR_RECEIVE_PIN)));
#else
  Serial.println(F("Ready to receive NEC IR signals at pin " STR(IR_RECEIVE_PIN)));
#endif

  startSmoothTransition();
}

void loop() {
  if (TinyIRReceiverData.justWritten) {
    TinyIRReceiverData.justWritten = false;
#if !defined(USE_FAST_PROTOCOL)
    // We have no address at FAST protocol
    Serial.print(F("Address="));
    Serial.print(TinyIRReceiverData.Address, HEX);
    Serial.print(' ');
#endif
    Serial.print(F("Command="));
    Serial.print(TinyIRReceiverData.Command);
    if (TinyIRReceiverData.Flags == IRDATA_FLAGS_IS_REPEAT) {
      Serial.print(F(" Repeat"));
    }
    if (TinyIRReceiverData.Flags == IRDATA_FLAGS_PARITY_FAILED) {
      Serial.print(F(" Parity failed"));
#if !defined(USE_EXTENDED_NEC_PROTOCOL) && !defined(USE_ONKYO_PROTOCOL)
      Serial.print(F(", try USE_EXTENDED_NEC_PROTOCOL or USE_ONKYO_PROTOCOL"));
#endif
    }
  }
  if(TinyIRReceiverData.Command != 0 && TinyIRReceiverData.Command != previousCommand)
  {  handleCommand(TinyIRReceiverData.Command);
  Serial.println(runSmoothTransition);}
  if (runSmoothTransition)
    startSmoothTransition();
}
void handleCommand(uint8_t command) {
  Serial.print(F("Handling command: "));
  Serial.println(command);
  previousCommand = command;
  switch (command) {
    case 7:
      runSmoothTransition = false;
      Serial.println(F("Setting color to White"));
      setColor(230, 225, 80);  // White
      break;
    case 4:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Red"));
      setColor(185, 0, 0);  // Red
      break;
    case 5:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Green"));
      setColor(21, 200, 0);  // Green
      break;
    case 6:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Blue"));
      setColor(0, 0, 255);  // Blue
      break;
    case 10:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Below Blue"));
      setColor(0, 87, 100);  // Below Blue
      break;
    case 13:
      runSmoothTransition = false;
      Serial.println(F("Setting color to CYAN"));
      setColor(0, 87, 100);  // CYAN
      break;
    case 16:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Light Orange"));
      setColor(185, 125, 0);  // Light Orange
      break;
    case 17:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Dark CYAN"));
      setColor(162, 185, 255);  // Dark CYAN
      break;
    case 18:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Light Purple"));
      setColor(103, 0, 255);  // Light Purple
      break;
    case 22:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Pink"));
      setColor(255, 0, 228);  // Pink
      break;
    case 20:
      runSmoothTransition = false;
      Serial.println(F("Setting color to Yellow"));
      setColor(185, 225, 0);  // Yellow
      break;
    case 23:
      runSmoothTransition = true;
      Serial.println(F("Starting smooth transition"));
      startSmoothTransition();  // Smooth transition
      break;
    default:
      runSmoothTransition = true;
      Serial.println(F("Unknown command: Turning Off LEDs"));
      break;
  }
}

void setColor(int red, int green, int blue) {

  Serial.print("R:" + String(red) + " G:" + String(green) + " B:" + String(blue) + "\n");
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

void startSmoothTransition() {
  int red = 0;
  int green = 0;
  int blue = 0;
  int step = 1;
  if (runSmoothTransition) {
    for (red = 0; red <= 255; red += step) {
      if (!runSmoothTransition) return;
      setColor(red, 0, 0);
      delay(5);
    }
    for (green = 0; green <= 255; green += step) {
      if (!runSmoothTransition) return;
      setColor(red, green, 0);
      delay(5);
    }
    for (blue = 0; blue <= 255; blue += step) {
      if (!runSmoothTransition) return;
      setColor(red, green, blue);
      delay(5);
    }
    for (red = 255; red >= 0; red -= step) {
      if (!runSmoothTransition) return;
      setColor(red, green, blue);
      delay(5);
    }
    for (green = 255; green >= 0; green -= step) {
      if (!runSmoothTransition) return;
      setColor(red, green, blue);
      delay(5);
    }
    for (blue = 255; blue >= 0; blue -= step) {
      if (!runSmoothTransition) return;
      setColor(red, green, blue);
      delay(5);
    }
  }
}

/*
 * Optional code, if you require a callback
 */
#if defined(USE_CALLBACK_FOR_TINY_RECEIVER)
/*
 * This is the function, which is called if a complete frame was received
 * It runs in an ISR context with interrupts enabled, so functions like delay() etc. should work here
 */
#if defined(ESP8266) || defined(ESP32)
IRAM_ATTR
#endif

void handleReceivedTinyIRData() {
#if defined(ARDUINO_ARCH_MBED) || defined(ESP32)
  /*
     * Printing is not allowed in ISR context for any kind of RTOS, so we use the slihjtly more complex,
     * but recommended way for handling a callback :-). Copy data for main loop.
     * For Mbed we get a kernel panic and "Error Message: Semaphore: 0, Not allowed in ISR context" for Serial.print()
     * for ESP32 we get a "Guru Meditation Error: Core  1 panic'ed" (we also have an RTOS running!)
     */
  // Do something useful here...
#else
  // As an example, print very short output, since we are in an interrupt context and do not want to miss the next interrupts of the repeats coming soon
  printTinyReceiverResultMinimal(&Serial);
#endif
}
#endif
