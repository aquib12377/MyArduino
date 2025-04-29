#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- Pins ---
#define IR_SENSOR_PIN  14
#define TRIG_PIN       26
#define ECHO_PIN       27
#define RED_LED        16
#define GREEN_LED      17
#define BUZZER_PIN     25
#define SERVO_PIN      33

// --- Objects ---
Servo servo;
WebServer server(80);

// --- Network ---
const char* ssid = "MyProject";
const char* password = "12345678";

// --- Variables ---
long duration;
int distance;
bool binFull = false;
bool lidOpen = false;
unsigned long lidOpenTime = 0;
const int binDepth = 30; // cm (actual depth of bin)
int percentage = 0;

void setup() {
  Serial.begin(115200);

  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  servo.attach(SERVO_PIN);
  servo.write(100); // Lid closed

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());

  // Web endpoints
  server.on("/", handleRoot);
  server.on("/status", handleStatusAPI);
  server.begin();
}

void loop() {
  server.handleClient();

  // Get distance
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH, 30000);  // timeout after 30ms (max ~500cm)
  if (duration == 0) return;  // skip if no pulse

  distance = duration * 0.034 / 2;
  distance = constrain(distance, 0, binDepth);

  percentage = map(binDepth - distance, 0, binDepth, 0, 100);
  binFull = (percentage >= 80);

  digitalWrite(RED_LED, binFull);
  digitalWrite(GREEN_LED, !binFull);
  digitalWrite(BUZZER_PIN, binFull ? HIGH : LOW);

  if (digitalRead(IR_SENSOR_PIN) == LOW && !lidOpen) {
    openLid();
  }

  if (lidOpen && millis() - lidOpenTime > 2000) {
    servo.write(100);
    lidOpen = false;
  }
}

void openLid() {
  servo.write(0);  // Open
  lidOpen = true;
  lidOpenTime = millis();
}

// Webpage
void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html><html>
    <head>
      <title>Smart Dustbin</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: sans-serif; background: #f9f9f9; text-align: center; padding: 30px; }
        .card { background: #fff; padding: 20px; border-radius: 12px; box-shadow: 0 2px 10px rgba(0,0,0,0.2); display: inline-block; }
        h2 { color: #333; }
        .bar { width: 100%; background: #eee; border-radius: 10px; overflow: hidden; margin: 10px 0; }
        .fill { height: 20px; background: #4CAF50; width: 0%; transition: width 0.5s ease; }
        .status { font-size: 1.2em; margin-top: 10px; }
      </style>
      <script>
        function updateData() {
          fetch("/status").then(r => r.json()).then(data => {
            document.getElementById("distance").innerText = data.distance + " cm";
            document.getElementById("fill").style.width = data.percentage + "%";
            document.getElementById("percent").innerText = data.percentage + "%";
            document.getElementById("binstatus").innerText = data.full ? "Full ❌" : "Available ✅";
          });
        }
        setInterval(updateData, 3000);
        window.onload = updateData;
      </script>
    </head>
    <body>
      <div class="card">
        <h2>Smart Dustbin Status</h2>
        <p class="status">Distance: <span id="distance">--</span></p>
        <div class="bar"><div id="fill" class="fill"></div></div>
        <p class="status">Fill Level: <span id="percent">--</span></p>
        <p class="status">Status: <span id="binstatus">--</span></p>
      </div>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// JSON API
void handleStatusAPI() {
  String json = "{";
  json += "\"distance\":" + String(distance) + ",";
  json += "\"percentage\":" + String(percentage) + ",";
  json += "\"full\":" + String(binFull ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}
