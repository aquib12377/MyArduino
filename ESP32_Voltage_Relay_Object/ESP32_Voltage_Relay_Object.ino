#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin Definitions
#define VOLTAGE_SENSOR_PIN 34  // Voltage sensor output pin
#define RELAY_PIN 15           // Relay control pin
#define IR_SENSOR_PIN 4        // IR sensor pin

// LCD I2C address and dimensions
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi Credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Create a web server object
WebServer server(80);

// Variables to store voltage, IR sensor state, and relay state
float voltage = 0.0;
bool relayState = false;
bool irDetected = false;

// Function to read voltage from the sensor
float readVoltage() {
  int analogValue = analogRead(VOLTAGE_SENSOR_PIN);
  float voltage = (analogValue * 16.5) / 4095.0;  // Scale for 25V sensor
  return voltage;
}

// Function to check if an object is detected by the IR sensor
bool isObjectDetected() {
  return digitalRead(IR_SENSOR_PIN) == LOW;  // Active LOW when object detected
}

// Update LCD display
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Voltage:");
  lcd.setCursor(9, 0);
  lcd.print(voltage, 2);  // Display voltage up to 2 decimal places
  lcd.print("V");
  
  lcd.setCursor(0, 1);
  if (irDetected) {
    lcd.print("IR: Detected   ");
  } else {
    lcd.print("IR: Not Detected");
  }
}

// Handle root URL "/"
void handleRoot() {
  String page = R"rawliteral(
    <!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Voltage Monitor</title>
  <style>
    /* Global Styles */
    body {
      font-family: 'Roboto', sans-serif;
      background-color: #e0f7fa;
      margin: 0;
      padding: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
    }

    /* Main container */
    .container {
      background-color: #ffffff;
      border-radius: 15px;
      box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
      padding: 30px;
      text-align: center;
      max-width: 400px;
      width: 100%;
    }

    /* Header styling */
    h1 {
      font-size: 24px;
      color: #00796b;
      margin-bottom: 20px;
    }

    /* Gauge container */
    .gauge-container {
      width: 100%;
      height: 200px;
      margin-bottom: 20px;
    }

    #gauge {
      width: 100%;
      height: 100%;
    }

    /* Voltage display */
    h2 {
      font-size: 18px;
      color: #333;
      margin: 15px 0;
    }

    #voltageValue {
      font-weight: bold;
      font-size: 22px;
      color: #00796b;
    }

    /* Alert indicator */
    #alert {
      margin: 20px auto;
      width: 100px;
      height: 100px;
      border-radius: 50%;
      background-color: green;
      border: 4px solid #ccc;
      transition: background-color 0.5s ease;
    }

    /* Button styling */
    .btn {
      background: linear-gradient(90deg, #00796b, #004d40);
      color: white;
      padding: 12px 24px;
      font-size: 16px;
      border: none;
      border-radius: 25px;
      cursor: pointer;
      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
      margin-top: 30px;
      transition: all 0.3s ease;
    }

    .btn:hover {
      background: linear-gradient(90deg, #004d40, #00796b);
      box-shadow: 0 6px 12px rgba(0, 0, 0, 0.2);
    }

    /* Media query for responsive design */
    @media (max-width: 600px) {
      .container {
        padding: 20px;
        width: 90%;
      }

      .gauge-container {
        height: 150px;
      }

      #alert {
        width: 80px;
        height: 80px;
      }

      .btn {
        padding: 10px 20px;
        font-size: 14px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Voltage Monitor</h1>
    <div class="gauge-container">
      <canvas id="gauge"></canvas>
    </div>
    <h2>Voltage: <span id="voltageValue">0.00</span>V</h2>
    <div id="alert"></div>
    <button class="btn" onclick="toggleRelay()">Toggle Relay</button>
  </div>

  <!-- Include the gauge.js library -->
  <script src="https://bernii.github.io/gauge.js/dist/gauge.min.js"></script>

  <script>
    // Initialize gauge
    let gauge = new Gauge(document.getElementById("gauge")).setOptions({
      angle: 0.15,
      lineWidth: 0.44,
      radiusScale: 1,
      pointer: {
        length: 0.6,
        strokeWidth: 0.035,
        color: '#000000'
      },
      staticLabels: {
        font: "12px sans-serif",
        labels: [0, 5, 10, 15, 20, 25],
        color: "#000000",
        fractionDigits: 1
      },
      staticZones: [
        {strokeStyle: "#30B32D", min: 0, max: 15},
        {strokeStyle: "#F03E3E", min: 15, max: 25}
      ],
      limitMax: false,
      limitMin: false,
      highDpiSupport: true
    });
    gauge.maxValue = 25;
    gauge.setMinValue(0);
    gauge.set(0);

    // Function to update voltage and alert status
    function updateData() {
      fetch('/voltage')
        .then(response => response.json())
        .then(data => {
          document.getElementById("voltageValue").innerText = data.voltage.toFixed(2);
          gauge.set(data.voltage);

          // Update alert status based on IR sensor
          if (data.irDetected) {
            document.getElementById("alert").style.backgroundColor = "red";
          } else {
            document.getElementById("alert").style.backgroundColor = "green";
          }
        });
    }

    // Function to toggle relay
    function toggleRelay() {
      fetch('/toggleRelay')
        .then(response => response.json())
        .then(data => {
          console.log("Relay toggled");
        });
    }

    // Update data every second
    setInterval(updateData, 1000);
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", page);
}

// Handle voltage data request "/voltage"
void handleVoltage() {
  voltage = readVoltage();
  irDetected = isObjectDetected();

  // Update LCD display
  updateLCD();

  String jsonResponse = "{\"voltage\": " + String(voltage, 2) + ", \"irDetected\": " + String(irDetected) + "}";
  server.send(200, "application/json", jsonResponse);
}

// Handle relay toggle "/toggleRelay"
void handleRelayToggle() {
  relayState = !relayState;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);

  String jsonResponse = "{\"relayState\": " + String(relayState) + "}";
  server.send(200, "application/json", jsonResponse);
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize LCD
  lcd.init(); 
  lcd.backlight(); 
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Pin configurations
  pinMode(VOLTAGE_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  // Set relay to off initially
  digitalWrite(RELAY_PIN, LOW);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    lcd.setCursor(0, 1);
    lcd.print("Connecting...");
  }
  Serial.println("Connected to WiFi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // Configure web server routes
  server.on("/", handleRoot);
  server.on("/voltage", handleVoltage);
  server.on("/toggleRelay", handleRelayToggle);

  // Start web server
  server.begin();
  Serial.println("Server started");
}

void loop() {
  // Handle web server requests
  server.handleClient();
}
