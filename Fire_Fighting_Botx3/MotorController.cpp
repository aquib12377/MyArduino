#include "MotorController.h"

MotorController::MotorController(int leftPin1, int leftPin2, int rightPin1, int rightPin2)
    : _leftPin1(leftPin1), _leftPin2(leftPin2), _rightPin1(rightPin1), _rightPin2(rightPin2) {
    initializePins();
}

void MotorController::initializePins() {
    pinMode(_leftPin1, OUTPUT);
    pinMode(_leftPin2, OUTPUT);
    pinMode(_rightPin1, OUTPUT);
    pinMode(_rightPin2, OUTPUT);
}

void MotorController::controlLeftMotor(bool forward) {
    if (forward) {
        digitalWrite(_leftPin1, HIGH);
        digitalWrite(_leftPin2, LOW);
    } else {
        digitalWrite(_leftPin1, LOW);
        digitalWrite(_leftPin2, HIGH);
    }
}

void MotorController::controlRightMotor(bool forward) {
    if (forward) {
        digitalWrite(_rightPin1, HIGH);
        digitalWrite(_rightPin2, LOW);
    } else {
        digitalWrite(_rightPin1, LOW);
        digitalWrite(_rightPin2, HIGH);
    }
}

void MotorController::forward() {
    controlLeftMotor(true);
    controlRightMotor(true);
}

void MotorController::backward() {
    controlLeftMotor(false);
    controlRightMotor(false);
}

void MotorController::left() {
    controlLeftMotor(false);
    controlRightMotor(true);
}

void MotorController::right() {
    controlLeftMotor(true);
    controlRightMotor(false);
}

void MotorController::stop() {
    digitalWrite(_leftPin1, LOW);
    digitalWrite(_leftPin2, LOW);
    digitalWrite(_rightPin1, LOW);
    digitalWrite(_rightPin2, LOW);
}
