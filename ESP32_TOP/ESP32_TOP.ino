#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32_Servo.h>

// WiFi credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Motor pins
const int motorPin1 = 14; // Motor 1
const int motorPin2 = 27; // Motor 1
const int motorPin3 = 26; // Motor 2
const int motorPin4 = 25; // Motor 2

// Servo
Servo myservo;
const int servoPin = 13;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adjust the LCD address and size

// Ultrasonic Sensor
const int trigPin = 33;
const int echoPin = 32;

// Web server on port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // Initialize motor pins
  pinMode(motorPin1, OUTPUT);
  pinMode(motorPin2, OUTPUT);
  pinMode(motorPin3, OUTPUT);
  pinMode(motorPin4, OUTPUT);

  // Initialize servo
  myservo.attach(servoPin);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to WiFi");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    Serial.println(WiFi.status());
  }

  lcd.clear();
  lcd.print("WiFi connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // Initialize ultrasonic sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Define endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/servo", HTTP_GET, handleServo);
  server.on("/setlcd", HTTP_GET, handleLCD);
  server.on("/distance", HTTP_GET, []() {
    server.send(200, "text/plain", String(getDistance()) + " cm");
  });
  server.begin();
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  String html = "<html><head><title>ESP32 WiFi Bot</title></head><body>"
                "<h1>Control Panel</h1>"
                "<p><button onclick=\"sendCommand('forward')\">Forward</button></p>"
                "<p><button onclick=\"sendCommand('backward')\">Backward</button></p>"
                "<p><button onclick=\"sendCommand('left')\">Left</button></p>"
                "<p><button onclick=\"sendCommand('right')\">Right</button></p>"
                "<p><button onclick=\"sendCommand('stop')\">Stop</button></p>"
                "<p>Servo: <input type='range' min='0' max='180' onchange=\"sendServo(this.value)\"></p>"
                "<p>Set LCD Text: <input type='text' id='lcdtext' value=''><button onclick='setLCDText()'>Set Text</button></p>"
                "<p>Distance: <span id='distance'>0</span> cm</p>"
                "<script>"
                "function sendCommand(command) {"
                "  fetch('/command?cmd=' + command);"
                "}"
                "function sendServo(angle) {"
                "  fetch('/servo?angle=' + angle);"
                "}"
                "function setLCDText() {"
                "  var text = document.getElementById('lcdtext').value;"
                "  fetch('/setlcd?text=' + text);"
                "}"
                "setInterval(function() {"
                "  fetch('/distance').then(response => response.text()).then(data => document.getElementById('distance').textContent=data);"
                "}, 1000);"
                "</script>"
                "</body></html>";
  server.send(200, "text/html", html);
}

void handleCommand() {
  if (server.hasArg("cmd")) {
    String command = server.arg("cmd");
    controlMotors(command);
  }
  server.send(200, "text/plain", "Command executed");
}

void handleServo() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    myservo.write(angle);
  }
  server.send(200, "text/plain", "Servo angle set");
}

void handleLCD() {
  if (server.hasArg("text")) {
    lcd.clear();
    lcd.print(server.arg("text"));
  }
  server.send(200, "text/plain", "LCD text set");
}

void controlMotors(String direction) {
  // Motor control logic as before
}

float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}
