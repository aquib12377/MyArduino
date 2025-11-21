#include <SoftwareSerial.h>

// RE and DE Pins set the RS485 module
// to Receiver or Transmitter mode
#define RE 8
#define DE 7

// SoftwareSerial: RX, TX
// (Using D2 = RX, D3 = TX)
SoftwareSerial mod(2, 3);

// Set this to your sensor address:
// If your NPK sensor is 06 → use 0x06
// If default → try 0x01
#define SLAVE_ID 0x01

// ------------- Modbus CRC16 (standard 0xA001) -------------
uint16_t crc16_modbus(const uint8_t *buf, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];
    for (int i = 0; i < 8; i++) {
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

// Utility: print a frame in HEX
void printFrame(const char *title, const uint8_t *data, size_t len) {
  Serial.print(title);
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ------------- Low-level Modbus query -------------
// Read holding register (1 register, 16-bit)
// Returns true on success; outValue updated
bool readHolding16(uint8_t slaveId, uint16_t regAddr, uint16_t &outValue) {
  uint8_t req[8];

  // Build request: [ID][03][reg_hi][reg_lo][00][01][CRC_L][CRC_H]
  req[0] = slaveId;
  req[1] = 0x03; // Read Holding Registers
  req[2] = highByte(regAddr);
  req[3] = lowByte(regAddr);
  req[4] = 0x00;
  req[5] = 0x01;

  uint16_t crc = crc16_modbus(req, 6);
  req[6] = lowByte(crc);
  req[7] = highByte(crc);

  Serial.println(F("\n---- MODBUS QUERY ----"));
  printFrame("TX → ", req, 8);

  // Enable TX
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delayMicroseconds(50);

  // Clear any old data
  while (mod.available()) mod.read();

  // Send request
  mod.write(req, 8);
  mod.flush();
  delay(5); // small gap

  // Back to RX
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);

  // Wait for response: we expect 7 bytes
  const uint8_t expected = 7;
  uint8_t resp[expected];
  uint32_t start = millis();

  while ((mod.available() < expected) && (millis() - start < 500)) {
    // wait up to 500ms
    // no-op
  }

  int avail = mod.available();
  Serial.print(F("Bytes available: "));
  Serial.println(avail);

  if (avail <= 0) {
    Serial.println(F("⛔ Timeout: 0 bytes received"));
    return false;
  }

  // Read up to expected bytes
  uint8_t toRead = (avail >= expected) ? expected : avail;
  Serial.print(F("Reading ")); Serial.print(toRead); Serial.println(F(" bytes:"));

  for (uint8_t i = 0; i < toRead; i++) {
    int b = mod.read();
    if (b < 0) {
      Serial.println(F("⛔ mod.read() = -1"));
      return false;
    }
    resp[i] = (uint8_t)b;

    if (resp[i] < 0x10) Serial.print("0");
    Serial.print(resp[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println(F("------------------------"));

  // Basic validation
  if (toRead < 5) {
    Serial.println(F("⛔ Not enough bytes to parse"));
    return false;
  }

  // Expected: [ID][03][02][HI][LO][CRC_L][CRC_H]
  if (resp[0] != slaveId) {
    Serial.print(F("⛔ Wrong slave ID in response: 0x"));
    Serial.println(resp[0], HEX);
    return false;
  }
  if (resp[1] != 0x03 || resp[2] != 0x02) {
    Serial.println(F("⛔ Wrong function code or byte count"));
    return false;
  }

  outValue = ((uint16_t)resp[3] << 8) | resp[4];
  Serial.print(F("Parsed value = "));
  Serial.println(outValue);

  // Optional CRC check on response
  uint16_t crcResp = ((uint16_t)resp[6] << 8) | resp[5];
  uint16_t crcCalc = crc16_modbus(resp, 5);
  if (crcResp != crcCalc) {
    Serial.print(F("⚠ CRC mismatch! resp="));
    Serial.print(crcResp, HEX);
    Serial.print(F(" calc="));
    Serial.println(crcCalc, HEX);
    // You can return false here if you want strict check
  }

  return true;
}

// High-level helpers for N, P, K
bool readNitrogen(uint16_t &val)   { return readHolding16(SLAVE_ID, 0x001E, val); }
bool readPhosphorus(uint16_t &val) { return readHolding16(SLAVE_ID, 0x001F, val); }
bool readPotassium(uint16_t &val)  { return readHolding16(SLAVE_ID, 0x0020, val); }

void setup() {
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);

  digitalWrite(RE, LOW); // start in RX mode
  digitalWrite(DE, LOW);

  Serial.begin(9600);
  delay(500);

  Serial.println(F("NPK Sensor (Modbus RTU over RS485)"));
  Serial.print(F("Using slave ID: 0x"));
  Serial.println(SLAVE_ID, HEX);
  Serial.println(F("Baud: 9600 8N1\n"));

  mod.begin(9600);
  delay(500);
}

void loop() {
  uint16_t N = 0, P = 0, K = 0;

  Serial.println(F("\n========== NEW NPK READ =========="));

  bool okN = readNitrogen(N);
  delay(200);
  bool okP = readPhosphorus(P);
  delay(200);
  bool okK = readPotassium(K);
  delay(200);

  if (okN && okP && okK) {
    Serial.print(F("Nitrogen (N): "));     Serial.print(N); Serial.println(F(" mg/kg"));
    Serial.print(F("Phosphorus (P): "));   Serial.print(P); Serial.println(F(" mg/kg"));
    Serial.print(F("Potassium (K): "));    Serial.print(K); Serial.println(F(" mg/kg"));
  } else {
    Serial.println(F("❌ Failed to read one or more values"));
    Serial.print(F("N OK: ")); Serial.println(okN);
    Serial.print(F("P OK: ")); Serial.println(okP);
    Serial.print(F("K OK: ")); Serial.println(okK);
  }

  Serial.println(F("==================================\n"));
  delay(3000);
}
