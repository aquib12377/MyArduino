#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3LgIp3Fzi"
#define BLYNK_TEMPLATE_NAME "Speed Monitoring and Control"
#define BLYNK_AUTH_TOKEN "dJEERBKq_WGUuQPzpoJmq-09q37PXbCj"

#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Blynk credentials
char ssid[] = "MyProject";        // Replace with your WiFi SSID
char pass[] = "12345678";         // Replace with your WiFi Password

// Pin Definitions
#define motorPin 13      // Motor PWM pin
#define irSensorPin 14    // IR sensor pin for measuring speed

// LCD address and dimensions
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address of your LCD (0x27 is common)

// Variables for motor speed
volatile int pulseCount = 0;
float motorSpeed = 0;
unsigned long lastTime = 0;

BlynkTimer timer;

// Function to count pulses from the IR sensor
void IR_sensor_ISR() {
  pulseCount++;
}

// Function to calculate motor speed (in RPM) based on IR sensor pulses
void calculateMotorSpeed() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - lastTime;

  // Calculate RPM based on pulses in a given time frame
  motorSpeed = (pulseCount / 20.0) * (60000.0 / elapsedTime); // Adjust as per your encoder specs
  pulseCount = 0;
  lastTime = currentTime;

  // Send motor speed to Blynk
  Blynk.virtualWrite(V0, motorSpeed);

  // Display motor speed on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Motor Speed:");
  lcd.setCursor(0, 1);
  lcd.print(motorSpeed);
  lcd.print(" RPM");
}

// Function to control motor speed using a slider in Blynk
BLYNK_WRITE(V1) {
  int sliderValue = param.asInt();  // Get the slider value
  int motorPWM = map(sliderValue, 0, 300, 0, 255);  // Map the slider value to PWM range
  Serial.println("Speed: "+String(motorPWM));
  analogWrite(motorPin, motorPWM);  // Set motor speed
}

void setup() {
  // Setup Serial monitor
  Serial.begin(115200);

  // Setup LCD
  lcd.begin();              // Initialize the LCD
  lcd.backlight();         // Turn on the backlight

  // Setup IR sensor and motor pins
  pinMode(irSensorPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);

  // Attach interrupt for the IR sensor
  attachInterrupt(digitalPinToInterrupt(irSensorPin), IR_sensor_ISR, FALLING);

  // Connect to Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup a timer to calculate motor speed every second
  timer.setInterval(1000L, calculateMotorSpeed);
}

void loop() {
  Blynk.run();
  timer.run();
}
