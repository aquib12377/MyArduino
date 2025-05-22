#pragma once
#include <esp_now.h>
#include <WiFi.h>

/* ———— ESP-NOW payload ———— */
typedef struct __attribute__((packed)) {
  uint8_t doorId;   // 0..3
  uint8_t state;    // 0 = CLOSED, 1 = OPEN
  uint32_t millis;  // Sender millis() for optional debug
} DoorMsg;

#define CMD_OPEN  0
#define CMD_CLOSE 1

/* ———— fixed MAC of the Master (fill in later) ———— */
/*94:54:c5:a9:45:c8*/
constexpr uint8_t MASTER_MAC[6] = {0x94,0x54,0xC5,0xA9,0x45,0xC8};

/* Reed switch pin on door node */
constexpr int REED_PIN = 15;
