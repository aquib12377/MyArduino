/*
  ESP32 Multi-Sensor Telegram Alert System
  Components: MQ2, MQ135, DHT11, MPU6050, OLED 0.96", Buzzer, Push Button
  Alerts via Telegram on: High Gas, High Temp, Fall Detection, Emergency Button
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>

// ===================== WiFi & Telegram Config =====================
const char* ssid     = "AAA";
const char* password = "Acube@123";

#define BOTtoken  "8559211703:AAFvCMDG_iiK63Gr9dK8YAF4WAdsre5hWaQ"
#define CHAT_ID   "6755574619"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// ===================== Pin Definitions =====================
#define MQ2_PIN       34    // Analog - Gas / Smoke sensor
#define MQ135_PIN     35    // Analog - Air quality sensor
#define DHT_PIN       5     // Digital - DHT11
#define BUZZER_PIN    4     // Digital - Active Buzzer
#define BUTTON_PIN    33    // Digital - Emergency Push Button

// I2C pins (default ESP32: SDA=21, SCL=22)
// MPU6050 and OLED share I2C bus

// ===================== Sensor Config =====================
#define DHT_TYPE      DHT11

#define MQ2_THRESHOLD    400   // Raw ADC value (0-4095) — adjust per calibration
#define MQ135_THRESHOLD  500   // Raw ADC value — adjust per calibration
#define TEMP_THRESHOLD   35.0  // Celsius
#define HUM_THRESHOLD    80.0  // %RH (optional warning)

// Fall detection: sudden spike then near-zero in accel magnitude
#define FALL_THRESHOLD_HIGH  2.5   // g — free fall or impact spike
#define FALL_THRESHOLD_LOW   0.4   // g — near-zero gravity (free fall phase)

// ===================== OLED Config =====================
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ===================== Objects =====================
DHT dht(DHT_PIN, DHT_TYPE);
MPU6050 mpu;

// ===================== State Variables =====================
unsigned long lastAlertTime    = 0;
unsigned long lastSensorRead   = 0;
const unsigned long ALERT_COOLDOWN   = 30000; // 30 sec between same alert
const unsigned long SENSOR_INTERVAL  = 2000;  // read sensors every 2 sec

bool gasAlertSent    = false;
bool airAlertSent    = false;
bool tempAlertSent   = false;
bool fallAlertSent   = false;
bool buttonAlertSent = false;

// For fall detection: track previous accel magnitude
float prevAccelMag = 1.0;
unsigned long fallDetectTime = 0;

// ===================== Helper: Send Alert =====================
void sendAlert(String message) {
  Serial.println("Sending: " + message);
  bot.sendMessage(CHAT_ID, message, "");
  buzzAlert();
}

// ===================== Helper: Buzzer =====================
void buzzAlert() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
  }
}

// ===================== OLED Display =====================
void showOLED(String line1, String line2 = "", String line3 = "", String line4 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);  display.println(line1);
  display.setCursor(0, 16); display.println(line2);
  display.setCursor(0, 32); display.println(line3);
  display.setCursor(0, 48); display.println(line4);

  display.display();
}

void showAlert(String alertType) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("!! ALERT !!");
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.println(alertType);
  display.display();
}

// ===================== Fall Detection =====================
bool detectFall(float ax, float ay, float az) {
  float mag = sqrt(ax * ax + ay * ay + az * az);

  // Phase 1: sudden spike or drop (impact or free fall)
  if (mag < FALL_THRESHOLD_LOW) {
    fallDetectTime = millis();
  }

  // Phase 2: impact spike within 500ms of free fall
  if ((millis() - fallDetectTime < 500) && (mag > FALL_THRESHOLD_HIGH)) {
    fallDetectTime = 0; // reset
    return true;
  }

  return false;
}

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);

  // Pin modes
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // LOW when pressed

  // DHT
  dht.begin();

  // I2C
  Wire.begin();

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED init failed!");
  }
  display.clearDisplay();
  showOLED("Initializing...", "Please wait");

  // MPU6050
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    showOLED("MPU6050", "NOT FOUND!");
    delay(2000);
  }

  // WiFi
  showOLED("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  int wifiTry = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTry < 20) {
    delay(500);
    Serial.print(".");
    wifiTry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
    showOLED("WiFi Connected!", WiFi.localIP().toString());
    delay(1500);
    bot.sendMessage(CHAT_ID, "ESP32 Alert System Online!", "");
  } else {
    Serial.println("\nWiFi FAILED - running offline");
    showOLED("WiFi FAILED!", "Alerts disabled");
    delay(2000);
  }
}

// ===================== Loop =====================
void loop() {
  unsigned long now = millis();

  // ---- Read MPU6050 continuously for fall detection ----
  int16_t ax_raw, ay_raw, az_raw, gx, gy, gz;
  mpu.getMotion6(&ax_raw, &ay_raw, &az_raw, &gx, &gy, &gz);

  // Convert to g (MPU6050 default ±2g → 16384 LSB/g)
  float ax = ax_raw / 16384.0;
  float ay = ay_raw / 16384.0;
  float az = az_raw / 16384.0;

  bool fallNow = detectFall(ax, ay, az);

  if (fallNow && !fallAlertSent) {
    String msg = "⚠️ FALL DETECTED!\nDevice may have fallen.\nAccel: " +
                 String(ax, 2) + "g, " + String(ay, 2) + "g, " + String(az, 2) + "g";
    sendAlert(msg);
    showAlert("FALL DETECTED!");
    fallAlertSent = true;
    lastAlertTime = now;
  } else if (!fallNow) {
    fallAlertSent = false;
  }

  // ---- Check Emergency Button ----
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonAlertSent) {
      String msg = "🆘 EMERGENCY BUTTON PRESSED!\nUser needs immediate assistance!";
      sendAlert(msg);
      showAlert("SOS - EMERGENCY!");
      buttonAlertSent = true;
    }
    delay(50); // debounce
  } else {
    buttonAlertSent = false;
  }

  // ---- Periodic Sensor Read ----
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    // --- DHT11 ---
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();

    if (isnan(temp)) temp = -1;
    if (isnan(hum))  hum  = -1;

    // --- MQ2 (Gas/Smoke) ---
    int mq2Val = analogRead(MQ2_PIN);

    // --- MQ135 (Air Quality / CO2) ---
    int mq135Val = analogRead(MQ135_PIN);

    Serial.printf("Temp=%.1fC Hum=%.1f%% MQ2=%d MQ135=%d\n",
                  temp, hum, mq2Val, mq135Val);

    bool anyAlert = false;

    // --- Gas Alert (MQ2) ---
    if (mq2Val > MQ2_THRESHOLD) {
      if (!gasAlertSent) {
        String msg = "🔥 HIGH GAS / SMOKE DETECTED!\nMQ2 Level: " + String(mq2Val) +
                     "\nPossible fire or gas leak. Evacuate immediately!";
        sendAlert(msg);
        showAlert("GAS ALERT!");
        gasAlertSent = true;
        anyAlert = true;
      }
    } else {
      gasAlertSent = false;
    }

    // --- Air Quality Alert (MQ135) ---
    if (mq135Val > MQ135_THRESHOLD) {
      if (!airAlertSent) {
        String msg = "🌫️ POOR AIR QUALITY DETECTED!\nMQ135 Level: " + String(mq135Val) +
                     "\nHigh CO2 / toxic gas concentration!";
        sendAlert(msg);
        showAlert("AIR QUALITY!");
        airAlertSent = true;
        anyAlert = true;
      }
    } else {
      airAlertSent = false;
    }

    // --- Temperature Alert ---
    if (temp > TEMP_THRESHOLD && temp != -1) {
      if (!tempAlertSent) {
        String msg = "🌡️ HIGH TEMPERATURE!\nTemp: " + String(temp, 1) + "°C\n" +
                     "Humidity: " + String(hum, 1) + "%\nPossible fire or heat hazard!";
        sendAlert(msg);
        showAlert("HIGH TEMP!");
        tempAlertSent = true;
        anyAlert = true;
      }
    } else {
      tempAlertSent = false;
    }

    // --- OLED Normal Display (if no alert) ---
    if (!anyAlert && !fallAlertSent && !buttonAlertSent) {
      String tempStr = (temp == -1) ? "Err" : (String(temp, 1) + "C");
      String humStr  = (hum == -1)  ? "Err" : (String(hum, 0) + "%");
      String mq2Str  = "Gas:" + String(mq2Val);
      String mq135Str= "Air:" + String(mq135Val);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      // Header
      display.setCursor(20, 0);
      display.setTextSize(1);
      display.println("All Systems OK");
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

      display.setCursor(0, 14);
      display.println("Temp : " + tempStr + "  Hum:" + humStr);

      display.setCursor(0, 26);
      display.println("MQ2  : " + String(mq2Val) +
                      (mq2Val > MQ2_THRESHOLD * 0.75 ? " WARN" : " OK"));

      display.setCursor(0, 38);
      display.println("MQ135: " + String(mq135Val) +
                      (mq135Val > MQ135_THRESHOLD * 0.75 ? " WARN" : " OK"));

      // Accel magnitude
      float mag = sqrt(ax * ax + ay * ay + az * az);
      display.setCursor(0, 50);
      display.println("Accel: " + String(mag, 2) + "g");

      display.display();
    }
  }

  delay(50);
}
