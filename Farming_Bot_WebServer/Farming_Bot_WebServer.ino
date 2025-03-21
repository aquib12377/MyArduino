/*************************************************
 * ESP32 Farming Bot with Modern UI
 * 
 * Pin mapping:
 *   - Motor A pins: 14, 27
 *   - Motor B pins: 26, 25
 *   - Seeder (Servo) pin: 13
 *   - Sprayer (Active-low Relay) pin: 12
 *   - Soil Moisture (Analog Input) pin: 34
 *
 * Make sure to install the "ESP32Servo" library if
 * the default Servo library doesn't work well.
 *************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>  // Use this library for ESP32

// Replace with your actual network credentials
const char* ssid     = "MyProject";
const char* password = "12345678";

// Motor control pins
const int motorA1 = 14;  // Motor A direction 1
const int motorA2 = 27;  // Motor A direction 2
const int motorB1 = 26;  // Motor B direction 1
const int motorB2 = 25;  // Motor B direction 2

// Seeder (Servo) pin
const int seederPin = 13;
Servo seederServo;

// Sprayer (Active-low Relay) pin
const int sprayerPin = 12;

// Soil moisture analog input pin
const int moisturePin = 34;

// Create a WebServer on port 80
WebServer server(80);

// Track whether the seeder is "open" or "closed"
bool seederOpen = false;

// Track whether the sprayer is ON or OFF
bool sprayerOn = false;

/*************************************************
 * Helper Functions
 *************************************************/

// Stop all motors
void stopMotors() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, LOW);
}

// Forward
void handleForward() {
  digitalWrite(motorA1, HIGH);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, HIGH);
  digitalWrite(motorB2, LOW);
  server.send(200, "text/plain", "Forward");
}

// Backward
void handleBackward() {
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, HIGH);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, HIGH);
  server.send(200, "text/plain", "Backward");
}

// Left
void handleLeft() {
  // Turn left: left motor backward, right motor forward
  digitalWrite(motorA1, LOW);
  digitalWrite(motorA2, HIGH);
  digitalWrite(motorB1, HIGH);
  digitalWrite(motorB2, LOW);
  server.send(200, "text/plain", "Left");
}

// Right
void handleRight() {
  // Turn right: left motor forward, right motor backward
  digitalWrite(motorA1, HIGH);
  digitalWrite(motorA2, LOW);
  digitalWrite(motorB1, LOW);
  digitalWrite(motorB2, HIGH);
  server.send(200, "text/plain", "Right");
}

// Stop
void handleStop() {
  stopMotors();
  server.send(200, "text/plain", "Stop");
}

// Toggle Seeder
void handleSeeder() {
  if (seederOpen) {
    seederServo.write(0);   // Move to 0 degrees
    seederOpen = false;
    server.send(200, "text/plain", "Seeder Closed");
  } else {
    seederServo.write(90);  // Move to 90 degrees
    seederOpen = true;
    server.send(200, "text/plain", "Seeder Opened");
  }
}

// Toggle Sprayer (Active LOW)
void handleSprayer() {
  if (sprayerOn) {
    // Turn OFF = set relay pin HIGH
    digitalWrite(sprayerPin, HIGH);
    sprayerOn = false;
    server.send(200, "text/plain", "Sprayer OFF");
  } else {
    // Turn ON = set relay pin LOW
    digitalWrite(sprayerPin, LOW);
    sprayerOn = true;
    server.send(200, "text/plain", "Sprayer ON");
  }
}

// Return moisture reading
void handleMoisture() {
  int moistureValue = analogRead(moisturePin);
  server.send(200, "text/plain", String(moistureValue));
}

/*************************************************
 * HTML for the Web UI
 *************************************************/

// The main page will load the UI with embedded CSS/JS
String buildHTML() {
  String page = R"====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 0; padding: 0;
      background: #f1f1f1;
    }
    header {
      background-color: #4CAF50;
      padding: 1em;
      text-align: center;
      color: #fff;
    }
    h1 {
      margin: 0;
      font-size: 1.8em;
    }
    .container {
      margin: 2% auto;
      max-width: 600px;
      background: #fff;
      padding: 1.5em;
      box-shadow: 0 2px 5px rgba(0,0,0,0.2);
      border-radius: 8px;
    }
    .controls {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 1em;
      margin-top: 1em;
    }
    button {
      padding: 0.75em 1.5em;
      border: none;
      border-radius: 6px;
      cursor: pointer;
      font-size: 1em;
      transition: background 0.3s, transform 0.2s;
      background-color: #4CAF50;
      color: #fff;
    }
    button:hover {
      background-color: #45a049;
      transform: translateY(-2px);
    }
    .stop-button {
      background-color: #f44336;
    }
    .stop-button:hover {
      background-color: #e53935;
    }
    .section-title {
      font-weight: bold;
      margin-top: 0.5em;
      margin-bottom: 0.5em;
    }
    .status {
      text-align: center;
      margin-top: 1em;
      font-size: 1.2em;
      background: #e3e3e3;
      padding: 0.5em;
      border-radius: 6px;
    }
    .moisture-display {
      font-weight: bold;
      color: #333;
    }
  </style>
</head>
<body>
  <header>
    <h1>Farming Bot Control</h1>
  </header>
  
  <div class="container">
    <div class="status">
      Soil Moisture: <span id="moisture" class="moisture-display">--</span>
    </div>
    
    <h2 class="section-title">Movement Controls</h2>
    <div class="controls">
      <button onclick="sendCommand('/forward')">Forward</button>
      <button onclick="sendCommand('/left')">Left</button>
      <button onclick="sendCommand('/right')">Right</button>
      <button onclick="sendCommand('/backward')">Backward</button>
      <button class="stop-button" onclick="sendCommand('/stop')">Stop</button>
    </div>
    
    <h2 class="section-title">Actions</h2>
    <div class="controls">
      <button onclick="sendCommand('/seeder')">Toggle Seeder</button>
      <button onclick="sendCommand('/sprayer')">Toggle Sprayer</button>
    </div>
  </div>

  <script>
    // Sends a request to a given endpoint and logs the response
    function sendCommand(endpoint) {
      fetch(endpoint)
        .then(response => response.text())
        .then(data => {
          console.log("Response:", data);
        })
        .catch(error => {
          console.error("Error:", error);
        });
    }

    // Periodically update the moisture reading
    function updateMoisture() {
      fetch('/moisture')
        .then(response => response.text())
        .then(data => {
          document.getElementById('moisture').innerText = data;
        })
        .catch(error => {
          console.error("Moisture Error:", error);
        });
    }

    setInterval(updateMoisture, 2000); // update every 2 seconds
    window.onload = updateMoisture;    // also update on page load
  </script>
</body>
</html>
  )====";

  return page;
}

/*************************************************
 * Routes
 *************************************************/

void handleRoot() {
  server.send(200, "text/html", buildHTML());
}

/*************************************************
 * Setup & Loop
 *************************************************/

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(motorA1, OUTPUT);
  pinMode(motorA2, OUTPUT);
  pinMode(motorB1, OUTPUT);
  pinMode(motorB2, OUTPUT);
  stopMotors(); // Ensure motors are off

  // Seeder servo
  seederServo.attach(seederPin);
  seederServo.write(0); // Start closed
  seederOpen = false;

  // Sprayer pin (active low)
  pinMode(sprayerPin, OUTPUT);
  digitalWrite(sprayerPin, HIGH); // OFF initially
  sprayerOn = false;

  // Connect to Wi-Fi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Define routes
  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);
  server.on("/seeder", handleSeeder);
  server.on("/sprayer", handleSprayer);
  server.on("/moisture", handleMoisture);

  // Start the server
  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  // Handle client requests
  server.handleClient();
}
