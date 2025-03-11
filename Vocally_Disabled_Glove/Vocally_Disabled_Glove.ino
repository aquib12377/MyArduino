#include <SparkFun_ADXL345.h>  // ADXL345 library
#include <SoftwareSerial.h>    // For HC-05 on pins 2,3

// SoftwareSerial pins: RX = 2, TX = 3
SoftwareSerial BTSerial(2, 3);

// Four unique CS pins for ADXL345:
ADXL345 adxl1(6);  // CS on pin 6
ADXL345 adxl2(7);  // CS on pin 7
ADXL345 adxl3(8);  // CS on pin 8
ADXL345 adxl4(9);  // CS on pin 9

// Track the last average X and Y to detect significant changes
int lastX = 0;
int lastY = 0;

// Threshold for “significant” movement
const int SIGNIFICANT_CHANGE = 10;

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);

  Serial.println("ADXL345 X/Y-Only with Significant Change Printing");
  BTSerial.println("HC-05 connected");

  // Initialize each ADXL345
  initADXL(adxl1);
  initADXL(adxl2);
  initADXL(adxl3);
  initADXL(adxl4);

  Serial.println("All ADXL345 sensors initialized!");
  BTSerial.println("All ADXL345 sensors initialized!");
}

void loop() {
  // 1) Read X and Y from each sensor
  int x1, y1;
  readAndPrintXY(adxl1, "Sensor1", x1, y1);

  int x2, y2;
  readAndPrintXY(adxl2, "Sensor2", x2, y2);

  int x3, y3;
  readAndPrintXY(adxl3, "Sensor3", x3, y3);

  int x4, y4;
  readAndPrintXY(adxl4, "Sensor4", x4, y4);

  // 2) Compute average X and average Y
  int averageX = (x1 + x2 + x3 + x4) / 4;
  int averageY = (y1 + y2 + y3 + y4) / 4;

  // 3) Check for a significant change from the last reading
  int diffX = abs(averageX - lastX);
  int diffY = abs(averageY - lastY);

  // Only print if there is a significant change in X or Y
  if (diffX >= SIGNIFICANT_CHANGE || diffY >= SIGNIFICANT_CHANGE) {
    // Print the average only if there's a big enough jump
    Serial.print("Significant change detected!  ");
    Serial.print("Average X = ");
    Serial.print(averageX);
    Serial.print(", Average Y = ");
    Serial.println(averageY);

    // (Optional) Send via Bluetooth, if needed
    BTSerial.print("Significant change! X = ");
    BTSerial.print(averageX);
    BTSerial.print(", Y = ");
    BTSerial.println(averageY);
  }

  // Update lastX and lastY with current values
  lastX = averageX;
  lastY = averageY;

  Serial.println("--------------------------------------------------");
  delay(1000);
}

/********************************************************************
  Function to initialize the ADXL345 with your desired settings
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
  Function to read the X, Y, Z values from an ADXL345 and return X/Y.
  We still read Z (for completeness) but ignore it in the average.
********************************************************************/
void readAndPrintXY(ADXL345 &sensor, const char* label, int &xOut, int &yOut) {
  int x, y, z;
  sensor.readAccel(&x, &y, &z);

  // (Optional) Debug print for each sensor
  // Remove or comment out if you don’t want individual sensor prints
  // Serial.print(label);
  // Serial.print(" => X: "); 
  // Serial.print(x);
  // Serial.print(", Y: "); 
  // Serial.print(y);
  // Serial.print(", Z: "); 
  // Serial.println(z);

  // Pass back X and Y to caller
  xOut = x;
  yOut = y;
}
