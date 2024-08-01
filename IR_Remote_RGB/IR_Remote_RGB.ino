#include <Arduino.h>
#include <IRremote.hpp>

#define DECODE_NEC // Includes Apple and Onkyo

#define PROCESS_IR_RESULT_IN_MAIN_LOOP
#if defined(PROCESS_IR_RESULT_IN_MAIN_LOOP) || defined(ARDUINO_ARCH_MBED) || defined(ESP32)
volatile bool sIRDataJustReceived = false;
#endif

const int redPin = 9;
const int greenPin = 10;
const int bluePin = 11;
uint16_t receivedCommand;

void setup() {
    Serial.begin(115200);
    Serial.println(F("START"));

    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    pinMode(bluePin, OUTPUT);

    IrReceiver.begin(2, true);

    Serial.print(F("Ready to receive IR signals of protocols: "));
    printActiveIRProtocols(&Serial);

    //startSmoothTransition();
}

void loop() {
    if (IrReceiver.decode()) {
        receivedCommand = IrReceiver.decodedIRData.command;
        Serial.print(F("Received Command: "));
        Serial.println(receivedCommand, HEX);
        handleCommand(receivedCommand);
        IrReceiver.resume();
    } else {
        // Continue smooth transition if no command is received
        startSmoothTransition();
    }
}

void handleCommand(uint8_t command) {
    Serial.print(F("Handling command: "));
    Serial.println(command, HEX);
    switch (command) {
        case 0x7:
            Serial.println(F("Setting color to White"));
            setColor(230, 225, 80); // White
            break;
        case 0x4:
            Serial.println(F("Setting color to Red"));
            setColor(185, 0, 0); // Red
            break;
        case 0x5:
            Serial.println(F("Setting color to Green"));
            setColor(21, 200, 0); // Green
            break;
        case 0x6:
            Serial.println(F("Setting color to Blue"));
            setColor(0, 0, 255); // Blue
            break;
        case 0x10:
            Serial.println(F("Setting color to Below Blue"));
            setColor(0, 87, 100); // Below Blue
            break;
        case 0x13:
            Serial.println(F("Setting color to CYAN"));
            setColor(0, 87, 100); // CYAN
            break;
        case 0x16:
            Serial.println(F("Setting color to Light Orange"));
            setColor(185, 125, 0); // Light Orange
            break;
        case 0x17:
            Serial.println(F("Setting color to Dark CYAN"));
            setColor(162, 185, 255); // Dark CYAN
            break;
        case 0x18:
            Serial.println(F("Setting color to Light Purple"));
            setColor(103, 0, 255); // Light Purple
            break;
        case 0x22:
            Serial.println(F("Setting color to Pink"));
            setColor(255, 0, 228); // Pink
            break;
        case 0x20:
            Serial.println(F("Setting color to Yellow"));
            setColor(185, 225, 0); // Yellow
            break;
        case 0x23:
            Serial.println(F("Starting smooth transition"));
            startSmoothTransition(); // Smooth transition
            break;
        default:
            Serial.println(F("Unknown command"));
            break;
    }
}

void setColor(int red, int green, int blue) {
    Serial.print(F("Setting RGB color to: R="));
    Serial.print(red);
    Serial.print(F(" G="));
    Serial.print(green);
    Serial.print(F(" B="));
    Serial.println(blue);

    analogWrite(redPin, red);
    analogWrite(greenPin, green);
    analogWrite(bluePin, blue);
}

void startSmoothTransition() {
    int red = 0;
    int green = 0;
    int blue = 0;
    int step = 1;
    while (runSmoothTransition) {
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
