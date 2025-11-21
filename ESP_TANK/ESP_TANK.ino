#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>   // Install "ESP32Servo" via Library Manager

// ================== USER WIFI CONFIG ==================
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// ================== PIN DEFINITIONS ===================
// L298N - Motor A
#define IN1_MA  27
#define IN2_MA  26

// L298N - Motor B
#define IN3_MB  25
#define IN4_MB  33

// Relay for 3rd Motor (ACTIVE-LOW)
#define RELAY_PIN           32
const bool RELAY_ACTIVE   = LOW;   // LOW turns motor ON
const bool RELAY_INACTIVE = HIGH;  // HIGH turns motor OFF

// Servo SG90
#define SERVO_PIN  14

// ================== OBJECTS & TYPES ===================
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
Servo myServo;

enum MotorDir : uint8_t {
  MOTOR_STOP = 0,
  MOTOR_FORWARD,
  MOTOR_BACKWARD
};

// =============== SIMPLE WEB PAGE (HTML + JS) ==========
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <title>ESP32 Motor & Servo Control</title>
  <style>
    body { font-family: Arial, sans-serif; background:#111; color:#eee; text-align:center; }
    h1 { color:#4CAF50; }
    .btn {
      padding: 10px 20px; margin: 5px; font-size: 16px;
      border: none; border-radius: 4px; cursor: pointer;
    }
    .drive { background:#4CAF50; color:white; }
    .stop  { background:#f44336; color:white; }
    .relay { background:#2196F3; color:white; }
    .servo { margin-top:20px; }
    #status { margin:10px; font-size:14px; }
    .row { margin:10px 0; }
  </style>
</head>
<body>
  <h1>ESP32 Motor & Servo Control</h1>
  <div id="status">WebSocket: <span id="wsState">CONNECTING...</span></div>

  <div class="row">
    <button class="btn drive" onclick="sendCmd('DRIVE:F')">Forward</button>
  </div>
  <div class="row">
    <button class="btn drive" onclick="sendCmd('DRIVE:L')">Left</button>
    <button class="btn stop"  onclick="sendCmd('DRIVE:S')">Stop</button>
    <button class="btn drive" onclick="sendCmd('DRIVE:R')">Right</button>
  </div>
  <div class="row">
    <button class="btn drive" onclick="sendCmd('DRIVE:B')">Backward</button>
  </div>

  <hr/>

  <div class="row">
    <button class="btn relay" onclick="sendCmd('RELAY:1')">Relay ON</button>
    <button class="btn relay" onclick="sendCmd('RELAY:0')">Relay OFF</button>
  </div>

  <hr/>

  <div class="servo">
    <label for="servoRange">Servo Angle: <span id="servoVal">90</span>°</label><br/>
    <input id="servoRange" type="range" min="0" max="180" value="90"
           oninput="updateServo(this.value)" />
  </div>

  <script>
    var ws;
    var wsStateSpan = document.getElementById('wsState');
    var servoValSpan = document.getElementById('servoVal');

    function initWebSocket() {
      // change 81 if you change WebSocket port in code
      ws = new WebSocket('ws://' + window.location.hostname + ':81/');
      
      ws.onopen = function() {
        wsStateSpan.textContent = 'OPEN';
        wsStateSpan.style.color = '#4CAF50';
      };

      ws.onclose = function() {
        wsStateSpan.textContent = 'CLOSED - retrying...';
        wsStateSpan.style.color = '#f44336';
        setTimeout(initWebSocket, 2000);
      };

      ws.onerror = function() {
        wsStateSpan.textContent = 'ERROR';
        wsStateSpan.style.color = '#ff9800';
      };

      ws.onmessage = function(event) {
        console.log('Received:', event.data);
      };
    }

    function sendCmd(cmd) {
      if (!ws || ws.readyState !== WebSocket.OPEN) {
        console.warn('WebSocket not connected');
        return;
      }
      console.log('Sending:', cmd);
      ws.send(cmd);
    }

    function updateServo(value) {
      servoValSpan.textContent = value;
      sendCmd('SERVO:' + value);
    }

    window.addEventListener('load', initWebSocket);
  </script>
</body>
</html>
)HTML";

// =============== MOTOR & SERVO HELPERS ===============

// Motor A (via L298N IN1/IN2)
void setMotorA(MotorDir dir) {
  switch (dir) {
    case MOTOR_FORWARD:
      digitalWrite(IN1_MA, HIGH);
      digitalWrite(IN2_MA, LOW);
      break;
    case MOTOR_BACKWARD:
      digitalWrite(IN1_MA, LOW);
      digitalWrite(IN2_MA, HIGH);
      break;
    case MOTOR_STOP:
    default:
      digitalWrite(IN1_MA, LOW);
      digitalWrite(IN2_MA, LOW);
      break;
  }
}

// Motor B (via L298N IN3/IN4)
void setMotorB(MotorDir dir) {
  switch (dir) {
    case MOTOR_FORWARD:
      digitalWrite(IN3_MB, HIGH);
      digitalWrite(IN4_MB, LOW);
      break;
    case MOTOR_BACKWARD:
      digitalWrite(IN3_MB, LOW);
      digitalWrite(IN4_MB, HIGH);
      break;
    case MOTOR_STOP:
    default:
      digitalWrite(IN3_MB, LOW);
      digitalWrite(IN4_MB, LOW);
      break;
  }
}

// Relay Motor control (ACTIVE-LOW)
void setRelayMotor(bool on) {
  digitalWrite(RELAY_PIN, on ? RELAY_ACTIVE : RELAY_INACTIVE);
}

// Servo control: 0–180 degrees
void setServoAngle(int angle) {
  if (angle < 0)   angle = 0;
  if (angle > 180) angle = 180;
  myServo.write(angle);
}

// =============== WEBSOCKET HANDLER ===================
void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
  // Convert payload to String safely
  String msg;
  msg.reserve(length + 1);
  for (size_t i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.printf("[WS %u] Received: %s\n", num, msg.c_str());

  // DRIVE commands: DRIVE:F / DRIVE:B / DRIVE:L / DRIVE:R / DRIVE:S
  if (msg.startsWith("DRIVE:")) {
    char d = msg.charAt(6);
    switch (d) {
      case 'F':   // forward
        setMotorA(MOTOR_FORWARD);
        setMotorB(MOTOR_FORWARD);
        break;
      case 'B':   // backward
        setMotorA(MOTOR_BACKWARD);
        setMotorB(MOTOR_BACKWARD);
        break;
      case 'L':   // left (A backward, B forward) - adjust if you want opposite
        setMotorA(MOTOR_BACKWARD);
        setMotorB(MOTOR_FORWARD);
        break;
      case 'R':   // right (A forward, B backward)
        setMotorA(MOTOR_FORWARD);
        setMotorB(MOTOR_BACKWARD);
        break;
      case 'S':   // stop
      default:
        setMotorA(MOTOR_STOP);
        setMotorB(MOTOR_STOP);
        break;
    }
  }

  // RELAY commands: RELAY:1 (ON), RELAY:0 (OFF)
  else if (msg.startsWith("RELAY:")) {
    // After "RELAY:" comes either '1' or '0'
    char state = msg.charAt(6);
    bool on = (state == '1');
    setRelayMotor(on);
  }

  // SERVO commands: SERVO:<angle>
  else if (msg.startsWith("SERVO:")) {
    String valStr = msg.substring(6);
    int angle = valStr.toInt();
    setServoAngle(angle);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[WS %u] Connected from %s\n", num, ip.toString().c_str());
      webSocket.sendTXT(num, "Connected to ESP32 WebSocket");
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[WS %u] Disconnected\n", num);
      break;

    case WStype_TEXT:
      handleWebSocketMessage(num, payload, length);
      break;

    default:
      break;
  }
}

// =============== HTTP HANDLER ========================
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  delay(200);

  // ---- GPIO setup ----
  pinMode(IN1_MA, OUTPUT);
  pinMode(IN2_MA, OUTPUT);
  pinMode(IN3_MB, OUTPUT);
  pinMode(IN4_MB, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);
  setRelayMotor(false);             // relay OFF initially

  setMotorA(MOTOR_STOP);
  setMotorB(MOTOR_STOP);

  myServo.attach(SERVO_PIN);
  setServoAngle(90);                // center position

  // ---- WiFi ----
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // ---- HTTP server ----
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started on port 80");

  // ---- WebSocket server ----
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

// ======================== LOOP =======================
void loop() {
  server.handleClient();
  webSocket.loop();
}
