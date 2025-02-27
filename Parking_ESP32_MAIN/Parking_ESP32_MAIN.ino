/************ Include necessary libraries ************/
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- RFID Libraries ---
#include <SPI.h>
#include <MFRC522.h>

/************ Wi-Fi credentials ************/
const char* ssid     = "MyProject";
const char* password = "12345678";

/************ Define pins ************/
// Remove entry/exit IR sensor pins => no longer used

// Parking slot IR sensors (unchanged)
#define SLOT_IR_1  25
#define SLOT_IR_2  26
#define SLOT_IR_3  27
#define SLOT_IR_4  14

// Servo control pin
#define SERVO_PIN   12

// Pin to trigger ESP32-CAM? (Not strictly necessary if we’re only sending UID over Serial)
// If you don’t need it, you can remove it. Kept here in case you still want to pulse it.
#define CAM_TRIGGER_PIN  13

/************ RFID Pins ************/
// Example pins for RC522
#define RFID_SS_PIN  5
#define RFID_RST_PIN 4

/************ Global Variables ************/
// WebServer on port 80
WebServer server(80);

// Create a servo object
Servo gateServo;

// Parking slot states (0 = free, 1 = occupied)
int slotStates[4] = {0, 0, 0, 0};

// Create an LCD object at I2C address 0x27 with 16 columns and 2 rows.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Create MFRC522 instance
MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);

/*********************************************************
   Helper function: Convert RFID UID bytes to a String
*********************************************************/
String getUidString(MFRC522 &rfid) {
  String uidStr = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uidStr += "0";
    }
    uidStr += String(rfid.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();
  return uidStr;
}

/************ Function to check slot IR states ************/
void updateSlotStates() {
  // Assuming LOW = blocked = occupied
  slotStates[0] = (digitalRead(SLOT_IR_1) == LOW) ? 1 : 0;
  slotStates[1] = (digitalRead(SLOT_IR_2) == LOW) ? 1 : 0;
  slotStates[2] = (digitalRead(SLOT_IR_3) == LOW) ? 1 : 0;
  slotStates[3] = (digitalRead(SLOT_IR_4) == LOW) ? 1 : 0;
}

/************ Function to update LCD UI ************/
void updateLCDUI() {
  lcd.clear();
  // First row: S1 and S2
  lcd.setCursor(0, 0);
  lcd.print("S1:");
  lcd.print(slotStates[0] ? "Occ" : "Free");
  lcd.print(" S2:");
  lcd.print(slotStates[1] ? "Occ" : "Free");

  // Second row: S3 and S4
  lcd.setCursor(0, 1);
  lcd.print("S3:");
  lcd.print(slotStates[2] ? "Occ" : "Free");
  lcd.print(" S4:");
  lcd.print(slotStates[3] ? "Occ" : "Free");
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
      "<meta http-equiv='refresh' content='5'/>"
      "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css' rel='stylesheet'/>"
      "<style>"
        ".occupied { color: #dc3545; font-weight: 600; }"
        ".free { color: #28a745; font-weight: 600; }"
      "</style>"
    "</head>"
    "<body class='bg-light'>"
      "<div class='container my-5'>"
        "<h1 class='text-center mb-4'>Smart Parking Slot Status</h1>"
        "<div class='row justify-content-center'>";

  // Generate a card for each slot
  for (int i = 0; i < 4; i++) {
    String slotNumber = String(i + 1);
    bool occupied = (slotStates[i] == 1);
    String statusText = occupied ? "Occupied" : "Available";
    String statusClass = occupied ? "occupied" : "free";

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

  htmlPage +=
        "</div>"
      "</div>"
      "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js'></script>"
    "</body>"
    "</html>";

  server.send(200, "text/html", htmlPage);
}

/************ Setup ************/
void setup() {
  Serial.begin(115200);    // For debugging

  // OPTIONAL: Use Serial2 to send data to ESP32-CAM
  // Example: TX2 -> 17, RX2 -> 16, or adjust to your wiring
  Serial2.begin(115200, SERIAL_8N1, 16, 17); // <-- Adjust pins as needed

  // Initialize SPI + RC522
  SPI.begin(18, 19, 23, RFID_SS_PIN); // (SCK=18, MISO=19, MOSI=23, SS=RFID_SS_PIN)
  mfrc522.PCD_Init();

  // Pin modes for parking slot IR sensors
  pinMode(SLOT_IR_1, INPUT_PULLUP);
  pinMode(SLOT_IR_2, INPUT_PULLUP);
  pinMode(SLOT_IR_3, INPUT_PULLUP);
  pinMode(SLOT_IR_4, INPUT_PULLUP);

  // Pin mode for CAM trigger (if needed)
  pinMode(CAM_TRIGGER_PIN, OUTPUT);
  digitalWrite(CAM_TRIGGER_PIN, LOW);

  // Attach servo
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); // Initialize gate in closed position

  // Initialize LCD
  lcd.begin();      // I2C LCD init
  lcd.backlight();
  lcd.clear();

  // Connect to Wi-Fi
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

  // Check if an RFID tag is present
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Attempt to read the card
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // We have a new card; get its UID as a String
  String uidStr = getUidString(mfrc522);
  Serial.println("RFID UID detected: " + uidStr);

  // OPTIONAL: Trigger the camera pin if you want
  // digitalWrite(CAM_TRIGGER_PIN, HIGH);
  // delay(100);
  // digitalWrite(CAM_TRIGGER_PIN, LOW);

  // Send the UID to the ESP32-CAM via Serial2
  // (Your ESP32-CAM code must listen on Serial2 for this string)
  Serial2.println(uidStr);

  // For demonstration, open the gate for 3 seconds, then close
  gateServo.write(90);
  delay(3000);
  gateServo.write(0);

  // Halt the card so it doesn’t continually get read
  mfrc522.PICC_HaltA();

  // Update the LCD once in a while (or every time, up to you)
  updateSlotStates();
  updateLCDUI();
}
