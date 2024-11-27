#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Wire.h>                   // Include Wire library for I2C
#include <LiquidCrystal_I2C.h>      // Include LiquidCrystal I2C library

// Replace with your network credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Define GPIO pins
// DC Motor
const int motorPin1 = 14; // IN1
const int motorPin2 = 27; // IN2
//const int motorEnablePin = 5; // ENA (PWM)

// Servo Motor
const int servoPin = 12;

// DHT11 Sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// IR Sensor
const int irSensorPin = 13;

// Servo object
Servo myServo;

// Create WebServer object on port 80
WebServer server(80);

// Variables to store sensor data
float temperature = 0.0;
float humidity = 0.0;
bool personDetected = false;

// Initialize I2C LCD
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change to 20,4 if you have a 20x4 LCD

// Function to control DC Motor
void controlMotor(String direction) {
  if (direction == "forward") {
    digitalWrite(motorPin1, HIGH);
    digitalWrite(motorPin2, LOW);
    lcd.setCursor(0,1);
    lcd.print("Motor: Forward   ");
  }
  else if (direction == "reverse") {
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, HIGH);
    lcd.setCursor(0,1);
    lcd.print("Motor: Reverse   ");
  }
  else { // stop
    digitalWrite(motorPin1, LOW);
    digitalWrite(motorPin2, LOW);
    lcd.setCursor(0,1);
    lcd.print("Motor: Stopped   ");
  }
}

// Function to read sensors and update variables and LCD
void readSensors() {
  // Read DHT11
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    lcd.setCursor(0,0);
    lcd.print("DHT11 Error      ");
    return;
  }

  // Read IR Sensor
  int irValue = digitalRead(irSensorPin);
  personDetected = (irValue == LOW) ? true : false; // Adjust based on sensor

  // Update LCD with sensor data
  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(temperature,1);
  lcd.print("C H:");
  lcd.print(humidity,1);
  lcd.print("% ");

  lcd.setCursor(0,1);
  lcd.print("Pers:");
  lcd.print(personDetected ? "Yes " : "No  ");
}

// Handle root path
void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Control Panel</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; }";
  html += ".container { margin-top: 50px; }";
  html += "button { padding: 15px 25px; font-size: 16px; margin: 5px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32 Control Panel</h1>";

  // Motor Controls
  html += "<h2>DC Motor Control</h2>";
  html += "<button onclick=\"location.href='/motor?direction=forward'\">Forward</button>";
  html += "<button onclick=\"location.href='/motor?direction=reverse'\">Reverse</button>";
  html += "<button onclick=\"location.href='/motor?direction=stop'\">Stop</button>";

  // Servo Control
  html += "<h2>Servo Control</h2>";
  html += "<input type='range' min='0' max='180' value='90' id='servoRange' oninput='updateServo(this.value)'/>";
  html += "<span id='servoValue'>90</span>°";

  // Sensor Data
  html += "<h2>Sensor Data</h2>";
  html += "<p>Temperature: <span id='temp'>--</span> &deg;C</p>";
  html += "<p>Humidity: <span id='hum'>--</span> %</p>";
  html += "<p>Person Detected: <span id='ir'>--</span></p>";

  // JavaScript for AJAX
  html += "<script>";
  html += "function updateServo(val){";
  html += "document.getElementById('servoValue').innerText = val;";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/servo?angle=' + val, true);";
  html += "xhr.send();";
  html += "}";
  html += "function fetchStatus(){";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.onreadystatechange = function() {";
  html += "if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "var data = JSON.parse(xhr.responseText);";
  html += "document.getElementById('temp').innerText = data.temperature;";
  html += "document.getElementById('hum').innerText = data.humidity;";
  html += "document.getElementById('ir').innerText = data.personDetected ? 'Yes' : 'No';";
  html += "}";
  html += "};";
  html += "xhr.open('GET', '/status', true);";
  html += "xhr.send();";
  html += "}";
  html += "setInterval(fetchStatus, 2000);"; // Fetch status every 2 seconds
  html += "</script>";

  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

// Handle motor control
void handleMotor() {
  if (server.hasArg("direction")) {
    String direction = server.arg("direction");
    controlMotor(direction);
    server.send(200, "text/plain", "Motor " + direction);
  }
  else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// Handle servo control
void handleServo() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    angle = constrain(angle, 0, 180);
    myServo.write(angle);
    server.send(200, "text/plain", "Servo angle set to " + String(angle));
  }
  else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// Handle sensor status
void handleStatus() {
  // Read sensors and update variables and LCD
  readSensors();

  // Prepare JSON
  String json = "{";
  json += "\"temperature\":" + String(temperature,1) + ",";
  json += "\"humidity\":" + String(humidity,1) + ",";
  json += "\"personDetected\":" + String(personDetected);
  json += "}";

  server.send(200, "application/json", json);
}

void setup(){
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize motor pins
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  //pinMode(motorEnablePin, OUTPUT);
  //analogWrite(motorEnablePin, 255); // Set motor speed to max (optional)

  // Initialize IR sensor pin
  pinMode(irSensorPin, INPUT);

  // Initialize DHT sensor
  dht.begin();

  // Initialize Servo
  myServo.attach(servoPin);
  myServo.write(90); // Set servo to middle position

  // Initialize I2C LCD
  lcd.begin();                      // Initialize the LCD
  lcd.backlight();                 // Turn on the backlight
  lcd.setCursor(0,0);
  lcd.print("Borewell Rescue");
  lcd.setCursor(0,1);
  lcd.print("Initializing...");

  delay(2000); // Wait for 2 seconds to show the message

  // Clear LCD after initialization
  lcd.clear();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
    lcd.setCursor(0,0);
    lcd.print("Connecting WiFi..");
  }
  Serial.println();
  Serial.print("Connected to WiFi. IP Address: ");
  Serial.println(WiFi.localIP());

  // Display IP on LCD
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  lcd.setCursor(0,1);
  lcd.print("Borewell Ready");

  delay(2000); // Display the IP for 2 seconds

  // Clear LCD to show sensor data
  lcd.clear();
  readSensors(); // Initial sensor read and LCD update

  // Define server routes
  server.on("/", handleRoot);
  server.on("/motor", handleMotor);
  server.on("/servo", handleServo);
  server.on("/status", handleStatus);

  // Handle undefined routes
  server.onNotFound([](){
    server.send(404, "text/plain", "404: Not Found");
  });

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop(){
  server.handleClient();
}
