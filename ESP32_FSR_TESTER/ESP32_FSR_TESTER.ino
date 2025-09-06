// ESP32 - Read 6 FSR Sensors, Map Values to 0-100 with Averaging

#define FSR1_PIN 33
#define FSR2_PIN 39
#define FSR3_PIN 34
#define FSR4_PIN 35
#define FSR5_PIN 32
#define FSR6_PIN 36 // New sensor

int readFSR(int pin) {
  long sum = 0;
  const int samples = 10; // number of samples for averaging
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(200);
  }
  int avgValue = sum / samples;

  // Map to 0-100 but clamp to avoid >100
  int mapped = map(avgValue, 0, 4095, 0, 100);
  if (mapped > 100) mapped = 100;
  if (mapped < 0) mapped = 0;

  return mapped;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("FSR Sensor Reading with Averaging...");
}

void loop() {
  int fsr1 = readFSR(FSR1_PIN);
  int fsr2 = readFSR(FSR2_PIN);
  int fsr3 = readFSR(FSR3_PIN);
  int fsr4 = readFSR(FSR4_PIN);
  int fsr5 = readFSR(FSR5_PIN);
  int fsr6 = readFSR(FSR6_PIN); // Read new sensor

  Serial.print("FSR1: "); Serial.print(fsr1);
  Serial.print("\tFSR2: "); Serial.print(fsr2);
  Serial.print("\tFSR3: "); Serial.print(fsr3);
  Serial.print("\tFSR4: "); Serial.print(fsr4);
  Serial.print("\tFSR5: "); Serial.print(fsr5);
  Serial.print("\tFSR6: "); Serial.println(fsr6);

  delay(200);
}
