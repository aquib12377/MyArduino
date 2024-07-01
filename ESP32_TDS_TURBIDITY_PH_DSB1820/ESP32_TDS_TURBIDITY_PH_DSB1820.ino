#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL3uxtlyKrg"
#define BLYNK_TEMPLATE_NAME "Water Quality Monitoring"
#define BLYNK_AUTH_TOKEN "UadiioxqRsshp-XrF3Ay0XWCjpzIetF-"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// Pin definitions
#define TDS_PIN 34
#define TURBIDITY_PIN 35
#define PH_PIN 32
#define ONE_WIRE_BUS 4

char ssid[] = "MyProject";   // Your WiFi network SSID
char pass[] = "12345678";    // Your WiFi network password

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
float calibration_value = 21.34;
int phval = 0; 
unsigned long int avgval; 
int buffer_arr[10], temp;

// DS18B20 setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();

  sensors.begin();

  lcd.setCursor(0, 0);
  lcd.print("Water Quality");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring");
  delay(3000);
  lcd.clear();

  // Display WiFi connection steps
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("WiFi...");
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("      ");
  }
  Serial.println(" Connected!");
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connected");
  delay(2000);
  lcd.clear();

  // Display Blynk connection steps
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Blynk...");
  Serial.print("Connecting to Blynk");

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  while (Blynk.connected() == false) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("Blynk...");
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("      ");
  }
  Serial.println(" Connected!");
  lcd.setCursor(0, 1);
  lcd.print("Blynk Connected");
  delay(2000);
  lcd.clear();
}

void loop() {
  Blynk.run();

  // Read sensors
  int tdsValue = readTDS();
  int turbidityValue = readTurbidity();
  int pHValue = readPH();
  float temperature = readTemperature();

  // Update LCD
  lcd.setCursor(0, 0);
  lcd.print("TDS:" + String(tdsValue) + " Turb:" + String(turbidityValue));  // Print TDS value without decimals
  lcd.setCursor(30, 0);
  lcd.print(pHValue, 2);  // Print pH value with 2 decimal places

  lcd.setCursor(0, 1);
  lcd.print("pH:" + String(pHValue) + " Temp:" + String(temperature));  // Print temperature value with 2 decimal places

  // Debug to Serial Monitor
  Serial.print("TDS: ");
  Serial.print(tdsValue);
  Serial.print(" ppm, Turbidity: ");
  Serial.print(turbidityValue);
  Serial.print(" NTU, pH: ");
  Serial.print(pHValue);
  Serial.print(", Temp: ");
  Serial.print(temperature);
  Serial.println(" C");

  // Send data to Blynk
  Blynk.virtualWrite(V0, pHValue);
  Blynk.virtualWrite(V1, tdsValue);
  Blynk.virtualWrite(V2, turbidityValue);
  Blynk.virtualWrite(V3, temperature);

  delay(1000);
}

float readTDS() {
  int rawValue = analogRead(TDS_PIN);
  // Convert to TDS value using calibration factor
  float tds = (rawValue / 4096.0);  // Example conversion
  return rawValue;
}

float readTurbidity() {
  int rawValue = analogRead(TURBIDITY_PIN);
  // Convert to turbidity value using calibration factor
  float turbidity = (rawValue / 4096.0);  // Example conversion
  return rawValue;
}

float readPH() {
  float voltage = analogRead(PH_PIN) * (3.3 / 4095.0);
  float ph = (3.3 * voltage);
  Serial.println(ph);
  delay(500);
  return ph;
}

float readTemperature() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}
