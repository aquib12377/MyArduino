/*
  This code integrates NEO-6M GPS and MPU6050 with TTGO T-Call ESP32 SIM800L module.
  It sends an SMS with location data whenever high movement indicating an accident is detected,
  or when the SOS button is pressed.

  If an accident is detected, it waits 30 seconds before sending the SMS.
  If the SOS button is pressed within those 30 seconds, the SMS is canceled.

  Ensure that the GPS module is connected to UART2 (GPIO16 and GPIO17),
  and the MPU6050 is connected to the I2C bus (GPIO21 and GPIO22).
  The SOS button is connected to GPIO13 with an internal pull-up resistor.
*/

// SIM card PIN (leave empty if not defined)
const char simPIN[] = "";

// Your phone number to send SMS: + (plus sign) and country code, followed by phone number
#define SMS_TARGET "+918433802238" // Replace with your phone number

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024    // Set RX buffer to 1Kb

#include <Wire.h>
#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include "MPU6050_light.h"  // Include the MPU6050_light library

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// GPS module pins
#define GPS_RX_PIN           16   // GPS TX connected to ESP32 RX2
#define GPS_TX_PIN           17   // GPS RX connected to ESP32 TX2

// SOS button pin
#define SOS_BUTTON_PIN       23   // Adjust based on your wiring

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// GPS serial port
#define SerialGPS Serial2

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

TinyGPSPlus gps;
MPU6050 mpu(Wire);  // Initialize MPU6050 with Wire

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setPowerBoostKeepOn(int en){
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

void sendSMS(String smsMessage) {
  if(modem.sendSMS(SMS_TARGET, smsMessage)){
    SerialMon.println("SMS sent successfully:");
    SerialMon.println(smsMessage);
  }
  else{
    SerialMon.println("SMS failed to send");
  }
}

// Global variables
long timer = 0;
bool accidentDetected = false;
unsigned long accidentDetectedTime = 0;
bool smsSent = false;

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Initialize SOS button
  pinMode(SOS_BUTTON_PIN, INPUT_PULLUP);

  // Initialize MPU6050
  byte status = mpu.begin();
  SerialMon.print(F("MPU6050 status: "));
  SerialMon.println(status);
  while(status != 0){
    SerialMon.println(F("MPU6050 connection failed. Retrying..."));
    delay(1000);
    status = mpu.begin();
  }

  SerialMon.println(F("Calculating offsets, do not move MPU6050"));
  delay(1000);
  mpu.calcOffsets(true,true); // gyro and accelero
  SerialMon.println("Done!\n");

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Initialize GPS module
  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // Restart SIM800 module
  SerialMon.println("Initializing modem...");
  modem.restart();
  // Use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }

  SerialMon.println("Setup complete");
}

void loop() {
  // Read GPS data
  while (SerialGPS.available() > 0) {
    char c = SerialGPS.read();
    gps.encode(c);
  }

  // Update MPU6050 readings
  mpu.update();

  // Get accelerometer readings in g
  float ax = mpu.getAngleX();
  float ay = mpu.getAngleY();
  float az = mpu.getAngleZ();

  // Compute total acceleration
  float acceleration = sqrt(ax * ax + ay * ay + az * az);
  const float ACCIDENT_THRESHOLD = 200; // Threshold in g units (adjust as needed)
  mpu.calcOffsets();
  // Check for high acceleration (accident detection)
  if (!accidentDetected && acceleration > ACCIDENT_THRESHOLD) {
    accidentDetected = true;
    accidentDetectedTime = millis();
    smsSent = false;
    SerialMon.println("High acceleration detected! Starting 30-second timer.");
  }

  // Check for SOS button press
  bool sosButtonPressed = (digitalRead(SOS_BUTTON_PIN) == LOW);
  if (sosButtonPressed) {
    SerialMon.println("SOS button pressed!");
    // If SOS button is pressed, send SMS immediately
   
    sendEmergencySMS();
   
    // Reset accident detection
    accidentDetected = false;
    smsSent = false;
  }

  // Handle accident SMS sending after 30 seconds if SOS button not pressed
  if (accidentDetected && !smsSent) {
    unsigned long elapsedTime = millis() - accidentDetectedTime;
    // Check if 30 seconds have passed
    if (elapsedTime >= 30000) { // 30000 milliseconds = 30 seconds
      sendEmergencySMS();
      smsSent = true;
      accidentDetected = false; // Reset accident detection after sending SMS
    } else {
      // If SOS button is pressed during countdown, cancel SMS
      if (sosButtonPressed) {
        SerialMon.println("SMS canceled due to SOS button press during countdown.");
        accidentDetected = false;
        smsSent = false;
      }
    }
  }

  // For debugging: print accelerometer readings every second
  if (millis() - timer > 1000) {
    SerialMon.print(F("Total Acceleration: "));
    SerialMon.println(acceleration);
    SerialMon.print(F("AX: "));
    SerialMon.print(ax);
    SerialMon.print(F("\tAY: "));
    SerialMon.print(ay);
    SerialMon.print(F("\tAZ: "));
    SerialMon.println(az);
    SerialMon.println(F("=====================================================\n"));
    timer = millis();
  }

  delay(100);
}

void sendEmergencySMS() {
  String smsMessage = "Emergency! ";
 modem.callNumber(SMS_TARGET);
 delay(10000);
 modem.callHangup();
 delay(2000);
 modem.callNumber("+917977845638");
 delay(10000);
 modem.callHangup();
  if (gps.location.isValid()) {
    smsMessage += "Location: http://maps.google.com/maps?q=";
    smsMessage += String(gps.location.lat(), 6);
    smsMessage += ",";
    smsMessage += String(gps.location.lng(), 6);
  } else {
    smsMessage += "https://maps.app.goo.gl/xf8V8zkFtMTiahWw6";
  }

  sendSMS(smsMessage);
}