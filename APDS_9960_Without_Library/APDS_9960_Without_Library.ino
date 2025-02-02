#include <Wire.h>

// APDS-9960 I2C address
#define APDS9960_I2C_ADDR 0x39

// APDS-9960 Registers
#define APDS9960_ENABLE        0x80
#define APDS9960_GCONFIG4      0xAB
#define APDS9960_GPENTH        0xA0
#define APDS9960_GEXTH         0xA1
#define APDS9960_GCONFIG1      0xA2
#define APDS9960_GCONFIG2      0xA3
#define APDS9960_STATUS        0x93
#define APDS9960_CONFIG2       0x90
#define APDS9960_GOFFSET_U     0xA4
#define APDS9960_GOFFSET_D     0xA5
#define APDS9960_GOFFSET_L     0xA7
#define APDS9960_GOFFSET_R     0xA9
#define APDS9960_GPULSE        0xA6
#define APDS9960_GCONFIG3      0xAA
#define APDS9960_GFLVL         0xAE
#define APDS9960_GSTATUS       0xAF
#define APDS9960_GFIFO_U       0xFC
#define APDS9960_GFIFO_D       0xFD
#define APDS9960_GFIFO_L       0xFE
#define APDS9960_GFIFO_R       0xFF
#define APDS9960_CONFIG1       0x8D

// ENABLE register bits
#define ENABLE_PON             0x01  // Power ON
#define ENABLE_GEN             0x40  // Gesture Enable

// GCONFIG4 bits
#define GIEN                   0x02  // Gesture Interrupt Enable

// GCONFIG1 bits
#define GFIFOTH                0xC0  // Gesture FIFO Threshold (bits 7:6)
#define GEXMSK                 0x3C  // Gesture Exit Mask (bits 5:2)
#define GEXPERS                0x03  // Gesture Exit Persistence (bits 1:0)

// GCONFIG2 bits
#define GGAIN                  0x60  // Gesture Gain Control (bits 6:5)
#define GLDRIVE                0x18  // Gesture LED Drive Strength (bits 4:3)
#define GWTIME                 0x07  // Gesture Wait Time (bits 2:0)

// STATUS bits
#define PGSAT                  0x40  // Gesture Saturation

// CONFIG2 bits
#define LEDBOOST               0x30  // Gesture/Proximity LED Boost (bits 5:4)

// GPULSE bits
#define GPULSE                 0x3F  // Pulse Count (bits 5:0)
#define GPLEN                  0xC0  // Gesture Pulse Length (bits 7:6)

// GCONFIG3 bits
#define GDIMS                  0x03  // Gesture Dimension Select (bits 1:0)

// GSTATUS bits
#define GFOV                   0x02  // Gesture FIFO Overflow
#define GVALID                 0x01  // Gesture Valid

void setup() {
  Wire.begin();
  Serial.begin(9600);

  Serial.println("Initializing APDS-9960 for gesture detection...");
  if (!initAPDS9960()) {
    Serial.println("Failed to initialize APDS-9960.");
    while (1); // Halt program
  }
  Serial.println("APDS-9960 initialized successfully for gesture detection.");
}

void loop() {
  if (isGestureAvailable()) {
    String gesture = readGesture();
    Serial.print("Gesture Detected: ");
    Serial.println(gesture);
  }

  delay(100); // Adjust delay for gesture responsiveness
}

// Initialize the APDS9960 for gesture detection
bool initAPDS9960() {
  // Check device ID
  uint8_t id = readRegister(0x92);
  Serial.print("Device ID: 0x");
  Serial.println(id, HEX);
  if (id != 0xA8) {
    Serial.println("Error: Incorrect device ID");
    return false;
  }

  // Power ON and Gesture mode
  writeRegister(APDS9960_ENABLE, ENABLE_PON | ENABLE_GEN);

  // Gesture Interrupt Enable
  writeRegister(APDS9960_GCONFIG4, GIEN);

  // Gesture Proximity Entry Threshold
  writeRegister(APDS9960_GPENTH, 0x14); // Example threshold

  // Gesture Exit Threshold
  writeRegister(APDS9960_GEXTH, 0x32); // Example threshold

  // Gesture FIFO Threshold, Exit Mask, and Exit Persistence
  writeRegister(APDS9960_GCONFIG1, GFIFOTH | GEXMSK | GEXPERS);

  // Gesture Gain Control, LED Drive Strength, and Wait Time
  writeRegister(APDS9960_GCONFIG2, GGAIN | GLDRIVE | GWTIME);

  // Gesture Saturation, LED Boost
  writeRegister(APDS9960_STATUS, PGSAT);
  writeRegister(APDS9960_CONFIG2, LEDBOOST);

  // Gesture Offsets (up, down, left, right)
  writeRegister(APDS9960_GOFFSET_U, 0x00); // Example offset
  writeRegister(APDS9960_GOFFSET_D, 0x00); // Example offset
  writeRegister(APDS9960_GOFFSET_L, 0x00); // Example offset
  writeRegister(APDS9960_GOFFSET_R, 0x00); // Example offset

  // Gesture Pulse Count and Length
  writeRegister(APDS9960_GPULSE, GPULSE | GPLEN);

  // Gesture Dimension Select
  writeRegister(APDS9960_GCONFIG3, GDIMS);

  // Gesture FIFO Level
  writeRegister(APDS9960_GFLVL, 0x00); // Example FIFO level (no data in FIFO)

  return true;
}

// Check if a gesture is available
bool isGestureAvailable() {
  uint8_t gstatus = readRegister(APDS9960_GSTATUS);
  Serial.print("Gesture Status: 0x");
  Serial.println(gstatus, HEX);

  return (gstatus & GVALID) != 0;
}

// Read and interpret gesture data
String readGesture() {
  int up = readRegister(APDS9960_GFIFO_U);
  int down = readRegister(APDS9960_GFIFO_D);
  int left = readRegister(APDS9960_GFIFO_L);
  int right = readRegister(APDS9960_GFIFO_R);

  Serial.print("Gesture Data: U=");
  Serial.print(up);
  Serial.print(" D=");
  Serial.print(down);
  Serial.print(" L=");
  Serial.print(left);
  Serial.print(" R=");
  Serial.println(right);

  // Determine gesture direction based on FIFO values
  if (up > down && up > left && up > right) {
    return "UP";
  } else if (down > up && down > left && down > right) {
    return "DOWN";
  } else if (left > up && left > down && left > right) {
    return "LEFT";
  } else if (right > up && right > down && right > left) {
    return "RIGHT";
  }

  return "UNKNOWN";
}

// Write a byte to a register
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(APDS9960_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();

  Serial.print("Wrote 0x");
  Serial.print(value, HEX);
  Serial.print(" to register 0x");
  Serial.println(reg, HEX);
}

// Read a byte from a register
uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(APDS9960_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(APDS9960_I2C_ADDR, 1);

  if (Wire.available()) {
    uint8_t value = Wire.read();
    Serial.print("Read 0x");
    Serial.print(value, HEX);
    Serial.print(" from register 0x");
    Serial.println(reg, HEX);
    return value;
  }
  return 0x00; // Return 0 if no data is available
}
