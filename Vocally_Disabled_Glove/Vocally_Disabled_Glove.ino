/********************************************************************
  Example: Using 4x ADXL345 in SPI mode with different CS pins
           + HC-05 Bluetooth on SoftwareSerial pins 2,3
           + Sending keywords based on averageVal range
           + Prevent same keyword spam (5-second cooldown)

  ADXL345 Library:
    https://github.com/sparkfun/SparkFun_ADXL345_Arduino_Library

  Pins for SPI on Arduino Uno/Nano:
    MOSI -> 11
    MISO -> 12
    SCK  -> 13

  Four unique CS pins for ADXL345:
    CS1 -> 6
    CS2 -> 7
    CS3 -> 8
    CS4 -> 9

  HC-05 Bluetooth via SoftwareSerial:
    RX -> Pin 2  (Arduino receives data here from HC-05's TX)
    TX -> Pin 3  (Arduino sends data here to HC-05's RX)
  
  Baud Rates:
    - Hardware Serial (USB) at 9600
    - SoftwareSerial (HC-05) also at 9600
  
  Ranges for averageVal (example):
    Stop       : 33 to 36
    Cup        : 22 to 24
    Cookies    : -35 to -31
    Yes        : -21 to -18
    No         : -3 to 1
    Hello      : 40 to 45
    I love you : -11 to -8
********************************************************************/

#include <SparkFun_ADXL345.h>  // SparkFun ADXL345 Library
#include <SoftwareSerial.h>    // For HC-05 on pins 2,3

// SoftwareSerial pins: RX = 2, TX = 3
SoftwareSerial BTSerial(2, 3);

// Create four ADXL345 objects, each with a different CS pin
ADXL345 adxl1(6);  // CS on pin 6
ADXL345 adxl2(7);  // CS on pin 7
ADXL345 adxl3(8);  // CS on pin 8
ADXL345 adxl4(9);  // CS on pin 9

// Variables to manage anti-spam logic
String lastKeyword = "";
unsigned long lastSendTime = 0;     // Store the last time we sent a keyword
const unsigned long SPAM_DELAY = 5000; // 5 seconds delay

void setup() {
  // Initialize hardware Serial for debugging
  Serial.begin(9600);
  Serial.println("4x ADXL345 SPI + HC-05 Example with Range-Based Words and Anti-Spam");

  // Initialize SoftwareSerial for HC-05
  BTSerial.begin(9600);
  BTSerial.println("HC-05 connected. Sending ADXL345 data...");

  // Initialize each ADXL345 sensor with the same settings
  initADXL(adxl1);
  initADXL(adxl2);
  initADXL(adxl3);
  initADXL(adxl4);

  Serial.println("All ADXL345 sensors initialized!");
  BTSerial.println("All ADXL345 sensors initialized!");
}

void loop() {
  // Read from each sensor (returns an integer: x + y)
  int val1 = readAndPrint(adxl1, "Sensor1");
  int val2 = readAndPrint(adxl2, "Sensor2");
  int val3 = readAndPrint(adxl3, "Sensor3");
  int val4 = readAndPrint(adxl4, "Sensor4");

  // Compute average of the 4 returned values
  int averageVal = (val1 + val2 + val3 + val4) / 4;

  // Print the average to hardware Serial
  Serial.print("averageVal => ");
  Serial.println(averageVal);

  // Also send this data via Bluetooth
  // BTSerial.print("averageVal => ");
  // BTSerial.println(averageVal);

  // Determine which word to send based on the averageVal range
  String currentKeyword = getKeyword(averageVal);

  // Anti-spam logic: Only send if it's a new keyword OR 5 seconds have passed
  if (currentKeyword != lastKeyword && currentKeyword != "Unknown") {
    // Different keyword - send immediately
    sendKeyword(currentKeyword);
    lastKeyword = currentKeyword;
    lastSendTime = millis();
  } else {
    // Same keyword - check if 5 seconds have passed
    if (millis() - lastSendTime >= SPAM_DELAY && currentKeyword != "Unknown") {
      sendKeyword(currentKeyword);
      lastSendTime = millis();
    }
  }

  Serial.println("--------------------------------");
  //BTSerial.println("--------------------------------");
  delay(500);
}

/********************************************************************
  Helper function to print and send the keyword over Serial and BT
********************************************************************/
void sendKeyword(const String &word) {
  Serial.print("Keyword => ");
  Serial.println(word);

  //BTSerial.print("Keyword => ");
  BTSerial.println(word);
}

/********************************************************************
  Function to choose a word/phrase based on an integer value.
  Adjust the ranges to your needs.
********************************************************************/
String getKeyword(int value) {
  // Check the ranges (example values):
  // Stop       : 33 to 36
  // Cup        : 22 to 24
  // Cookies    : -35 to -31
  // Yes        : -21 to -18
  // No         : -3 to 1
  // Hello      : 40 to 45
  // I love you : -11 to -8

  if(value >= 33 && value <= 36) {
    return "Stop";
  }
  else if(value >= 22 && value <= 24) {
    return "Cup";
  }
  else if(value >= -35 && value <= -31) {
    return "Cookies";
  }
  else if(value >= -21 && value <= -18) {
    return "Yes";
  }
  else if(value >= -3 && value <= 1) {
    return "No";
  }
  else if(value >= 40 && value <= 45) {
    return "Hello";
  }
  else if(value >= -11 && value <= -8) {
    return "I love you";
  }
  else {
    // If none matched, return something else
    return "Unknown";
  }
}

/********************************************************************
  Function to initialize an ADXL345 object with desired settings.
********************************************************************/
void initADXL(ADXL345 &sensor) {
  sensor.powerOn();        
  delay(50);

  sensor.setRangeSetting(16); // Range can be 2, 4, 8, or 16g
  sensor.setSpiBit(0);        // 4-wire SPI

  // Optional feature configuration
  sensor.setActivityXYZ(1, 0, 0);  // Activity detection on X only
  sensor.setActivityThreshold(75); // 62.5 mg/LSB (~4.7 g)

  sensor.setInactivityXYZ(1, 0, 0);
  sensor.setInactivityThreshold(75);
  sensor.setTimeInactivity(10);

  sensor.setTapDetectionOnXYZ(0, 0, 1);
  sensor.setTapThreshold(50);
  sensor.setTapDuration(15);
  sensor.setDoubleTapLatency(80);
  sensor.setDoubleTapWindow(200);

  sensor.setFreeFallThreshold(7);
  sensor.setFreeFallDuration(30);

  sensor.InactivityINT(1);
  sensor.ActivityINT(1);
  sensor.FreeFallINT(1);
  sensor.doubleTapINT(1);
  sensor.singleTapINT(1);
}

/********************************************************************
  Function to read the X, Y, Z values from an ADXL345 and print them.
  Returns an integer: x + y (per your original design).
********************************************************************/
int readAndPrint(ADXL345 &sensor, const char* label) {
  int x, y, z;
  sensor.readAccel(&x, &y, &z);

  // Convert raw readings to "g" units (optional debug)
  const float scaleFactor = 0.0039;  
  float ax = x * scaleFactor;
  float ay = y * scaleFactor;
  float az = z * scaleFactor;
  float magnitude = sqrt(ax*ax + ay*ay + az*az);

  // Return x+y as your "computedVal"
  int computedVal = x + y;

  // Print raw values + computedVal
  Serial.print(label);
  Serial.print(" | Raw(X,Y,Z): ");
  Serial.print(x); Serial.print(", ");
  Serial.print(y); Serial.print(", ");
  Serial.print(z);
  Serial.print(" | Magnitude: ");
  Serial.print(magnitude, 3);
  Serial.print(" g | ComputedVal(x+y): ");
  Serial.println(computedVal);

  return computedVal;
}
