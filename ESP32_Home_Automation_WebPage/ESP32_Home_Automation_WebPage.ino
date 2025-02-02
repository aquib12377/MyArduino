#include <WiFi.h>
#include <WebServer.h>

// Replace with your network credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Define GPIO pins for the 4 relays
const int relayPins[4] = { 32, 33, 25, 26 };

// Create a web server object
WebServer server(80);

// HTML page
String htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Home Automation</title>
    <style>
        body {
            font-family: sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            margin: 0;
            background-color: #f0f0f0;
        }
        .container {
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
            text-align: center;
        }
        h1 {
            margin-bottom: 20px;
        }
        .appliance {
            border: 1px solid #ddd;
            padding: 15px;
            margin-bottom: 10px;
            border-radius: 5px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
        }
        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: #2196F3;
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .status {
            margin-left: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Home Automation Dashboard</h1>
        <div class="appliance">
            <h2>Living Room Lights</h2>
            <label class="switch">
                <input type="checkbox" id="livingRoomLights" onchange="toggle('1', this)">
                <span class="slider round"></span>
            </label>
            <div class="status" id="livingRoomLightsStatus">Off</div>
        </div>
        <div class="appliance">
            <h2>Bedroom Lights</h2>
            <label class="switch">
                <input type="checkbox" id="bedroomFan" onchange="toggle('2', this)">
                <span class="slider round"></span>
            </label>
            <div class="status" id="bedroomFanStatus">Off</div>
        </div>
        <div class="appliance">
            <h2>Coridoor Lights</h2>
            <label class="switch">
                <input type="checkbox" id="kitchenAC" onchange="toggle('3', this)">
                <span class="slider round"></span>
            </label>
            <div class="status" id="kitchenACStatus">Off</div>
        </div>
        <div class="appliance">
            <h2>Living Room Fan</h2>
            <label class="switch">
                <input type="checkbox" id="garageDoor" onchange="toggle('4', this)">
                <span class="slider round"></span>
            </label>
            <div class="status" id="garageDoorStatus">Closed</div>
        </div>
    </div>
    <script>
        async function toggle(appliance, element) {
            const state = element.checked ? 'on' : 'off';
            await fetch(`/toggle?appliance=${appliance}&state=${state}`);
            const status = element.checked ? 'On' : 'Off';
            document.getElementById(`${element.id}Status`).textContent = status;
        }
    </script>
</body>
</html>
)rawliteral";

// Appliance states
bool applianceStates[4] = { false, false, false, false };

// Function to handle the root page
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Function to handle toggle requests
void handleToggle() {
  if (server.hasArg("appliance") && server.hasArg("state")) {
    int appliance = server.arg("appliance").toInt();
    String state = server.arg("state");

    if (appliance >= 1 && appliance <= 4) {
      bool isOn = (state == "on");
      digitalWrite(relayPins[appliance - 1], isOn ? LOW : HIGH);  // Active LOW
      applianceStates[appliance - 1] = isOn;
      Serial.println("Appliance " + String(appliance) + " is now " + state);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid Appliance");
    }
  } else {
    server.send(400, "text/plain", "Missing Parameters");
  }
}

void setup() {
  Serial.begin(115200);

  // Set up relay pins
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Turn relays off (Active LOW)
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
  Serial.println(WiFi.localIP());

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);

  // Start the server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}
