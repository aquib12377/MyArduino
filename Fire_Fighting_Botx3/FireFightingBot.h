#ifndef FIREFIGHTINGBOT_H
#define FIREFIGHTINGBOT_H

#include <Arduino.h>

enum FireDirection {
    NO_FIRE,
    LEFT_FIRE,
    CENTER_FIRE,
    RIGHT_FIRE
};

class FireFightingBot {
public:
    FireFightingBot(int leftSensorPin = -1, int centerSensorPin = -1, int rightSensorPin = -1);

    FireDirection detectFire();
    int readSensor(int pin);
private:
    int _leftSensorPin;
    int _centerSensorPin;
    int _rightSensorPin;

    
};

#endif
