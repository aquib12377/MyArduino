#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3M2lCHRdN"
#define BLYNK_TEMPLATE_NAME "ESP32 Bot"
#define BLYNK_AUTH_TOKEN "yRZBIkEU0rLT0dLcPzWzroh93rb7KHka"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "MyProject";   // Your WiFi network SSID
char pass[] = "12345678";    // Your WiFi network password

#define IN1 12  // Input pin 1 for motor A
#define IN2 14  // Input pin 2 for motor A
#define EN1 13  // Enable pin for motor A
#define IN3 27  // Input pin 1 for motor B
#define IN4 26  // Input pin 2 for motor B
#define EN2 25  // Enable pin for motor B

int motorSpeed = 255; // Default speed value

void setup() {
  Serial.begin(115200);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(EN1, OUTPUT);
  pinMode(EN2, OUTPUT);
}

void loop() {
  Blynk.run();
  delay(500);
}

BLYNK_WRITE(V1) {  // Virtual pin for controlling the car's forward motion
  Serial.println("Forward");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(EN1, motorSpeed);  // Set speed for motor A
  analogWrite(EN2, motorSpeed);  // Set speed for motor B
}

BLYNK_WRITE(V2) {  // Virtual pin for controlling the car's backward motion
  Serial.println("Backward");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(EN1, motorSpeed);  // Set speed for motor A
  analogWrite(EN2, motorSpeed);  // Set speed for motor B
}

BLYNK_WRITE(V3) {  // Virtual pin for controlling the car's left turn
  Serial.println("Left");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(EN1, motorSpeed);  // Set speed for motor A
  analogWrite(EN2, motorSpeed);  // Set speed for motor B
}

BLYNK_WRITE(V0) {  // Virtual pin for controlling the car's right turn
  Serial.println("Right");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(EN1, motorSpeed);  // Set speed for motor A
  analogWrite(EN2, motorSpeed);  // Set speed for motor B
}

BLYNK_WRITE(V5) {  // Virtual pin for speed control slider
  motorSpeed = param.asInt();  // Get the slider value and set it as motor speed
  Serial.print("Motor speed set to: ");
  Serial.println(motorSpeed);
}
