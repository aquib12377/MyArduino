/************** Include Necessary Libraries **************/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

/************** Define Constants **************/
// WiFi credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Create an ESP8266 Web Server on port 80
ESP8266WebServer server(80);

// DHT11 Sensor
#define DHTPIN 14        // GPIO 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Ultrasonic Sensor
#define TRIG_PIN 12      // GPIO 12
#define ECHO_PIN 13      // GPIO 13

// Flame Sensor (Active LOW)
#define FLAME_SENSOR_PIN 16  // GPIO 16

// Gas Sensor (Analog Input)
#define GAS_SENSOR_PIN A0    // Analog Pin A0

// Buzzer
#define BUZZER_PIN 15        // GPIO 15

// Relays (Active LOW)
#define RELAY1_PIN 2         // GPIO 2
#define RELAY2_PIN 0         // GPIO 0

// I2C LCD (SDA and SCL)
// SDA: D2 (GPIO 4)
// SCL: D1 (GPIO 5)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust the I2C address if necessary

/************** Variables **************/
bool relay1State = false;
bool relay2State = false;

float temperature = 0.0;
float humidity = 0.0;
long duration;
int distance;

bool flameDetected = false;
bool gasDetected = false;
int gasValue = 0;  // Variable to store gas sensor analog value

/************** Function Prototypes **************/
void handleRoot();
void handleRelay1();
void handleRelay2();
void updateSensorData();
void checkAlarms();
void buzzAlert(int times);
void displayLCD();
void setupPins();

/************** Setup Function **************/
void setup() {
  Serial.begin(115200);
  
  // Initialize I2C for LCD
  Wire.begin(4, 5); // SDA = D2 (GPIO 4), SCL = D1 (GPIO 5)
  lcd.begin();
  lcd.backlight();

  // Initialize sensors
  dht.begin();
  
  setupPins();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connecting to WiFi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected! IP Address: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
  }

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/relay1", handleRelay1);
  server.on("/relay2", handleRelay2);
  server.begin();
  Serial.println("HTTP server started");
}

/************** Loop Function **************/
void loop() {
  server.handleClient();
  updateSensorData();
  checkAlarms();
  displayLCD();
  delay(1000); // Update every second
}

/************** Setup Pins Function **************/
void setupPins() {
  // Ultrasonic Sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Flame Sensor
  pinMode(FLAME_SENSOR_PIN, INPUT_PULLUP);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Relays
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, HIGH); // Turn off relay (Active LOW)
  digitalWrite(RELAY2_PIN, HIGH); // Turn off relay (Active LOW)
}

/************** Handle Root Function **************/
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>ESP8266 Sensor Dashboard</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='2'>"; // Auto-refresh every 2 seconds
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f2f2f2; margin: 0; padding: 0; }";
  html += ".container { width: 90%; margin: 0 auto; max-width: 800px; padding: 20px; }";
  html += "h1 { text-align: center; color: #333; }";
  html += ".card { background-color: #fff; padding: 20px; margin-bottom: 20px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += ".card h2 { margin-top: 0; }";
  html += ".card p { font-size: 18px; margin: 10px 0; }";
  html += ".status { font-weight: bold; color: #007bff; }";
  html += ".button { display: inline-block; padding: 10px 20px; font-size: 16px; margin: 5px 0; cursor: pointer; text-align: center; text-decoration: none; outline: none; color: #fff; background-color: #28a745; border: none; border-radius: 5px; }";
  html += ".button:hover { background-color: #218838; }";
  html += ".button:active { background-color: #1e7e34; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP8266 Sensor Dashboard</h1>";
  
  html += "<div class='card'>";
  html += "<h2>Sensor Data</h2>";
  html += "<p>Temperature: <span class='status'>" + String(temperature) + " &deg;C</span></p>";
  html += "<p>Humidity: <span class='status'>" + String(humidity) + " %</span></p>";
  html += "<p>Distance: <span class='status'>" + String(distance) + " cm</span></p>";
  html += "<p>Gas Value: <span class='status'>" + String(gasValue) + "</span></p>";
  html += "<p>Gas Detected: <span class='status'>" + String(gasDetected ? "YES" : "NO") + "</span></p>";
  html += "<p>Flame Detected: <span class='status'>" + String(flameDetected ? "YES" : "NO") + "</span></p>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>Relay Controls</h2>";
  html += "<form action='/relay1' method='POST'>";
  html += "<button class='button' type='submit'>Toggle Relay 1</button>";
  html += "</form>";
  html += "<p>Relay 1 State: <span class='status'>" + String(relay1State ? "ON" : "OFF") + "</span></p>";
  html += "<form action='/relay2' method='POST'>";
  html += "<button class='button' type='submit'>Toggle Relay 2</button>";
  html += "</form>";
  html += "<p>Relay 2 State: <span class='status'>" + String(relay2State ? "ON" : "OFF") + "</span></p>";
  html += "</div>";
  
  html += "</div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


/************** Handle Relay1 Function **************/
void handleRelay1() {
  relay1State = !relay1State;
  digitalWrite(RELAY1_PIN, relay1State ? LOW : HIGH); // Active LOW
  server.sendHeader("Location", "/");
  server.send(303);
}

/************** Handle Relay2 Function **************/
void handleRelay2() {
  relay2State = !relay2State;
  digitalWrite(RELAY2_PIN, relay2State ? LOW : HIGH); // Active LOW
  server.sendHeader("Location", "/");
  server.send(303);
}

/************** Update Sensor Data Function **************/
void updateSensorData() {
  // Read temperature and humidity
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Read ultrasonic sensor
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;

  // Read flame sensor (Active LOW)
  flameDetected = digitalRead(FLAME_SENSOR_PIN) == LOW;

  // Read gas sensor (analog value)
  gasValue = analogRead(GAS_SENSOR_PIN); // Read analog value from gas sensor
  gasDetected = gasValue < 400; // Adjust threshold based on your sensor
}

/************** Check Alarms Function **************/
void checkAlarms() {
  if (flameDetected || gasDetected || temperature > 35.0) {
    buzzAlert(3);
  }
}

/************** Buzzer Alert Function **************/
void buzzAlert(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

/************** Display LCD Function **************/
void displayLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1); // One decimal place
  lcd.print("C G:");
  lcd.print(gasValue);
  lcd.setCursor(0, 1);
  lcd.print("D:");
  lcd.print(distance);
  lcd.print("cm F:");
  lcd.print(flameDetected ? "Y" : "N");
}

/***************************************************/
