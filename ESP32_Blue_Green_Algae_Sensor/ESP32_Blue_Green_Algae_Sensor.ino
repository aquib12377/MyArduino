/**
 * RS-BA-N01-2 blue-green-algae sensor reader
 * – ESP32 + RS-485 (HW-Serial2) + ModbusRTU master –
 *
 * ─ Registers used ─
 *   0x0000–0x0001  float (algae conc. cells/mL)
 *   0x0002         int16/10 (temperature °C)
 * ─ Connection ─
 *   ESP32 GPIO-17 (TX2) → DI  (RS-485)
 *   ESP32 GPIO-16 (RX2) ← RO
 *   ESP32 GPIO-4  (DE/RE) → DE & RE (tied together)
 *   Sensor Yellow (A)  ↔  RS-485 A
 *   Sensor Blue   (B)  ↔  RS-485 B
 *   Sensor Brown  → +12 V (for example)
 *   Sensor Black  → GND  (and ESP32 GND)

 */

#include <ModbusRTU.h>          // by Alexander Emelianov
HardwareSerial RS485(2);        // UART2

// ----- pin mapping (adapt if you like) -----
#define PIN_RX     16
#define PIN_TX     17
#define PIN_DE_RE  4

// ----- sensor settings -----
const uint8_t  SLAVE_ID   = 1;      // factory default
const uint32_t BAUD       = 9600;   // code 2 in 0×07D1

ModbusRTU mb;
uint16_t algaeReg[2];     // two words
uint16_t tempReg;

void setup() {
  Serial.begin(115200);

  RS485.begin(BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  mb.begin(&RS485, PIN_DE_RE);  // library drives DE/RE automatically
  mb.master();

  Serial.println(F("Blue-green-algae sensor demo"));
}

void loop() {
  static uint32_t nextPoll = 0;
  mb.task();               // keep state-machine running

  if (millis() > nextPoll) {        // poll every 2 s
    nextPoll = millis() + 2000;

    /* 1) read algae (2 words) */
    mb.readHreg(SLAVE_ID, 0x0000, algaeReg, 2,
      [](Modbus::ResultCode rc, uint16_t, void*) {
        if (rc == Modbus::EX_SUCCESS) {
          float cyanos = modbusFloatBE(algaeReg);
          Serial.printf("Algae: %.1f cells/mL  ", cyanos);
        } else {
          Serial.printf("Algae read error: %02X  ", rc);
        }
      }
    );

    /* 2) read temperature (1 word) */
    mb.readHreg(SLAVE_ID, 0x0002, &tempReg, 1,
      [](Modbus::ResultCode rc, uint16_t, void*) {
        if (rc == Modbus::EX_SUCCESS) {
          float t = int16_t(tempReg) / 10.0f;
          Serial.printf("Temp: %.1f °C\n", t);
        } else {
          Serial.printf("Temp read error: %02X\n", rc);
        }
      }
    );
  }
}

/* ─── helper: converts two BE registers to float ─── */
float modbusFloatBE(const uint16_t r[2]) {
  uint32_t raw = (uint32_t)r[0] << 16 | r[1];
  uint8_t  b[] = { uint8_t(raw >> 24), uint8_t(raw >> 16),
                   uint8_t(raw >>  8), uint8_t(raw) };
  float f;
  memcpy(&f, b, 4);
  return f;
}
