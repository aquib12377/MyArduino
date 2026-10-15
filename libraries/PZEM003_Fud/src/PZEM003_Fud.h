//    FILE: M Makhfud.h
//  AUTHOR: M Makhfud
// VERSION: 1.0.7
//    DATE: 2025-03-24
// PURPOSE: PZEM-003 library - measurement
//     URL: https://github.com/MuhammadMakhfud/PZEM003_Fud

#ifndef PZEM003_FUD_H
#define PZEM003_FUD_H

#include <SoftwareSerial.h>

class PZEM003_Fud
{
public:
    PZEM003_Fud(uint8_t rxPin, uint8_t txPin, uint8_t address = 0x01);
    void begin(uint32_t baudRate);
    void update();
    void resetEnergy();
    float getVoltage();
    float getCurrent();
    float getPower();
    float getEnergy();

private:
    SoftwareSerial pzemSerial;
    uint8_t address;
    byte buffer[100];
    byte *ptr;
    float voltage, current, power, energy;

    uint16_t calcCRC(byte *data, byte length);
    void readData();
    void sendCommand(byte *command, size_t length);
};

#endif
