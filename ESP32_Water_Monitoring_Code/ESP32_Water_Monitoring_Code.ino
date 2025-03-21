/************************************************
 * ESP32 Water Reminder Project (Bluetooth Version)
 * Components:
 *   - DS18B20 (Temperature)
 *   - Ultrasonic Sensor (Water level detection)
 *   - Buzzer
 *   - BluetoothSerial for data exchange
 ************************************************/

#include <BluetoothSerial.h>     // Library for Classic Bluetooth SPP
#include <OneWire.h>            // Library for DS18B20
#include <DallasTemperature.h>  // Library for DS18B20

/************* DS18B20 Setup *************/
#define ONE_WIRE_BUS 15    // DS18B20 data pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

/************* Ultrasonic Sensor Setup *************/
#define TRIG_PIN 12
#define ECHO_PIN 14

/************* Buzzer Setup *************/
#define BUZZER_PIN 26

/************* Reminder Interval *************/
unsigned long reminderInterval = 30 * 60 * 1000UL; // Default 30 mins in milliseconds
unsigned long lastDrinkCheck = 0;
float lastWaterLevel = 0.0;

/************* BluetoothSerial *************/
BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);

  // DS18B20 initialization
  sensors.begin();

  // Ultrasonic pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize last water level
  lastWaterLevel = getWaterLevel();
  lastDrinkCheck = millis();

  // Start Bluetooth
  SerialBT.begin("ESP32_Water_Reminder"); 
  // This is the Bluetooth device name that your phone will see
  Serial.println("Bluetooth device started, visible as 'ESP32_Water_Reminder'");

  // Optionally set a pin for pairing:
  // SerialBT.setPin("1234");
}

void loop() {
  // Check if we have data from Flutter app
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    command.trim();
    handleBluetoothCommand(command);
  }

  // Periodically check if user has drunk water
  unsigned long currentMillis = millis();
  if (currentMillis - lastDrinkCheck >= reminderInterval) {
    float currentLevel = getWaterLevel();

    // If water level has decreased significantly (e.g., > 2 cm), user drank water
    if (lastWaterLevel - currentLevel > 2.0) {
      Serial.println("User drank water. No buzzer.");
      // Reset
    } else {
      Serial.println("User did NOT drink water in the interval. Buzzer ON!");
      buzzerBeep();
    }

    // Prepare for next interval
    lastWaterLevel = currentLevel;
    lastDrinkCheck = currentMillis;
  }
}

/********************************************
 * Helper functions
 ********************************************/

void handleBluetoothCommand(const String& cmd) {
  // Expecting:
  //  "SET_INTERVAL:xx"
  //  "GET_DATA"
  // Example: "SET_INTERVAL:15" (for 15 minutes)

  if (cmd.startsWith("SET_INTERVAL:")) {
    String valueStr = cmd.substring(String("SET_INTERVAL:").length());
    int minutes = valueStr.toInt();
    if (minutes > 0) {
      reminderInterval = (unsigned long)minutes * 60UL * 1000UL;
      Serial.print("Interval set to ");
      Serial.print(minutes);
      Serial.println(" minute(s).");
      SerialBT.println("OK: Interval updated");
    } else {
      SerialBT.println("ERROR: Invalid interval");
    }
  } else if (cmd == "GET_DATA") {
    float tempC = getTemperature();
    float level = map(getWaterLevel(),11,2,0,100);
    level = constrain(level, 0, 100);
    // Return JSON-like string
    // e.g. {"temperature":25.00,"waterLevel":15.20,"interval":30}
    char response[128];
    snprintf(response, sizeof(response),
             "{\"temperature\":%.2f,\"waterLevel\":%.2f,\"interval\":%lu}",
             tempC, level, reminderInterval / (60UL * 1000UL));
    SerialBT.println(response);
    Serial.println(response);

  } else {
    SerialBT.println("ERROR: Unknown command");
  }
}

float getTemperature() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

float getWaterLevel() {
  // Example ultrasonic measurement in cm
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  // Speed of sound is ~343 m/s => 0.0343 cm/us
  float distanceCm = duration * 0.0343 / 2.0; 
  // Example: bottle is 30 cm tall => water level:
  float waterLevel = 30.0 - distanceCm;
  if (waterLevel < 0) waterLevel = 0;
  if (waterLevel > 30) waterLevel = 30;

  Serial.println(waterLevel);
  return distanceCm;
}

// Buzzer beep for 2 seconds
void buzzerBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(2000);
  digitalWrite(BUZZER_PIN, LOW);
}
