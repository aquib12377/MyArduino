#include "ESP32_NOW.h"
#include "WiFi.h"
#include <ESP32Servo.h>
#include <esp_mac.h>
#include <vector>

#define ESPNOW_WIFI_CHANNEL 6

Servo servo[5];
const int servoPins[5] = {13, 12, 14, 27, 26}; // Change pins if required


  void onReceive(const uint8_t *data, size_t len, bool broadcast) {
    int emgVal = atoi((const char *)data);
    Serial.printf("Received EMG: %d\n", emgVal);

    // Servo Mapping (example):
    int angle = map(emgVal, 0, 4095, 0, 180);
    angle = constrain(angle, 0, 180);

    // Control all 5 servos based on single EMG (Adjust logic as needed)
    
  }

void smoothMoveServo(int idx, int targetAngle, int stepDelay) {
  int current = servo[idx].read();               // get its last position
  int step = (targetAngle > current) ? 1 : -1;   // decide direction
  while (current != targetAngle) {
    current += step;
    servo[idx].write(current);
    delay(stepDelay);
  }
}
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600,SERIAL_8N1,16,17);
  // Attach servos to respective pins
  for (int i = 0; i < 5; i++) {
    servo[i].attach(servoPins[i]);
    servo[i].write(0);
  }

  while(false)
  {
    for (int i = 0; i < 5; i++) {
      smoothMoveServo(i, 90, 5);  // tweak the third parameter for speed
      delay(500);
    }
    delay(1000);
  for (int i = 0; i < 5; i++) {
      smoothMoveServo(i, 0, 5);  // tweak the third parameter for speed
      delay(500);
    }
    delay(1000);
  }

  Serial.println("Receiver Ready...");
}


void loop() {
  if (Serial1.available() > 0) {
    String msg = Serial1.readStringUntil('\n');
    Serial.println(msg);

    // move all servos out to 180°, one by one, smoothly
    for (int i = 0; i < 5; i++) {
      smoothMoveServo(i, 110, 10);  // tweak the third parameter for speed
    }

    delay(5000);

    // then back to 0°, one by one, smoothly
    for (int i = 0; i < 5; i++) {
      smoothMoveServo(i, 0, 10);
    }

    delay(5000);
  }
}
