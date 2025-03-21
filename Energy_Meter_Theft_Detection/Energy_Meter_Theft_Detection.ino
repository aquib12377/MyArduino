#include <Wire.h>             // For I2C
#include <EEPROM.h>
#include <SoftwareSerial.h>

// Pin for the relay control
#define RELAY_PIN 8

// GSM module on pins 10 (Rx) and 11 (Tx)
SoftwareSerial GSM(10, 11);

// Meter pulse pin
#define PULSE_IN 2

// Variables for storing total units
int unt_a = 0, unt_b = 0, unt_c = 0, unt_d = 0;
long total_unt = 0;

// Pulse counter before decrementing one unit
int pulseCount = 0;

// Forward declarations
void readUnitsFromEEPROM();
void writeUnitsToEEPROM();
void initGSM();
void checkForIncomingData();
void sendSMS(String number, String msg);
void pulseISR();

// ------------------------------------------------------------------------
// SETUP
// ------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);

  // Relay pin as output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay off initially

  // Meter pulse pin
  pinMode(PULSE_IN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PULSE_IN), pulseISR, RISING);

  // Read from EEPROM
  pulseCount = EEPROM.read(10);
  readUnitsFromEEPROM();

  // Init GSM module
  GSM.begin(9600);
  initGSM();

  // I2C SLAVE: Address set to 0x08, for example
  Wire.begin(0x08);
  // When the master requests data, call requestEvent()
  Wire.onRequest(requestEvent);
  // When the master sends data, call receiveEvent()
  Wire.onReceive(receiveEvent);

  // Optionally send an SMS
  sendSMS("+918591399832", "Project Started - Energy Meter with Theft Detection.");

  Serial.println("Meter Reading, I2C, and Call/SMS Handling Initialized.");
}

// ------------------------------------------------------------------------
// LOOP
// ------------------------------------------------------------------------
void loop() {
  // Continuously check for incoming calls/SMS
  checkForIncomingData();

  // No need to do anything special for I2C here; onRequest/onReceive do the work
  delay(1000);
}

// ------------------------------------------------------------------------
// INTERRUPT: Triggered on rising edge of PULSE_IN
// ------------------------------------------------------------------------
void pulseISR() {
  // If there's a rising edge on the PULSE_IN pin
  if (digitalRead(PULSE_IN) == HIGH) {
    pulseCount++;
    // Every 10 pulses => minus 1 unit
    if (pulseCount > 9) {
      pulseCount = 0;
      if (total_unt > 0) {
        total_unt--;
      }
      // Update EEPROM
      writeUnitsToEEPROM();
      readUnitsFromEEPROM();
    }
    // Save current pulse count
    EEPROM.write(10, pulseCount);
  }
}

// ------------------------------------------------------------------------
// I2C HANDLER #1: Master requests data => send "units, cost"
// ------------------------------------------------------------------------
void requestEvent() {
  // Calculate cost
  long cost = total_unt * 3;
  
  // Create a simple comma-delimited string: "<units>,<cost>"
  // Example: "12,36"
  String data = String(total_unt) + "," + String(cost);

  // Send it as raw text
  Wire.write(data.c_str());
}

// ------------------------------------------------------------------------
// I2C HANDLER #2: Master sends data => possibly "ON" or "OFF"
// ------------------------------------------------------------------------
void receiveEvent(int howMany) {
  String command = "";
  
  // Read all bytes available
  while (Wire.available() > 0) {
    char c = Wire.read();
    command += c;
  }
  command.trim();

  // If the master sent "ON", turn relay ON; if "OFF", turn relay OFF.
  if (command.equalsIgnoreCase("ON")) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Relay turned ON (via I2C).");
  } 
  else if (command.equalsIgnoreCase("OFF")) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Relay turned OFF (via I2C).");
  }
}

// ------------------------------------------------------------------------
// GSM Initialization
// ------------------------------------------------------------------------
void initGSM() {
  GSM.println("AT");
  delay(1000);

  GSM.println("ATE1");  // Echo ON
  delay(1000);

  GSM.println("AT+CPIN?");  // Check SIM card
  delay(1000);

  GSM.println("AT+CMGF=1"); // SMS in text mode
  delay(1000);

  // +CNMI=2,2 => Direct SMS display to serial
  GSM.println("AT+CNMI=2,2,0,0,0");
  delay(1000);

  // Enable Caller ID
  GSM.println("AT+CLIP=1");
  delay(1000);
}

// ------------------------------------------------------------------------
// Checks for any incoming data from GSM module: calls or SMS
// ------------------------------------------------------------------------
void checkForIncomingData() {
  static String incomingLine = "";
  static String lastSenderNumber = "";  // for SMS replies

  while (GSM.available() > 0) {
    char c = GSM.read();
    incomingLine += c;
    Serial.print(c);

    if (c == '\n') {
      incomingLine.trim();

      // If we get a line starting with +CLIP:, it's an incoming call
      if (incomingLine.startsWith("+CLIP:")) {
        // Extract caller’s number
        int firstQuote = incomingLine.indexOf('"');
        int secondQuote = incomingLine.indexOf('"', firstQuote + 1);
        if (firstQuote >= 0 && secondQuote > firstQuote) {
          String callerNumber = incomingLine.substring(firstQuote + 1, secondQuote);

          // Build a "bill" message
          String msg = "Units consumed = " + String(total_unt);
          msg += "\nTotal cost = ";
          msg += String(total_unt * 3);
          sendSMS(callerNumber, msg);

          // Optionally hang up
          // GSM.println("ATH");
        }
      }
      // If we see +CMT:, it’s an SMS header
      else if (incomingLine.startsWith("+CMT:")) {
        int firstQuote = incomingLine.indexOf('"');
        int secondQuote = incomingLine.indexOf('"', firstQuote + 1);
        if (firstQuote >= 0 && secondQuote > firstQuote) {
          lastSenderNumber = incomingLine.substring(firstQuote + 1, secondQuote);
        }
      }
      // If the next line is the SMS body, check if it says "Send Bill"
      else if (incomingLine.indexOf("Send Bill") >= 0) {
        String msg = "Units consumed = " + String(total_unt);
        msg += "\nTotal cost = ";
        msg += String(total_unt * 3);
        sendSMS(lastSenderNumber, msg);
      }
      incomingLine = "";
    }
  }
}

// ------------------------------------------------------------------------
// Send an SMS to a given number
// ------------------------------------------------------------------------
void sendSMS(String number, String msg) {
  GSM.print("AT+CMGS=\"");
  GSM.print(number);
  GSM.println("\"");
  delay(1000);

  GSM.println(msg);
  delay(500);

  // End message with CTRL+Z
  GSM.write(26);
  delay(5000);
}

// ------------------------------------------------------------------------
// Read total units from EEPROM
// ------------------------------------------------------------------------
void readUnitsFromEEPROM() {
  unt_a = EEPROM.read(1);
  unt_b = EEPROM.read(2);
  unt_c = EEPROM.read(3);
  unt_d = EEPROM.read(4);

  total_unt = unt_d * 1000 + unt_c * 100 + unt_b * 10 + unt_a;
}

// ------------------------------------------------------------------------
// Write total units to EEPROM
// ------------------------------------------------------------------------
void writeUnitsToEEPROM() {
  unt_d = total_unt / 1000;
  total_unt = total_unt - (unt_d * 1000);

  unt_c = total_unt / 100;
  total_unt = total_unt - (unt_c * 100);

  unt_b = total_unt / 10;
  unt_a = total_unt - (unt_b * 10);

  EEPROM.write(1, unt_a);
  EEPROM.write(2, unt_b);
  EEPROM.write(3, unt_c);
  EEPROM.write(4, unt_d);
}
