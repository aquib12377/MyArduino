#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
// HX711 circuit wiring for Load Cell 1
const int LOADCELL1_DOUT_PIN = 16;
const int LOADCELL1_SCK_PIN = 4;
const String PHONE = "+919324419285";

// HX711 circuit wiring for Load Cell 2
const int LOADCELL2_DOUT_PIN = 17;  // Adjusted for second load cell
const int LOADCELL2_SCK_PIN = 5;

// Servo motor pin
const int SERVO_PIN = 13;

// Ultrasonic sensor pins
const int ULTRASONIC1_TRIG_PIN = 12;
const int ULTRASONIC1_ECHO_PIN = 14;
const int ULTRASONIC2_TRIG_PIN = 27;
const int ULTRASONIC2_ECHO_PIN = 26;

// Buzzer pin
const int BUZZER_PIN = 25;

#define rxPin 15
#define txPin 2
HardwareSerial sim800(1);

HX711 scale1;        // Instance for Load Cell 1
HX711 scale2;        // Instance for Load Cell 2
Servo loadBalancer;  // Servo instance

const float LOAD_THRESHOLD = 1000.0;      // Maximum load in grams before redistribution
const int SERVO_POSITION_LEFT = 110;      // Servo position to shift load to Load Cell 1
const int SERVO_POSITION_RIGHT = 70;      // Servo position to shift load to Load Cell 2
const int SERVO_NEUTRAL_POSITION = 90;    // Neutral position of the servo (balanced state)
const int CRACK_DISTANCE_THRESHOLD = 10;  // Distance threshold in cm to detect crack

void setup() {
  Serial.begin(115200);

  // Set CPU frequency to 80 MHz for reliable HX711 communication
  rtc_cpu_freq_config_t config;
  rtc_clk_cpu_freq_get_config(&config);
  rtc_clk_cpu_freq_mhz_to_config(80000000, &config);
  rtc_clk_cpu_freq_set_config_fast(&config);

  // Initialize GSM
  sim800.begin(9600, SERIAL_8N1, rxPin, txPin);
  Serial.println("SIM800L serial initialize");
  delay(500);
  sim800.println("AT+CMGF=1");  //SMS text mode
  delay(500);
  sim800.println("AT+CMGD=1,4");  //delete all saved SMS
  delay(500);

  sim800.print("AT+CMGF=1\r");
  delay(500);
  sim800.print("AT+CMGS=\"" + PHONE + "\"\r");
  delay(500);
  sim800.print("Project Started..");
  delay(100);
  sim800.write(0x1A);  //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
  delay(500);
  //updateSerial();

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AI Based Railway");
  lcd.setCursor(0, 1);
  lcd.print("Wagon");

  // Initialize Load Cell 1
  scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
  scale1.set_scale(-102169 / 192);  // Calibration factor for Load Cell 1
  scale1.tare();                    // Reset scale to 0
  Serial.println("Scale 1 set");
  // Initialize Load Cell 2
  scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
  scale2.set_scale(-78938 / 192);  // Calibration factor for Load Cell 2
  scale2.tare();                   // Reset scale to 0
  Serial.println("Scale 2 set");
  // Initialize the servo motor
  loadBalancer.attach(SERVO_PIN);
  loadBalancer.write(SERVO_NEUTRAL_POSITION);  // Set servo to neutral position
  Serial.println("Servo set");
  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize ultrasonic sensors
  pinMode(ULTRASONIC1_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC1_ECHO_PIN, INPUT);
  pinMode(ULTRASONIC2_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC2_ECHO_PIN, INPUT);
  Serial.println("Pins set");
  Serial.println("Setup complete");
  delay(3000);
  lcd.clear();
}

void loop() {
  // Read values from both load cells
  float load1 = scale1.get_units(10);
  float load2 = scale2.get_units(10);

  // Display the readings
  Serial.print("Load Cell 1: ");
  Serial.print(load1, 1);
  Serial.print(" grams\t| Load Cell 2: ");
  Serial.println(load2, 1);

  // Redistribution logic
  if (load1 > LOAD_THRESHOLD && load2 < LOAD_THRESHOLD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Load is Un-balanced");
    Serial.println("Load Cell 1 exceeds threshold, shifting load to Load Cell 2");
    alertLoadDisbalance();
    for (int pos = SERVO_NEUTRAL_POSITION; pos >= SERVO_POSITION_RIGHT; pos -= 1) {  // goes from 180 degrees to 0 degrees
      loadBalancer.write(pos);                                                   // tell servo to go to position in variable 'pos'
      delay(15);                                                                 // waits 15ms for the servo to reach the position
    }                                                                            // Shift load to Load Cell 2
  } else if (load2 > LOAD_THRESHOLD && load1 < LOAD_THRESHOLD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Load is Un-balanced");
    Serial.println("Load Cell 2 exceeds threshold, shifting load to Load Cell 1");
    for (int pos = SERVO_NEUTRAL_POSITION; pos <= SERVO_POSITION_LEFT; pos += 1) {  // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      loadBalancer.write(pos);  // tell servo to go to position in variable 'pos'
      delay(15);                // waits 15ms for the servo to reach the position
    }                           // Shift load to Load Cell 1
    alertLoadDisbalance();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Load is balanced");
    Serial.println("Load is balanced");
    loadBalancer.write(SERVO_NEUTRAL_POSITION);  // Keep servo in neutral position
  }

  // Ultrasonic sensor 1 for crack detection
  int distance1 = getUltrasonicDistance(ULTRASONIC1_TRIG_PIN, ULTRASONIC1_ECHO_PIN);
  int distance2 = getUltrasonicDistance(ULTRASONIC2_TRIG_PIN, ULTRASONIC2_ECHO_PIN);

  if (distance1 <= CRACK_DISTANCE_THRESHOLD) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Crack Detected");
    Serial.println("Crack detected by Sensor 1!");
    alertCrack();
  } else if (distance2 <= CRACK_DISTANCE_THRESHOLD) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Crack Detected");
    Serial.println("Crack detected by Sensor 2!");
    alertCrack();
  } else {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("No Crack Detected");
  }

  // Power down HX711 to save power
  scale1.power_down();
  scale2.power_down();
  delay(1000);
  scale1.power_up();
  scale2.power_up();
}

// Function to get distance from ultrasonic sensor
int getUltrasonicDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;  // Convert duration to distance in cm
  Serial.print("Distance: ");
  Serial.println(distance);
  return distance;
}

// Function to alert crack detection
void alertLoadDisbalance() {
  sim800.println("AT+CMGF=1");  //SMS text mode
  delay(500);
  sim800.println("AT+CMGD=1,4");  //delete all saved SMS
  delay(500);

  sim800.print("AT+CMGF=1\r");
  delay(500);
  sim800.print("AT+CMGS=\"" + PHONE + "\"\r");
  delay(500);
  sim800.print("Load Un-Balanced.\nhttps://maps.app.goo.gl/m1jmufNcCUhY1TPt7");
  delay(100);
  sim800.write(0x1A);  //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
  delay(500);
}

void alertCrack() {
  digitalWrite(BUZZER_PIN, HIGH);  // Turn on buzzer
  delay(500);                      // Beep for 1 second
  digitalWrite(BUZZER_PIN, LOW);   // Turn off buzzer

  sim800.println("AT+CMGF=1");  //SMS text mode
  delay(500);
  sim800.println("AT+CMGD=1,4");  //delete all saved SMS
  delay(500);

  sim800.print("AT+CMGF=1\r");
  delay(500);
  sim800.print("AT+CMGS=\"" + PHONE + "\"\r");
  delay(500);
  sim800.print("Crack detected! Please check the system.\nhttps://maps.app.goo.gl/m1jmufNcCUhY1TPt7");
  delay(100);
  sim800.write(0x1A);  //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
  delay(500);
}
