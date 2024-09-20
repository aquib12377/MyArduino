#include <SoftwareSerial.h>
SoftwareSerial mySerial(2, 3);  // RX, TX for HC-05

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  Serial.println("HC-05 Bluetooth module initialized...");
}

void loop() {
  String data = receiveBluetoothData();

}

String receiveBluetoothData() {
  mySerial.listen();
  String data = "";
  delay(500);
  if (mySerial.available() > 0) {
    data = mySerial.readString();
    data.trim();
    mySerial.println("Received Data : " + data);
    Serial.println(data);
  }
  Serial.flush();
  mySerial.flush();
  return data;
}
