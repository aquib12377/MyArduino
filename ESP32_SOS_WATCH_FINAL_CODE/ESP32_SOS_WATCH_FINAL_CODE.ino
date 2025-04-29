#include <Wire.h>
#include <WiFi.h>
#include "time.h"
#include <BluetoothSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MAX30100_PulseOximeter.h>
#include <MPU6050_light.h>

// ====== PIN DEFINITIONS ======
#define RELAY_PIN       13     // active LOW relay
#define BUTTON_PIN      14    // SOS long-press button
#define SDA_PIN         21
#define SCL_PIN         22
#define OLED_RESET      -1
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// ====== NTP CONFIG ======
const char* ssid              = "MyProject";
const char* password          = "12345678";
const char* ntpServer         = "pool.ntp.org";
const long  gmtOffset_sec     = 5 * 3600 + 30 * 60;
const int   daylightOffset_sec= 0;

// ====== SOS / PIN CONFIG ======
const String correctPin       = "1234";
bool sosActive                = false;
unsigned long sosStartTime    = 0;
bool smsSent                  = false;

// ====== FREE-FALL & BUTTON ======
const float freeFallThreshold = 0.2;
MPU6050 mpu(Wire);
unsigned long btnPressStart   = 0;

// ====== HEART RATE ======
PulseOximeter pox;
float lastHeartRate           = 0;

// ====== DISPLAY & LED ======
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

BluetoothSerial SerialBT;

void initializeWiFiAndTime() {
  Serial.println("[WiFi] Connecting to " + String(ssid) + " …");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
  }
  Serial.println("\n[WiFi] Connected, IP: " + WiFi.localIP().toString());

  Serial.println("[NTP] Configuring time…");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.printf("[NTP] Time set: %02d:%02d:%02d\n",
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    Serial.println("[NTP] Failed to get time");
  }
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--:--";
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buf);
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Time:");
  display.setCursor(0, 24);
  display.print(getTimeString());
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("HR:");
  display.print(int(lastHeartRate));
  display.display();
  Serial.printf("[Display] %s | HR: %.0f\n", getTimeString().c_str(), lastHeartRate);
}

void setup() {
  Serial.begin(115200);
  while(!Serial) { delay(10); }
  Serial.println("\n=== STARTUP ===");

  // Bluetooth
  SerialBT.begin("ESP32_Watch");
  Serial.println("[BT] BluetoothSerial started");

  // LEDs
  Serial.println("[LED] WS2812 initialized");

  // Relay & Button
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("[I/O] Relay OFF (HIGH), Button PULLUP");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("[I2C] Wire initialized");

  // MPU6050
  mpu.begin();
  mpu.calcGyroOffsets();
  Serial.println("[MPU] Initialized and gyro offsets set");

  // MAX30100
  if (!pox.begin()) {
    Serial.println("[HR] MAX30100 init failed!");
    while (1) delay(100);
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  Serial.println("[HR] MAX30100 initialized");

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OLED] SSD1306 init failed!");
    while (1) delay(100);
  }
  Serial.println("[OLED] SSD1306 initialized");

  // WiFi + Time
  initializeWiFiAndTime();
}

void loop() {
  unsigned long now = millis();

  // —— Heartbeat ——
  pox.update();
  lastHeartRate = pox.getHeartRate();

 

  // —— Update Display ——
  static unsigned long lastDisplay = 0;
  if (now - lastDisplay >= 1000) {
    lastDisplay = now;
    updateDisplay();
  }

  // —— Free-fall detection ——
  mpu.update();
  float ax = mpu.getAccX(), ay = mpu.getAccY(), az = mpu.getAccZ();
  float mag = sqrt(ax*ax + ay*ay + az*az);
  if (!sosActive && mag < freeFallThreshold) {
    sosActive      = true;
    smsSent        = false;
    sosStartTime   = now;
    digitalWrite(RELAY_PIN, LOW);
    Serial.printf("[SOS] FREEFALL detected (g=%.2f)! Relay ON, prompt PIN\n", mag);
    SerialBT.println("FREEFALL DETECTED! ENTER PIN:");
  }

  if(SerialBT.available())
  {
    String receivedData = Serial.readStringUntil('\n');
    Serial.println("Received from APP: "+receivedData);
    if(receivedData.indexOf("APP_SOS") > 0)
    {
      sosActive      = true;
      smsSent        = false;
      sosStartTime   = now;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("[SOS] SOS From App! Relay ON, prompt PIN");
      SerialBT.println("BUTTON SOS! ENTER PIN:");
    }
  }
  // —— Button long-press ——
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (btnPressStart == 0) {
      btnPressStart = now;
      Serial.println("[BUTTON] Press started");
    }
    else if (!sosActive && now - btnPressStart >= 10000) {
      sosActive      = true;
      smsSent        = false;
      sosStartTime   = now;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("[SOS] BUTTON long-press 60s! Relay ON, prompt PIN");
      SerialBT.println("BUTTON SOS! ENTER PIN:");
    }
  } else {
    if (btnPressStart > 0) {
      Serial.println("[BUTTON] Released before 60s");
    }
    btnPressStart = 0;
  }

  // —— PIN entry via BT ——
  if (sosActive && SerialBT.available()) {
    String entry = SerialBT.readStringUntil('\n');
    entry.trim();
    Serial.println("[BT] PIN received: " + entry);
    if (entry == correctPin) {
      sosActive = false;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("[SOS] Correct PIN! Relay OFF, SOS canceled");
      SerialBT.println("PIN OK. SOS CANCELED.");
    } else {
      Serial.println("[SOS] Wrong PIN");
      SerialBT.println("WRONG PIN. TRY AGAIN:");
    }
  }

  // —— SMS fallback ——
  if (sosActive && !smsSent && now - sosStartTime >= 60000) {
    smsSent = true;
    Serial.printf("[SMS] Timeout → SEND_SMS:HR=%d\n", int(lastHeartRate));
    SerialBT.printf("SEND_SMS:HR=%d\n", int(lastHeartRate));
    SerialBT.println("EMERGENCY! SOS MESSAGE SENT.");
  }
}
