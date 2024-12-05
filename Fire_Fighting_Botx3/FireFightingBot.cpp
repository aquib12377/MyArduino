#include "FireFightingBot.h"

FireFightingBot::FireFightingBot(int rightSensorPin, int centerSensorPin, int leftSensorPin)
    : _leftSensorPin(leftSensorPin), _centerSensorPin(centerSensorPin), _rightSensorPin(rightSensorPin) {
    if (_leftSensorPin != -1) pinMode(_leftSensorPin, INPUT_PULLUP);
    if (_centerSensorPin != -1) pinMode(_centerSensorPin, INPUT_PULLUP);
    if (_rightSensorPin != -1) pinMode(_rightSensorPin, INPUT_PULLUP);
}

int FireFightingBot::readSensor(int pin) {
    int total = 0;
    for (int i = 0; i < 5; i++) {
        total += analogRead(pin);
    }
    return total / 5; // Average value
}

FireDirection FireFightingBot::detectFire() {
    bool leftFire = false, centerFire = false, rightFire = false;

    if (_leftSensorPin != -1) {
        leftFire = readSensor(_leftSensorPin) < 500;
    }
    if (_centerSensorPin != -1) {
        centerFire = readSensor(_centerSensorPin) < 500;
    }
    if (_rightSensorPin != -1) {
        rightFire = readSensor(_rightSensorPin) < 500;
    }

    if (centerFire) return CENTER_FIRE;
    if (leftFire) return LEFT_FIRE;
    if (rightFire) return RIGHT_FIRE;

    return NO_FIRE;
}
