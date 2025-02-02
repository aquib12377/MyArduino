/************ Include necessary libraries ************/
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ESP32Servo.h>
#include <time.h>       // For timestamp if required

// I2C LCD libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/************ Wi-Fi credentials ************/
const char* ssid     = "MyProject";
const char* password = "12345678";

/************ Define pins ************/
// IR sensors for entry/exit
#define IR_SENSOR_1  32   // Example pin
#define IR_SENSOR_2  33   // Example pin

// Parking slot IR sensors
#define SLOT_IR_1  25
#define SLOT_IR_2  26
#define SLOT_IR_3  27
#define SLOT_IR_4  14

// Servo control pin
#define SERVO_PIN   5

// Pin to trigger ESP32-CAM to take a picture
#define CAM_TRIGGER_PIN  4

/************ Global Variables ************/
// For detecting entry/exit direction
int lastSensorTriggered = 0; // 0 = none, 1 = IR1, 2 = IR2

// WebServer on port 80
WebServer server(80);

// Create a servo object
Servo gateServo;

// Parking slot states (0 = free, 1 = occupied)
int slotStates[4] = {0, 0, 0, 0};

// Create an LCD object at I2C address 0x27 with 16 columns and 2 rows.
// Adjust if your LCD has a different I2C address (e.g., 0x3F) or size (e.g., 20x4).
LiquidCrystal_I2C lcd(0x27, 16, 2);

/************ Function to check slot IR states ************/
void updateSlotStates() {
  slotStates[0] = (digitalRead(SLOT_IR_1) == LOW) ? 1 : 0; // Assuming LOW means blocked/occupied
  slotStates[1] = (digitalRead(SLOT_IR_2) == LOW) ? 1 : 0;
  slotStates[2] = (digitalRead(SLOT_IR_3) == LOW) ? 1 : 0;
  slotStates[3] = (digitalRead(SLOT_IR_4) == LOW) ? 1 : 0;
}

/************ Function to update LCD UI ************/
void updateLCDUI() {
  // Clear and set cursor to top-left
  lcd.clear();
  
  // First row: S1 and S2
  lcd.setCursor(0, 0);
  lcd.print("S1:");
  lcd.print(slotStates[0] == 1 ? "Occ" : "Free");
  lcd.print(" S2:");
  lcd.print(slotStates[1] == 1 ? "Occ" : "Free");
  
  // Second row: S3 and S4
  lcd.setCursor(0, 1);
  lcd.print("S3:");
  lcd.print(slotStates[2] == 1 ? "Occ" : "Free");
  lcd.print(" S4:");
  lcd.print(slotStates[3] == 1 ? "Occ" : "Free");
}

/************ Function to handle root page ************/
void handleRoot() {
  // Update slots before generating the page
  updateSlotStates();

  // Generate a simple HTML page
 String htmlPage = 
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
      "<meta charset='UTF-8' />"
      "<meta name='viewport' content='width=device-width, initial-scale=1' />"
      "<title>Smart Parking</title>"
      // Optional auto-refresh every 5 seconds; remove if not needed.
      "<meta http-equiv='refresh' content='5'/>"
      // Load Bootstrap CSS from CDN
      "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css' rel='stylesheet'/>"
      "<style>"
      // A tiny style tweak, for example
      ".occupied { color: #dc3545; font-weight: 600; }"  // red text
      ".free { color: #28a745; font-weight: 600; }"      // green text
      "</style>"
    "</head>"
    "<body class='bg-light'>"
      "<div class='container my-5'>"
        "<h1 class='text-center mb-4'>Smart Parking Slot Status</h1>"
        "<div class='row justify-content-center'>";

  // Generate a card for each slot
  for (int i = 0; i < 4; i++) {
    String slotNumber = String(i + 1);
    String statusText = (slotStates[i] == 1) ? "Occupied" : "Available";
    String statusClass = (slotStates[i] == 1) ? "occupied" : "free";

    htmlPage += 
          "<div class='col-sm-6 col-md-3 mb-3'>"
            "<div class='card shadow-sm'>"
              "<div class='card-body text-center'>"
                "<h5 class='card-title'>Slot " + slotNumber + "</h5>"
                "<p class='card-text " + statusClass + "'>" + statusText + "</p>"
              "</div>"
            "</div>"
          "</div>";
  }

  // Close the row and container
  htmlPage += 
        "</div>"  // end .row
      "</div>"    // end .container
      // Load Bootstrap JS bundle (not strictly necessary for this simple page)
      "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js'></script>"
    "</body>"
    "</html>";

  server.send(200, "text/html", htmlPage);
}

/************ Function to send dummy data ************/
void sendVehicleData(const char* vehicleName, const char* vehicleNumber, const char* entryTime) {
  Serial.println("Sending Vehicle Data:");
  Serial.println(String("Name: ") + vehicleName);
  Serial.println(String("Number: ") + vehicleNumber);
  Serial.println(String("Time: ") + entryTime);

  // (Pseudo code for sending data to a server or Firebase)
}

/************ Setup ************/
void setup() {
  Serial.begin(115200);

  // Pin modes for IR sensors
  pinMode(IR_SENSOR_1, INPUT_PULLUP);
  pinMode(IR_SENSOR_2, INPUT_PULLUP);

  // Pin modes for parking slot IR sensors
  pinMode(SLOT_IR_1, INPUT_PULLUP);
  pinMode(SLOT_IR_2, INPUT_PULLUP);
  pinMode(SLOT_IR_3, INPUT_PULLUP);
  pinMode(SLOT_IR_4, INPUT_PULLUP);

  // Pin mode for CAM trigger
  pinMode(CAM_TRIGGER_PIN, OUTPUT);
  digitalWrite(CAM_TRIGGER_PIN, LOW);

  // Attach servo
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // Initialize gate in closed position

  // Initialize LCD
  lcd.begin();         // Initialize the I2C LCD
  lcd.backlight();    // Turn on the LCD backlight
  lcd.clear();        // Clear the display

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Start MDNS (optional)
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  // Setup WebServer routes
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started on port 80");
}

/************ Loop ************/
void loop() {
  // Handle incoming web requests
  server.handleClient();

  // (1) Read the two IR sensors for entry/exit logic
  bool ir1State = (digitalRead(IR_SENSOR_1) == LOW);
  bool ir2State = (digitalRead(IR_SENSOR_2) == LOW);

  // (2) Check for first sensor triggered
  if (ir1State && lastSensorTriggered == 0) {
    lastSensorTriggered = 1;  // IR1 triggered first
  }
  if (ir2State && lastSensorTriggered == 0) {
    lastSensorTriggered = 2;  // IR2 triggered first
  }

  // (3) If IR1 -> IR2 => Entry
  if (ir2State && lastSensorTriggered == 1) {
    Serial.println("== Vehicle ENTRY Detected ==");

    // Send dummy vehicle data
    String currentTime = String(millis()); // or use a real timestamp from RTC
    sendVehicleData("DummyVehicle", "ABC-1234", currentTime.c_str());

    // Trigger ESP32-CAM
    digitalWrite(CAM_TRIGGER_PIN, HIGH);
    delay(500);
    digitalWrite(CAM_TRIGGER_PIN, LOW);

    // Open gate via servo
    gateServo.write(90);
    delay(3000);
    gateServo.write(0);

    lastSensorTriggered = 0; // Reset
  }

  // (4) If IR2 -> IR1 => Exit
  if (ir1State && lastSensorTriggered == 2) {
    Serial.println("== Vehicle EXIT Detected ==");
    // Optionally send exit data

    lastSensorTriggered = 0; // Reset
  }

  // (5) Update the LCD UI every 5 seconds (non-blocking approach)
  static unsigned long lastLcdUpdate = 0;
  if (millis() - lastLcdUpdate > 5000) {
    lastLcdUpdate = millis();

    // Update slot states and refresh the LCD content
    updateSlotStates();
    updateLCDUI();
  }
}
