#ifndef MOTORCONTROLLER_H
#define MOTORCONTROLLER_H

#include <Arduino.h>

class MotorController {
public:
    MotorController(int leftPin1, int leftPin2, int rightPin1, int rightPin2);

    void forward();
    void backward();
    void left();
    void right();
    void stop();

private:
    int _leftPin1, _leftPin2;
    int _rightPin1, _rightPin2;

    void initializePins();
    void controlLeftMotor(bool forward);
    void controlRightMotor(bool forward);
};

#endif
