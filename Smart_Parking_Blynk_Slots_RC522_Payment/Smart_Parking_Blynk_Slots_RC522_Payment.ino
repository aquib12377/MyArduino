#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3-tU8wLG4"
#define BLYNK_TEMPLATE_NAME "Smart Parking"
#define BLYNK_AUTH_TOKEN "RPF2KkeGZ2AEoQfTf_z6qZbiFBMZdud1"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define RST_PIN 4  // Reset pin for RFID
#define SS_PIN 5   // Slave select pin for RFID
#define IR_SENSOR_1 34
#define IR_SENSOR_2 35
#define IR_SENSOR_3 32
#define IR_SENSOR_4 33
#define IR_SENSOR_5 26
#define SERVO_PIN 25

Servo gateServo;
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD address 0x27 with 16 columns and 2 rows

// Blynk auth token and WiFi credentials
char ssid[] = "MyProject";
char pass[] = "12345678";

// Data structure for vehicle
struct Vehicle {
  String uid;
  int balance = 100;
  bool isParked = false;
};

Vehicle vehicles[3];

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  WiFi.begin(ssid, pass);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Setup IR sensors as input
  pinMode(IR_SENSOR_1, INPUT_PULLUP);
  pinMode(IR_SENSOR_2, INPUT_PULLUP);
  pinMode(IR_SENSOR_3, INPUT_PULLUP);
  pinMode(IR_SENSOR_4, INPUT_PULLUP);
  pinMode(IR_SENSOR_5, INPUT_PULLUP);

  // Attach servo and set initial position
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // Gate closed

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking");
  delay(2000);
  lcd.clear();
}

void loop() {
  Blynk.run();
  handleRFID();
  updateSlotStatus();
}

void handleRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("RFID Card Scanned: " + uid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card: " + uid);

  for (int i = 0; i < 3; i++) {
    if (vehicles[i].uid == uid || vehicles[i].uid == "") {
      if (vehicles[i].uid == "") {
        vehicles[i].uid = uid;
        vehicles[i].isParked = true;
        vehicles[i].balance -= 10;
        Serial.println("Vehicle Entered. Balance: Rs." + String(vehicles[i].balance));
        lcd.setCursor(0, 1);
        lcd.print("Ent. Bal: Rs." + String(vehicles[i].balance));
        operateGate();
        break;
      } else {
        if (vehicles[i].isParked) {
          vehicles[i].isParked = false;
          Serial.println("Vehicle Exited. Balance: Rs." + String(vehicles[i].balance));
          lcd.setCursor(0, 1);
          lcd.print("Ext. Bal: Rs." + String(vehicles[i].balance));
        } else {
          vehicles[i].isParked = true;
          vehicles[i].balance -= 10;
          Serial.println("Vehicle Re-entered. Balance: Rs." + String(vehicles[i].balance));
          lcd.setCursor(0, 1);
          lcd.print("Ent. Bal: Rs." + String(vehicles[i].balance));
        }
        operateGate();
        break;
      }
    }
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void operateGate() {
  gateServo.write(90); // Open gate
  delay(3000);         // Wait for vehicle
  gateServo.write(0);  // Close gate
}

void updateSlotStatus() {
  int slot1Status = digitalRead(IR_SENSOR_1)==LOW;
  int slot2Status = digitalRead(IR_SENSOR_2)==LOW;
  int slot3Status = digitalRead(IR_SENSOR_3)==LOW;
  int slot4Status = digitalRead(IR_SENSOR_4)==LOW;
  int slot5Status = digitalRead(IR_SENSOR_5)==LOW;

  Blynk.virtualWrite(V0, slot1Status); // Slot 1 status
  Blynk.virtualWrite(V1, slot2Status); // Slot 2 status
  Blynk.virtualWrite(V2, slot3Status); // Slot 3 status
  Blynk.virtualWrite(V3, slot4Status); // Slot 3 status
  Blynk.virtualWrite(V4, slot5Status); // Slot 3 status

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S4:" + String(slot4Status) + " S5:" + String(slot5Status));
  lcd.setCursor(0, 1);
  lcd.print("S1:" + String(slot1Status) + " S2:" + String(slot2Status) + " S3:" + String(slot3Status));

  Serial.println("Slot 1: " + String(slot1Status) + " | Slot 2: " + String(slot2Status) + " | Slot 3: " + String(slot3Status));
  delay(1000); 
}