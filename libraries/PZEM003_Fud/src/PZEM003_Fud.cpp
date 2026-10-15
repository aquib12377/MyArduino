//    FILE: M Makhfud.cpp
//  AUTHOR: M Makhfud
// VERSION: 1.0.7
//    DATE: 2025-03-24
// PURPOSE: ACS712 library - measurement
//     URL: https://github.com/MuhammadMakhfud/PZEM003_Fud

#include "PZEM003_Fud.h"
#include <Arduino.h>

PZEM003_Fud::PZEM003_Fud(uint8_t rxPin, uint8_t txPin, uint8_t address)
    : pzemSerial(rxPin, txPin), address(address), ptr(buffer) {}

void PZEM003_Fud::begin(uint32_t baudRate)
{
    pzemSerial.begin(baudRate);
}

void PZEM003_Fud::update()
{
    readData();
}

void PZEM003_Fud::resetEnergy()
{
    byte command[] = {address, 0x42, 0x00, 0x00};
    sendCommand(command, sizeof(command));
}

float PZEM003_Fud::getVoltage()
{
    return voltage;
}

float PZEM003_Fud::getCurrent()
{
    return current;
}

float PZEM003_Fud::getPower()
{
    return power;
}

float PZEM003_Fud::getEnergy()
{
    return energy;
}

void PZEM003_Fud::readData()
{
    byte command[] = {address, 0x04, 0x00, 0x00, 0x00, 0x08};
    sendCommand(command, sizeof(command));

    // Reset values
    voltage = 0;
    current = 0;
    power = 0;
    energy = 0;

    unsigned long startTime = millis();
    while (!pzemSerial.available() && (millis() - startTime) < 500)
    {
        yield(); // Biarkan WDT direset
    }

    if (pzemSerial.available())
    {
        // Clear buffer before reading new data
        memset(buffer, 0x00, sizeof(buffer));
        ptr = buffer;

        while (pzemSerial.available())
        {
            *ptr++ = pzemSerial.read();
            delay(10);
        }

        if (memcmp(buffer, command, 2) == 0)
        {
            ptr = buffer;
            voltage = ((ptr[3] << 8) + ptr[4]) * 0.01;
            current = ((ptr[5] << 8) + ptr[6]) * 0.01;
            power = (((uint32_t)ptr[9] << 24) + ((uint32_t)ptr[10] << 16) + (ptr[7] << 8) + ptr[8]) * 0.1;
            energy = (((uint32_t)ptr[13] << 24) + ((uint32_t)ptr[14] << 16) + (ptr[11] << 8) + ptr[12]);
        }
        else
        {
            // Serial.println("Data tidak cocok, reset serial");
            pzemSerial.end();
            delay(70);
            pzemSerial.begin(9600);
        }
    }
    else
    {
        // Serial.println("Tidak ada data sensor");
        pzemSerial.end();
        delay(50);
        pzemSerial.begin(9600);
    }
}

void PZEM003_Fud::sendCommand(byte *command, size_t length)
{
    uint16_t crc = calcCRC(command, length);
    pzemSerial.write(command, length);
    pzemSerial.write(lowByte(crc));
    pzemSerial.write(highByte(crc));
    delay(10);
}

uint16_t PZEM003_Fud::calcCRC(byte *data, byte length)
{
    uint16_t crc = 0xFFFF;
    for (byte i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (byte j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc = crc >> 1;
        }
    }
    return crc;
}
