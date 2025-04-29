#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>   // ← ESP32Servo instead of Servo.h

// —— HOTSPOT (STA) CONFIG ——
// Replace these with your hotspot’s credentials:
const char* STA_SSID = "MyProject";
const char* STA_PASS = "12345678";

// —— MOTOR PINS ——
const int LF_PIN = 27;
const int LB_PIN = 26;
const int RF_PIN = 33;
const int RB_PIN = 32;

// —— SERVO & RELAY ——
const int SERVO_PIN = 13;
const int RELAY_PIN = 15;  // active‑LOW

WebServer server(80);
Servo gateServo;

// —— HTML PAGE —— (Bootstrap 5 + Icons, mobile‑friendly)
const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>AgriBot Dashboard</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
  <link href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.10.5/font/bootstrap-icons.css" rel="stylesheet">
  <style>
    body {
      background-color: #f0fdf4;
      font-family: 'Segoe UI', sans-serif;
    }
    .dashboard {
      max-width: 480px;
      margin: auto;
      padding: 1rem;
    }
    .btn-control {
      width: 4rem;
      height: 4rem;
      font-size: 1.5rem;
    }
    .card {
      background-color: #ffffff;
      border: none;
      border-radius: 1rem;
      box-shadow: 0 0 20px rgba(0, 128, 0, 0.1);
    }
    .nav-branding {
      background-color: #28a745;
    }
    .form-switch .form-check-input:checked {
      background-color: #28a745;
      border-color: #28a745;
    }
  </style>
</head>
<body>
  <nav class="navbar nav-branding navbar-dark">
    <div class="container-fluid justify-content-center">
      <span class="navbar-brand mb-0 h1"><i class="bi bi-robot me-2"></i>AgriBot Control</span>
    </div>
  </nav>

  <div class="dashboard mt-4">
    <div class="card p-4">
      <h5 class="text-center mb-3 text-success"><i class="bi bi-joystick"></i> Movement Controls</h5>

      <div class="d-flex justify-content-center mb-3">
        <button class="btn btn-outline-success btn-control" onclick="cmd('forward')">
          <i class="bi bi-arrow-up"></i>
        </button>
      </div>

      <div class="d-flex justify-content-center gap-3 mb-3">
        <button class="btn btn-outline-success btn-control" onclick="cmd('left')">
          <i class="bi bi-arrow-left"></i>
        </button>
        <button class="btn btn-success btn-control" onclick="cmd('stop')">
          <i class="bi bi-stop-fill"></i>
        </button>
        <button class="btn btn-outline-success btn-control" onclick="cmd('right')">
          <i class="bi bi-arrow-right"></i>
        </button>
      </div>

      <div class="d-flex justify-content-center mb-4">
        <button class="btn btn-outline-success btn-control" onclick="cmd('backward')">
          <i class="bi bi-arrow-down"></i>
        </button>
      </div>

      <h6 class="text-center text-muted">Equipment Control</h6>
<div class="d-flex justify-content-around">
  <div class="form-check form-switch text-center">
    <input class="form-check-input" type="checkbox" id="servoToggle" onchange="toggleServo()" title="Toggle Seeding Mechanism">
    <label class="form-check-label" for="servoToggle">
      <i class="bi bi-seedling text-success me-1"></i> Seeding
    </label>
  </div>
  <div class="form-check form-switch text-center">
    <input class="form-check-input" type="checkbox" id="relayToggle" onchange="toggleRelay()" title="Toggle Water Spray System">
    <label class="form-check-label" for="relayToggle">
      <i class="bi bi-droplet-half text-primary me-1"></i> Water Spray
    </label>
  </div>
</div>

    </div>
  </div>

  <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js"></script>
  <script>
    function cmd(c) {
      fetch('/' + c).catch(console.error);
    }
    function toggleServo() {
      fetch('/toggleServo?state=' + (document.getElementById('servoToggle').checked ? 'on' : 'off'));
    }
    function toggleRelay() {
      fetch('/toggleRelay?state=' + (document.getElementById('relayToggle').checked ? 'on' : 'off'));
    }
  </script>
</body>
</html>

)rawliteral";


// —— MOTOR HANDLERS —— 
void handleForward(){
  Serial.println("[CMD] Forward");
  digitalWrite(LF_PIN, HIGH); digitalWrite(LB_PIN, LOW);
  digitalWrite(RF_PIN, HIGH); digitalWrite(RB_PIN, LOW);
  server.send(200, "text/plain", "OK");
}
void handleBackward(){
  Serial.println("[CMD] Backward");
  digitalWrite(LF_PIN, LOW);  digitalWrite(LB_PIN, HIGH);
  digitalWrite(RF_PIN, LOW);  digitalWrite(RB_PIN, HIGH);
  server.send(200, "text/plain", "OK");
}
void handleLeft(){
  Serial.println("[CMD] Left Pivot");
  digitalWrite(LF_PIN, LOW);  digitalWrite(LB_PIN, HIGH);
  digitalWrite(RF_PIN, HIGH); digitalWrite(RB_PIN, LOW);
  server.send(200, "text/plain", "OK");
}
void handleRight(){
  Serial.println("[CMD] Right Pivot");
  digitalWrite(LF_PIN, HIGH); digitalWrite(LB_PIN, LOW);
  digitalWrite(RF_PIN, LOW);  digitalWrite(RB_PIN, HIGH);
  server.send(200, "text/plain", "OK");
}
void handleStop(){
  Serial.println("[CMD] Stop");
  digitalWrite(LF_PIN, LOW); digitalWrite(LB_PIN, LOW);
  digitalWrite(RF_PIN, LOW); digitalWrite(RB_PIN, LOW);
  server.send(200, "text/plain", "OK");
}

// —— SERVO & RELAY HANDLERS —— 
void handleToggleServo(){
  String st = server.arg("state");
  Serial.printf("[CMD] Servo %s\n", st.c_str());
  gateServo.write(st == "on" ? 90 : 0);
  server.send(200, "text/plain", "OK");
}
void handleToggleRelay(){
  String st = server.arg("state");
  Serial.printf("[CMD] Relay %s\n", st.c_str());
  digitalWrite(RELAY_PIN, st == "on" ? LOW : HIGH);
  server.send(200, "text/plain", "OK");
}


void setup(){
  Serial.begin(115200);
  Serial.println("===== Starting ESP32 Bot Control (STA Mode) =====");

  // configure outputs
  pinMode(LF_PIN, OUTPUT);
  pinMode(LB_PIN, OUTPUT);
  pinMode(RF_PIN, OUTPUT);
  pinMode(RB_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // relay off

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  // —— CONNECT TO HOTSPOT —— 
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.printf("Connecting to SSID '%s' …\n", STA_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // start HTTP server
  server.on("/",           [](){ server.send_P(200, "text/html", MAIN_page); });
  server.on("/forward",    handleForward);
  server.on("/backward",   handleBackward);
  server.on("/left",       handleLeft);
  server.on("/right",      handleRight);
  server.on("/stop",       handleStop);
  server.on("/toggleServo", handleToggleServo);
  server.on("/toggleRelay", handleToggleRelay);
  server.begin();
  Serial.println("HTTP server running on port 80");
}

void loop(){
  server.handleClient();
}

