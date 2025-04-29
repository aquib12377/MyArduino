#include <SparkFun_ADXL345.h>  // ADXL345 library
#include <SoftwareSerial.h>    // For HC-05 on pins 2,3

// SoftwareSerial pins: RX = 2, TX = 3
SoftwareSerial BTSerial(2, 3);

// Four unique CS pins for ADXL345:
ADXL345 adxl1(8);  // CS on pin 6
ADXL345 adxl2(7);  // CS on pin 7
ADXL345 adxl3(6);  // CS on pin 8
ADXL345 adxl4(9);  // CS on pin 9

// Store old readings for each sensor (4 sensors => index 0..3)
int lastX[4] = {0,0,0,0};
int lastY[4] = {0,0,0,0};

// Tracks the last word we sent (to avoid spamming the same word)
String lastWord = "";

// Movement threshold for "significant" change in X or Y
const int SIGNIFICANT_CHANGE = 15;

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);

  Serial.println("ADXL345 (4 sensors, ±30 range) - 15 Distinct Word Conditions");
  BTSerial.println("HC-05 connected");

  // Initialize each ADXL345
  initADXL(adxl1);
  initADXL(adxl2);
  initADXL(adxl3);
  initADXL(adxl4);

  Serial.println("All ADXL345 sensors initialized!");
  //BTSerial.println("All ADXL345 sensors initialized!");
}

void loop() {
  // Current readings for each sensor
  int currentX[4], currentY[4];

  // 1) Read each sensor's X and Y
  readXY(adxl1, 0, currentX, currentY);
  readXY(adxl2, 1, currentX, currentY);
  readXY(adxl3, 2, currentX, currentY);
  readXY(adxl4, 3, currentX, currentY);
  Serial.println("=======================================");
  // 2) Check if ANY sensor changed significantly
  bool anySignificantChange = false;
  for (int i = 0; i < 4; i++) {
    int diffX = abs(currentX[i] - lastX[i]);
    int diffY = abs(currentY[i] - lastY[i]);
    if (diffX >= SIGNIFICANT_CHANGE || diffY >= SIGNIFICANT_CHANGE) {
      anySignificantChange = true;
      break;
    }
  }

  // 3) If there's a significant change, figure out which of 15 words to send
  if (true) {
    Serial.println("------------------------------------------------------------------------");
    // Use all 8 values in the logic
    String newWord = getFifteenWordRange(
      currentX[0], currentY[0],
      currentX[1], currentY[1],
      currentX[2], currentY[2],
      currentX[3], currentY[3]
    );

    // Only send if different from the last word we sent
      Serial.print("Significant change => Word: ");
      Serial.println(newWord);

      // Send over Bluetooth
      BTSerial.println(newWord);

      // Update lastWord
      lastWord = newWord;
  }

  // 4) Update stored values (lastX, lastY) for next loop
  for (int i = 0; i < 4; i++) {
    lastX[i] = currentX[i];
    lastY[i] = currentY[i];
  }
  delay(1000); // adjust as needed
}

/********************************************************************
  Get 1 of 15 words, checking ALL 8 sensor values.
  Each condition references (x1,y1,x2,y2,x3,y3,x4,y4).
  
  NOTE: These are just EXAMPLES to show how you might use all 8 values
        in different ways. Adjust thresholds and logic to suit your
        application (since max ±30).
********************************************************************/

bool inBetween(int val, int bound1, int bound2)
{
  return val > min(bound1, bound2) && val < max(bound1, bound2);
}


String getFifteenWordRange(int x1, int y1, int x2, int y2,
                           int x3, int y3, int x4, int y4)
{
if(inBetween(x1, 15, -15) && inBetween(x2, -15, 15)  && inBetween(x4, -15, 15) &&
inBetween(y1, 15, -15) && inBetween(y2, 15, -15)  && inBetween(y4, -15, 15) )
{
  return "Wait";
}

else if(inBetween(x1, -22, -42) && inBetween(x2, -22, -42)  && inBetween(x4, -22, -42) &&
inBetween(y1, 0, -20) && inBetween(y2, 10, -10)  && inBetween(y4, 20, -5) )
{
  return "Thank you";
}
else if(inBetween(x1, -22, -42) && inBetween(x2, -22, -42)  && inBetween(x4, -22, -35) &&
inBetween(y1, 0, 10) && inBetween(y2, 10, 0)  && inBetween(y4, 20, 0) )
{
  return "You";
}
else if(inBetween(x1, 19, -1) && inBetween(x2, -17, 3)  && inBetween(x4, -18, 2) &&
inBetween(y1, 37, 17) && inBetween(y2, 41,21)  && inBetween(y4, -7, -27) )
{
  return "Peace";
}
else if(inBetween(x1, 27, 7) && inBetween(x2, 16, -4) && inBetween(x4, 4,-16) &&
inBetween(y1, -17, -37) && inBetween(y2, -23,-43)  && inBetween(y4, 33,13) )
{
  return "Washroom";
}
else if(inBetween(x1, 43,23) && inBetween(x2, 40,20) && inBetween(x3, 37,17) && inBetween(x4, 37,17) &&
inBetween(y1, 0, 20) && inBetween(y2, -10,10) && inBetween(y3, -4, 16) && inBetween(y4, 5,-15) )
{
  return "Disagree";
}
else if(inBetween(x1, 14,-6) && inBetween(x2, 12,-8)  && inBetween(x4, -8,-29) &&
inBetween(y1, 43, 23) && inBetween(y2, 44,24) && inBetween(y4, 39,19) )
{
  return "Stop";
}
else if(inBetween(y1, 20,-5) && inBetween(y2, 22, 2)  && inBetween(y4, 25,5) &&
inBetween(x1, 30, 10) && inBetween(x2, 25,5) && inBetween(x4, 39,19) )
{
  return "Hello";
}

else if(y1 > 26 && y1 < 33 && y2 > 29 && y2 < 33 && y3 > -10 && y3 < -5 && y4 > 22 && y4 < 30)
{
  return  "Good";
}
else if(y1 > -5 && y1 < 2 && y2 > 28 && y3 >28 && y4 < 0)
{
  return  "Peace";
}
else if(y1 > 30 && y2 > 30 && y3 > 30 && y4 > 21)
{
  return  "Stop";
}
else if(y1 < -20 && y2 < -2 && y3 < -2 && y4 < -20)
{
  return  "Washroom";
}
else if(y3 > 15)
{
  return "Hello";
}
else if(y3 < -15)
{
  return "Bye";
}

else {
  return "";
}

}



/********************************************************************
  Initialize an ADXL345 sensor with your desired settings
********************************************************************/
void initADXL(ADXL345 &sensor) {
  sensor.powerOn();        
  delay(50);

  sensor.setRangeSetting(16); // Range can be 2,4,8,16g
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
  Read the X, Y, Z values from the specified ADXL345 sensor.
  We store them into currentX[index], currentY[index].
  (We ignore Z in this demonstration, but you could use it if needed.)
********************************************************************/
void readXY(ADXL345 &sensor, int index, int currentX[], int currentY[]) {
  int x, y, z;
  sensor.readAccel(&x, &y, &z);

  currentX[index] = x;
  currentY[index] = y;

  // Optional debug prints:
  
  Serial.print("Sensor #"); 
  Serial.print(index+1);
  Serial.print(", Y: "); 
  Serial.print(y);
    Serial.print(", X: "); 
  Serial.println(x);
}
