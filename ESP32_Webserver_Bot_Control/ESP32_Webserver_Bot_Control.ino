#include <WiFi.h>
#include <WebServer.h>

// Replace with your network credentials
const char* ssid = "MyProject";
const char* password = "12345678";

// Define GPIO pins for Motor A
const int IN1 = 13;
const int IN2 = 12;

// Define GPIO pins for Motor B
const int IN3 = 14;
const int IN4 = 27;

// Create a WebServer object that listens on port 80
WebServer server(80);

// Function declarations
void handleRoot();
void handleForward();
void handleBackward();
void handleLeft();
void handleRight();
void handleStop();
void handleNotFound();

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Initialize Motor Control Pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  // Initialize motors to stop
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  
  Serial.println();
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
  
  // Define web server routes
  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);
  
  // Handle undefined routes
  server.onNotFound(handleNotFound);
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

// Function to generate the HTML webpage with enhanced UI
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>ESP32 2-Wheel Bot Control</title>";
  html += "<style>";
  
  // CSS Reset
  html += "body, html { margin: 0; padding: 0; height: 100%; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f0f0; }";
  
  // Container styling
  html += ".container { display: flex; flex-direction: column; justify-content: center; align-items: center; height: 100%; }";
  
  // Title styling
  html += "h1 { color: #333; margin-bottom: 50px; }";
  
  // Button container
  html += ".button-grid { display: grid; grid-template-columns: repeat(3, 100px); grid-template-rows: repeat(3, 100px); gap: 20px; }";
  
  // Button styling
  html += ".control-button { width: 100px; height: 100px; border: none; border-radius: 10px; background: #4CAF50; color: white; font-size: 18px; cursor: pointer; box-shadow: 0 4px #999; transition: background 0.3s, transform 0.1s; }";
  
  // Button hover effect
  html += ".control-button:hover { background: #45a049; }";
  
  // Button active effect
  html += ".control-button:active { transform: translateY(4px); box-shadow: 0 2px #666; }";
  
  // Specific button styles
  html += ".forward { grid-column: 2 / 3; grid-row: 1 / 2; }";
  html += ".left { grid-column: 1 / 2; grid-row: 2 / 3; }";
  html += ".stop { grid-column: 2 / 3; grid-row: 2 / 3; background: #f44336; }";
  html += ".right { grid-column: 3 / 4; grid-row: 2 / 3; }";
  html += ".backward { grid-column: 2 / 3; grid-row: 3 / 4; }";
  
  // Stop button specific color
  html += ".stop { background: #f44336; }";
  
  // Responsive adjustments
  html += "@media (max-width: 600px) { .button-grid { grid-template-columns: repeat(3, 80px); grid-template-rows: repeat(3, 80px); gap: 15px; } .control-button { width: 80px; height: 80px; font-size: 16px; } }";
  
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"container\">";
  html += "<h1>2-Wheel Bot Control</h1>";
  html += "<div class=\"button-grid\">";
  html += "<button class=\"control-button forward\" onclick=\"location.href='/forward'\">&#8679;</button>";
  html += "<button class=\"control-button left\" onclick=\"location.href='/left'\">&#8678;</button>";
  html += "<button class=\"control-button stop\" onclick=\"location.href='/stop'\">&#9632;</button>";
  html += "<button class=\"control-button right\" onclick=\"location.href='/right'\">&#8680;</button>";
  html += "<button class=\"control-button backward\" onclick=\"location.href='/backward'\">&#8681;</button>";
  html += "</div>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Movement Control Functions
void handleForward() {
  Serial.println("Moving Forward");
  
  // Motor A
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  
  // Motor B
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleBackward() {
  Serial.println("Moving Backward");
  
  // Motor A
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  
  // Motor B
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLeft() {
  Serial.println("Turning Left");
  
  // Motor A
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  
  // Motor B
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRight() {
  Serial.println("Turning Right");
  
  // Motor A
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  
  // Motor B
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStop() {
  Serial.println("Stopping");
  
  // Motor A
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  
  // Motor B
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

// Handle undefined routes
void handleNotFound(){
  String message = "404: Not found\n";
  server.send(404, "text/plain", message);
}
