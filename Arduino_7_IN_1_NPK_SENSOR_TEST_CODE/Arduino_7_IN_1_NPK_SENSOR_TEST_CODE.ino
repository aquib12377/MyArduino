#include <SoftwareSerial.h>

// -------- RS485 control pins --------
#define RE_PIN 8
#define DE_PIN 7

// -------- SoftwareSerial for RS485 (UNO/MEGA) --------
// RX = 2, TX = 3
SoftwareSerial rs485Serial(2, 3);

// -------- Modbus settings --------
const byte SLAVE_ID           = 0x06;     // sensor address (often 1)
const unsigned long TIMEOUT   = 300;      // ms to wait for response
const unsigned long GAP_DELAY = 50;       // ms between queries

// Example register map (CHECK YOUR MANUAL)
const uint16_t REG_SOIL_MOIST = 0x0001;
const uint16_t REG_SOIL_TEMP  = 0x0002;
const uint16_t REG_SOIL_EC    = 0x0003;
const uint16_t REG_SOIL_PH    = 0x0004;
const uint16_t REG_NITROGEN   = 0x001E;
const uint16_t REG_PHOSPHORUS = 0x001F;
const uint16_t REG_POTASSIUM  = 0x0020;

// -------- CRC16 (Modbus RTU) --------
uint16_t modbusCRC16(const uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

// -------- Helpers to control RS485 direction --------
void rs485Transmit() {
  digitalWrite(DE_PIN, HIGH);
  digitalWrite(RE_PIN, HIGH);
}

void rs485Receive() {
  digitalWrite(DE_PIN, LOW);
  digitalWrite(RE_PIN, LOW);
}

// -------- Generic Modbus Read Holding Registers (0x03) --------
bool readHoldingRegisters(uint8_t id, uint16_t regStart, uint16_t numRegs, uint16_t *dest) {
  uint8_t frame[8];

  frame[0] = id;                 // Slave ID
  frame[1] = 0x03;               // Function code: Read Holding Registers
  frame[2] = regStart >> 8;      // Start address high byte
  frame[3] = regStart & 0xFF;    // Start address low byte
  frame[4] = numRegs >> 8;       // Number of registers high byte
  frame[5] = numRegs & 0xFF;     // Number of registers low byte

  uint16_t crc = modbusCRC16(frame, 6);
  frame[6] = crc & 0xFF;         // CRC low
  frame[7] = crc >> 8;           // CRC high

  // Clear any previous data
  while (rs485Serial.available()) rs485Serial.read();

  // Send request
  rs485Transmit();
  rs485Serial.write(frame, 8);
  rs485Serial.flush();           // wait for bytes to go out
  rs485Receive();

  // Response should be:
  // ID (1) + FC (1) + ByteCount (1) + Data (2*numRegs) + CRC (2)
  uint8_t expectedBytes = 5 + 2 * numRegs;
  uint8_t buffer[64];
  uint8_t idx = 0;
  unsigned long startTime = millis();

  // Read with timeout
  while ((millis() - startTime) < TIMEOUT) {
    while (rs485Serial.available()) {
      if (idx < sizeof(buffer)) {
        buffer[idx++] = rs485Serial.read();
      } else {
        rs485Serial.read(); // discard extra
      }
    }
    if (idx >= expectedBytes) break;
  }

  if (idx < expectedBytes) {
    Serial.println(F("Timeout / incomplete response"));
    return false;
  }

  // Check CRC
  uint16_t respCRC = (buffer[idx - 1] << 8) | buffer[idx - 2];
  uint16_t calcCRC = modbusCRC16(buffer, idx - 2);
  if (respCRC != calcCRC) {
    Serial.println(F("CRC error"));
    return false;
  }

  // Check slave ID and function
  if (buffer[0] != id || buffer[1] != 0x03) {
    Serial.println(F("Invalid ID/Function in response"));
    return false;
  }

  // Parse data
  uint8_t byteCount = buffer[2];
  if (byteCount != 2 * numRegs) {
    Serial.println(F("Byte count mismatch"));
    return false;
  }

  for (uint8_t i = 0; i < numRegs; i++) {
    uint8_t hi = buffer[3 + 2 * i];
    uint8_t lo = buffer[4 + 2 * i];
    dest[i] = ((uint16_t)hi << 8) | lo;
  }

  return true;
}

// ----------------------------------------------------
// Setup & Loop
// ----------------------------------------------------
void setup() {
  pinMode(RE_PIN, OUTPUT);
  pinMode(DE_PIN, OUTPUT);
  rs485Receive();  // start in receive mode

  Serial.begin(9600);
  while (!Serial) {;}

  rs485Serial.begin(9600);  // Most sensors default 9600 8N1

  Serial.println(F("7-in-1 NPK Sensor test"));
}

void loop() {
  uint16_t regs[7];  // we will read 7 regs starting from REG_SOIL_MOIST

  // Read 7 consecutive registers: moisture, temp, EC, pH, N, P, K
  bool ok = readHoldingRegisters(
    SLAVE_ID,
    REG_SOIL_MOIST,  // 0x0001
    7,
    regs
  );

  if (ok) {
    uint16_t rawMoist   = regs[0];
    uint16_t rawTemp    = regs[1];
    uint16_t rawEC      = regs[2];
    uint16_t rawPH      = regs[3];
    uint16_t rawN       = regs[4];
    uint16_t rawP       = regs[5];
    uint16_t rawK       = regs[6];

    // Scale (EXAMPLE – match to your datasheet!)
    float moisturePct   = rawMoist / 10.0;   // or rawMoist/1000 * 100
    float tempC         = rawTemp / 10.0;
    float pH            = rawPH / 10.0;

    Serial.println(F("---- Sensor Data ----"));
    Serial.print(F("Soil Moisture : ")); Serial.print(moisturePct); Serial.println(F(" %"));
    Serial.print(F("Soil Temp     : ")); Serial.print(tempC);       Serial.println(F(" °C"));
    Serial.print(F("EC/Cond       : ")); Serial.print(rawEC);       Serial.println(F(" uS/cm"));
    Serial.print(F("pH            : ")); Serial.println(pH);
    Serial.print(F("Nitrogen (N)  : ")); Serial.print(rawN);        Serial.println(F(" mg/kg"));
    Serial.print(F("Phosphorus(P) : ")); Serial.print(rawP);        Serial.println(F(" mg/kg"));
    Serial.print(F("Potassium (K) : ")); Serial.print(rawK);        Serial.println(F(" mg/kg"));
    Serial.println();
  } else {
    Serial.println(F("Failed to read sensor!"));
  }

  delay(2000);  // wait before next read
}
