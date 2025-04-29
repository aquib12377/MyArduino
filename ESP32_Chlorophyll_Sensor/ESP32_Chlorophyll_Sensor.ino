/**
 * CHLO-206A online chlorophyll sensor reader
 * – ESP32 + RS-485 + ModbusRTU –
 * Registers read: 0x0000 .. 0x0003  (4 × 16-bit)
 *   [0] Craw  (µg/L)
 *   [1] Cdec  (dec. places)
 *   [2] Traw  (°C)
 *   [3] Tdec  (dec. places)

 Sensor     | Function   | Transceiver | ESP32 pin (example)
  Red       | +12 – 24 V | Vcc         | external PSU
  Black     | GND        | GND         | GND
  Blue      | 485-A      | A           | 
  White     | 485-B      | B           | 
  DE+RE     | —          | DE/RE       | GPIO 4
TX2 → DI, RX2 ← RO |     | GPIO 17/16  | 
 */
#include <ModbusRTU.h>

HardwareSerial RS485(2);        // UART2
#define PIN_RX     16
#define PIN_TX     17
#define PIN_DE_RE   4           // DE & RE tied together

const uint8_t  SLAVE_ID = 6;    // factory default
const uint32_t BAUD     = 9600;

ModbusRTU mb;
uint16_t buf[4];

void setup() {
  Serial.begin(115200);
  RS485.begin(BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  mb.begin(&RS485, PIN_DE_RE);  // controls DE/RE for us
  mb.master();

  Serial.println(F("CHLO-206A chlorophyll demo – µg/L & °C"));
}

void loop() {
  static uint32_t next = 0;
  mb.task();                               // keep Modbus FSM alive

  if (millis() > next) {                   // poll every 2 s
    next = millis() + 2000;

    mb.readHreg(SLAVE_ID, 0x0000, buf, 4,
      [](Modbus::ResultCode rc, uint16_t, void*) {
        if (rc != Modbus::EX_SUCCESS) {
          Serial.printf("Modbus error: 0x%02X\n", rc);
          return;
        }
        float chl = buf[0] / powf(10, buf[1]);   // µg/L
        float tmp = buf[2] / powf(10, buf[3]);   // °C
        Serial.printf("Chlorophyll: %.1f µg/L   Temp: %.1f °C\n",
                      chl, tmp);
      }
    );
  }
}
