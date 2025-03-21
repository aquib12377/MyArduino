/***************************************************
 * 1) Add these libraries via Arduino Library Manager:
 *    - Blynk
 *    - Wire (already included for ESP8266)
 * 2) Adjust your Blynk credentials below.
 ***************************************************/
#define BLYNK_TEMPLATE_ID "TMPL3tUoah5CR"
#define BLYNK_TEMPLATE_NAME "Energy Meter Monitoring"
#define BLYNK_AUTH_TOKEN "ugh86NOIL0l97T_PAf3IV2gYSn-Vb-Cx"
// Blynk needs the following includes
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <Wire.h>  // I2C for ESP8266
#include <Arduino.h>

// ------------------------------------------------
// Blynk Authentication & WiFi Credentials
// ------------------------------------------------
char ssid[] = "MyProject";
char pass[] = "12345678";

// We'll update these Blynk pins:
// V1 => Display for "Units"
// V2 => Display for "Cost"
//
// We'll use V0 => Button to send relay ON/OFF

// ------------------------------------------------
// BlynkTimer to poll the Arduino
// ------------------------------------------------
BlynkTimer timer;

// ------------------------------------------------
// Function: Request data from the Arduino over I2C
//           Then update Blynk with the results
// ------------------------------------------------
void requestDataFromArduino() {
  // The Arduino code sends up to 10 bytes, typically "<units>,<cost>"
  Wire.requestFrom(0x08, 10);  // slave address = 0x08

  String data = "";
  while (Wire.available()) {
    char c = Wire.read();
    data += c;
  }

  if (data.length() > 0) {
    // Typically data looks like "12,36" => 12 units, cost 36

    // Let's parse it:
    int commaIndex = data.indexOf(',');
    if (commaIndex > 0) {
      String unitsStr = data.substring(0, commaIndex);
      String costStr  = data.substring(commaIndex + 1);
      Serial.println("Units: "+String(unitsStr));
      Serial.println("Cost: "+String(costStr));
      int units = unitsStr.toInt();
      int cost  = costStr.toInt();

      // Update Blynk widgets
      Blynk.virtualWrite(V0, units); // e.g. Value Display widget
      Blynk.virtualWrite(V1, cost);  // e.g. Value Display widget
    }
  }
}

// ------------------------------------------------
// BLYNK_WRITE: Called when the widget on V0 changes
//              We assume it's a switch/button that
//              sends 1 for ON, 0 for OFF
// ------------------------------------------------
BLYNK_WRITE(V2) {
  int value = param.asInt(); // 1 => ON, 0 => OFF

  if (value == 1) {
    // Send "ON" to Arduino
    Wire.beginTransmission(0x08);
    Wire.write("ON");
    Wire.endTransmission();
    Serial.println("Sent: ON to Arduino (Relay ON).");
  } else {
    // Send "OFF" to Arduino
    Wire.beginTransmission(0x08);
    Wire.write("OFF");
    Wire.endTransmission();
    Serial.println("Sent: OFF to Arduino (Relay OFF).");
  }
}

// ------------------------------------------------
// SETUP
// ------------------------------------------------
void setup() {
  Serial.begin(115200);

  // I2C on ESP8266 typically uses SDA=GPIO4, SCL=GPIO5
  Wire.begin(4, 5);

  // Connect to Blynk Cloud
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Use a timer to regularly request data from Arduino
  // e.g. once per second
  timer.setInterval(1000L, requestDataFromArduino);

  Serial.println("ESP8266 + Blynk + I2C Master Initialized.");
}

// ------------------------------------------------
// LOOP
// ------------------------------------------------
void loop() {
  Blynk.run();    // Process Blynk events
  timer.run();    // Run our timed functions
}
