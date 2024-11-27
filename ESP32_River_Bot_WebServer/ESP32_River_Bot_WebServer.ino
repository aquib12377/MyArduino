#include <WiFi.h>
#include <WebServer.h>

// ======= Wi-Fi Credentials =======
const char* ssid = "MyProject";       // Replace with your Wi-Fi SSID
const char* password = "12345678";    // Replace with your Wi-Fi Password

// ======= Movement Motor Pins =======
// Motor A
const int MOVE_IN1 = 13;
const int MOVE_IN2 = 12;

// Motor B
const int MOVE_IN3 = 14;
const int MOVE_IN4 = 27;

// ======= Conveyor Belt Motor Pins =======
const int CONV_IN1 = 25;
const int CONV_IN2 = 26;

// ======= Web Server Setup =======
WebServer server(80);

// ======= Function Declarations =======
void handleRoot();
void handleForward();
void handleBackward();
void handleLeft();
void handleRight();
void handleStop();
void handleConveyorStart();
void handleConveyorStop();
void handleNotFound();

// ======= Setup Function =======
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Initialize Movement Motor Control Pins
  pinMode(MOVE_IN1, OUTPUT);
  pinMode(MOVE_IN2, OUTPUT);
  
  pinMode(MOVE_IN3, OUTPUT);
  pinMode(MOVE_IN4, OUTPUT);
  
  // Initialize Conveyor Motor Control Pins
  pinMode(CONV_IN1, OUTPUT);
  pinMode(CONV_IN2, OUTPUT);
  
  // Initialize all motors to stop
  digitalWrite(MOVE_IN1, LOW);
  digitalWrite(MOVE_IN2, LOW);
  
  digitalWrite(MOVE_IN3, LOW);
  digitalWrite(MOVE_IN4, LOW);
  
  digitalWrite(CONV_IN1, LOW);
  digitalWrite(CONV_IN2, LOW);
  
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
  
  // Movement Controls
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);
  
  // Conveyor Controls
  server.on("/conveyorStart", handleConveyorStart);
  server.on("/conveyorStop", handleConveyorStop);
  
  // Handle undefined routes
  server.onNotFound(handleNotFound);
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

// ======= Loop Function =======
void loop() {
  server.handleClient();
}

// ======= Webpage Handler =======
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<title>River Cleaning Bot Control</title>";
  html += "<style>";
  
  // CSS Styling
  html += "body, html { margin: 0; padding: 0; height: 100%; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #e0f7fa; }";
  html += ".container { display: flex; flex-direction: column; justify-content: center; align-items: center; height: 100%; }";
  html += "h1 { color: #006064; margin-bottom: 30px; }";
  
  // Button Grid
  html += ".button-grid { display: grid; grid-template-columns: repeat(3, 80px); grid-template-rows: repeat(3, 80px); gap: 15px; }";
  
  // Control Buttons
  html += ".control-button { width: 80px; height: 80px; border: none; border-radius: 10px; background: #00838f; color: white; font-size: 24px; cursor: pointer; box-shadow: 0 4px #666; transition: background 0.3s, transform 0.1s; }";
  
  // Hover Effect
  html += ".control-button:hover { background: #006064; }";
  
  // Active Effect
  html += ".control-button:active { transform: translateY(4px); box-shadow: 0 2px #666; }";
  
  // Specific Button Styles
  html += ".forward { grid-column: 2 / 3; grid-row: 1 / 2; }";
  html += ".left { grid-column: 1 / 2; grid-row: 2 / 3; }";
  html += ".stop { grid-column: 2 / 3; grid-row: 2 / 3; background: #d32f2f; }";
  html += ".right { grid-column: 3 / 4; grid-row: 2 / 3; }";
  html += ".backward { grid-column: 2 / 3; grid-row: 3 / 4; }";
  
  // Conveyor Controls
  html += ".conveyor-controls { margin-top: 40px; }";
  html += ".conveyor-button { width: 120px; height: 50px; margin: 10px; border: none; border-radius: 10px; background: #004d40; color: white; font-size: 18px; cursor: pointer; box-shadow: 0 4px #666; transition: background 0.3s, transform 0.1s; }";
  html += ".conveyor-button:hover { background: #00332e; }";
  html += ".conveyor-button:active { transform: translateY(4px); box-shadow: 0 2px #666; }";
  
  // Responsive Design
  html += "@media (max-width: 600px) { .button-grid { grid-template-columns: repeat(3, 60px); grid-template-rows: repeat(3, 60px); gap: 10px; } .control-button { width: 60px; height: 60px; font-size: 20px; } .conveyor-button { width: 100px; height: 40px; font-size: 16px; } }";
  
  html += "</style>";
  html += "</head>";
  
  html += "<body>";
  html += "<div class=\"container\">";
  html += "<h1>River Cleaning Bot Control</h1>";
  
  // Movement Control Buttons
  html += "<div class=\"button-grid\">";
  html += "<button class=\"control-button forward\" onclick=\"location.href='/forward'\">&#8679;</button>";    // Up Arrow
  html += "<button class=\"control-button left\" onclick=\"location.href='/left'\">&#8678;</button>";          // Left Arrow
  html += "<button class=\"control-button stop\" onclick=\"location.href='/stop'\">&#9632;</button>";        // Stop Square
  html += "<button class=\"control-button right\" onclick=\"location.href='/right'\">&#8680;</button>";       // Right Arrow
  html += "<button class=\"control-button backward\" onclick=\"location.href='/backward'\">&#8681;</button>"; // Down Arrow
  html += "</div>";
  
  // Conveyor Belt Controls
  html += "<div class=\"conveyor-controls\">";
  html += "<button class=\"conveyor-button\" onclick=\"location.href='/conveyorStart'\">Start Conveyor</button>";
  html += "<button class=\"conveyor-button\" onclick=\"location.href='/conveyorStop'\">Stop Conveyor</button>";
  html += "</div>";
  
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// ======= Movement Control Functions =======

void handleForward() {
  Serial.println("Moving Forward");
  
  // Motor A Forward
  digitalWrite(MOVE_IN1, HIGH);
  digitalWrite(MOVE_IN2, LOW);
  
  // Motor B Forward
  digitalWrite(MOVE_IN3, HIGH);
  digitalWrite(MOVE_IN4, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleBackward() {
  Serial.println("Moving Backward");
  
  // Motor A Backward
  digitalWrite(MOVE_IN1, LOW);
  digitalWrite(MOVE_IN2, HIGH);
  
  // Motor B Backward
  digitalWrite(MOVE_IN3, LOW);
  digitalWrite(MOVE_IN4, HIGH);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleLeft() {
  Serial.println("Turning Left");
  
  // Motor A Backward
  digitalWrite(MOVE_IN1, LOW);
  digitalWrite(MOVE_IN2, HIGH);
  
  // Motor B Forward
  digitalWrite(MOVE_IN3, HIGH);
  digitalWrite(MOVE_IN4, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRight() {
  Serial.println("Turning Right");
  
  // Motor A Forward
  digitalWrite(MOVE_IN1, HIGH);
  digitalWrite(MOVE_IN2, LOW);
  
  // Motor B Backward
  digitalWrite(MOVE_IN3, LOW);
  digitalWrite(MOVE_IN4, HIGH);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStop() {
  Serial.println("Stopping");
  
  // Stop Motor A
  digitalWrite(MOVE_IN1, LOW);
  digitalWrite(MOVE_IN2, LOW);
  
  // Stop Motor B
  digitalWrite(MOVE_IN3, LOW);
  digitalWrite(MOVE_IN4, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

// ======= Conveyor Belt Control Functions =======

void handleConveyorStart() {
  Serial.println("Starting Conveyor Belt");
  
  // Conveyor Motor Forward
  digitalWrite(CONV_IN1, HIGH);
  digitalWrite(CONV_IN2, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleConveyorStop() {
  Serial.println("Stopping Conveyor Belt");
  
  // Stop Conveyor Motor
  digitalWrite(CONV_IN1, LOW);
  digitalWrite(CONV_IN2, LOW);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

// ======= Handle 404 Not Found =======
void handleNotFound(){
  String message = "404: Not found\n";
  server.send(404, "text/plain", message);
}
