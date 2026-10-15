#include <PZEM003_Fud.h>

// Define RX and TX pins (SoftwareSerial)
PZEM003_Fud pzem(D1, D2);  // RX, TX

void setup() {
  Serial.begin(9600);
  pzem.begin(9600);
}

void loop() {
  pzem.update();

  Serial.print("Voltage: ");
  Serial.print(pzem.getVoltage(), 2);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(pzem.getCurrent(), 2);
  Serial.println(" A");

  Serial.print("Power: ");
  Serial.print(pzem.getPower(), 1);
  Serial.println(" W");

  Serial.print("Energy: ");
  Serial.print(pzem.getEnergy(), 0);
  Serial.println(" Wh");

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "reset") {
      pzem.resetEnergy();
      Serial.println("Energy has been reset!");
    }
  }
  Serial.println(" ");
  delay(1000);
}