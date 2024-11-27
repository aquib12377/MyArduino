#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <NewPing.h>

// WiFi credentials
const char* ssid = "MyProject";          // Replace with your SSID
const char* password = "12345678";   // Replace with your password

// Motor pins
const int motor1Pin1 = 13; // Motor 1
const int motor1Pin2 = 12;
const int motor2Pin1 = 14; // Motor 2
const int motor2Pin2 = 27;

// Servo and ultrasonic pins
const int servoPin = 25;
const int trigPin = 5;
const int echoPin = 18;

// Ultrasonic sensor setup
NewPing sonar(trigPin, echoPin, 200); // Max distance 200 cm

// Create a web server object
WebServer server(80);
Servo myServo;

// Function prototypes
void handleRoot();
void handleMotorControl();
void handleServoControl();
void stopMotors();

void setup() {
    Serial.begin(115200);
    myServo.attach(servoPin);

    // Initialize motor pins
    pinMode(motor1Pin1, OUTPUT);
    pinMode(motor1Pin2, OUTPUT);
    pinMode(motor2Pin1, OUTPUT);
    pinMode(motor2Pin2, OUTPUT);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    // Define routes
    server.on("/", handleRoot);
    server.on("/motor", HTTP_GET, handleMotorControl);
    server.on("/servo", HTTP_GET, handleServoControl);

    // Start server
    server.begin();
    Serial.println("Server started");
}

void loop() {
    server.handleClient();

    // Check distance from ultrasonic sensor
    delay(50);
    int distance = sonar.ping_cm();
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    if(distance < 20)
    {
      stopMotors();
    }
}

void handleRoot() {
  String page = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<style>"
                  "body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; padding: 20px; }"
                  "h1 { color: #333; }"
                  "button { font-size: 18px; padding: 10px 20px; margin: 10px; background-color: #007bff; border: none; color: white; border-radius: 5px; cursor: pointer; }"
                  "button:hover { background-color: #0056b3; }"
                  ".container { max-width: 600px; margin: 0 auto; background-color: #fff; padding: 20px; border-radius: 10px; box-shadow: 0px 0px 15px rgba(0,0,0,0.1); }"
                  "</style></head><body>"
                  "<div class='container'>"
                  "<h1>Bot Control Panel</h1>"
                  "<h2>Motor Control</h2>"
                  "<button onclick=\"location.href='/motor?cmd=forward'\">Forward</button><br>"
                  "<button onclick=\"location.href='/motor?cmd=left'\">Left</button>"
                  "<button onclick=\"location.href='/motor?cmd=right'\">Right</button><br>"
                  "<button onclick=\"location.href='/motor?cmd=backward'\">Backward</button><br>"
                  "<button onclick=\"location.href='/motor?cmd=stop'\">Stop</button><br><br>"
                  "<h2>Servo Control</h2>"
                  "<button onclick=\"location.href='/servo?angle=0'\">0&deg;</button>"
                  "<button onclick=\"location.href='/servo?angle=90'\">90&deg;</button>"
                  "<button onclick=\"location.href='/servo?angle=180'\">180&deg;</button>"
                  "</div>"
                  "</body></html>";
    server.send(200, "text/html", page);
}

void handleMotorControl() {
    if (server.hasArg("cmd")) {
        String cmd = server.arg("cmd");
        if (cmd == "forward") {
            Serial.println("Moving forward");
            digitalWrite(motor1Pin1, HIGH);
            digitalWrite(motor1Pin2, LOW);
            digitalWrite(motor2Pin1, HIGH);
            digitalWrite(motor2Pin2, LOW);
        } else if (cmd == "backward") {
            Serial.println("Moving backward");
            digitalWrite(motor1Pin1, LOW);
            digitalWrite(motor1Pin2, HIGH);
            digitalWrite(motor2Pin1, LOW);
            digitalWrite(motor2Pin2, HIGH);
        } else if (cmd == "left") {
            Serial.println("Turning left");
            digitalWrite(motor1Pin1, LOW);
            digitalWrite(motor1Pin2, HIGH);
            digitalWrite(motor2Pin1, HIGH);
            digitalWrite(motor2Pin2, LOW);
        } else if (cmd == "right") {
            Serial.println("Turning right");
            digitalWrite(motor1Pin1, HIGH);
            digitalWrite(motor1Pin2, LOW);
            digitalWrite(motor2Pin1, LOW);
            digitalWrite(motor2Pin2, HIGH);
        } else if (cmd == "stop") {
            Serial.println("Stopping motors");
            stopMotors();
        }
    }
    server.send(200, "text/plain", "Motor command executed");
}

void handleServoControl() {
    if (server.hasArg("angle")) {
        int angle = server.arg("angle").toInt();
        if (angle >= 0 && angle <= 180) {
            myServo.write(angle);
            Serial.print("Servo moved to: ");
            Serial.println(angle);
        }
    }
    server.send(200, "text/plain", "Servo command executed");
}

void stopMotors() {
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    digitalWrite(motor2Pin1, LOW);
    digitalWrite(motor2Pin2, LOW);
}
