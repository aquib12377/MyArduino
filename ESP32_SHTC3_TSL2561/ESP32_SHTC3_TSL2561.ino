/*******************************************************
   Multi-sensor logger + dual-battery manager
   – One voltage divider on GPIO36 measures whichever
     battery is on the load.
   – 2×2 active-LOW relays:
       LOAD   : choose Batt-1 or Batt-2 for the system
       CHARGE : send the idle battery to the SCC
   – Swap as soon as active battery < 9 V
   – Print + Firebase upload every 60 s
********************************************************/

#include <Wire.h>
#include "Adafruit_SHTC3.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

/********************  GPIO ASSIGNMENTS  *****************/
#define RAIN_PIN            34
#define SOIL_PIN            35
#define BATT_PIN            36      // single divider → active battery

/* -------- 2-CH RELAY MODULE #1 :  LOAD  -------- */
#define RELAY_LOAD_B1_PIN   32      // LOW→Batt-1→LOAD
#define RELAY_LOAD_B2_PIN   33      // LOW→Batt-2→LOAD
/* -------- 2-CH RELAY MODULE #2 :  CHARGE -------- */
#define RELAY_CHG_B1_PIN    25      // LOW→Batt-1→SCC
#define RELAY_CHG_B2_PIN    26      // LOW→Batt-2→SCC

/********************  CONSTANTS  *****************/
const float DIVIDER   = 5.0;        // 30 kΩ : 7 .5 kΩ
const float V_SWITCH  = 9.0;        // V = swap threshold
const uint32_t REPORT_MS = 60'000;  // print & push period

/********************  WIFI / RTDB  *****************/
const char* ssid = "MyProject";
const char* pass = "12345678";
#define API_KEY      "AIzaSyD3Ijs8q_qkUSE8lvNd_Zvzr-uvdWBjISs"
#define DATABASE_URL "https://grapeguard-c7ad9-default-rtdb.firebaseio.com/"

/********************  OBJECTS  *********************/
Adafruit_SHTC3             shtc3;
Adafruit_TSL2561_Unified   tsl(TSL2561_ADDR_FLOAT, 12345);
FirebaseData   fbdo;
FirebaseConfig fbCfg;
FirebaseAuth   fbAuth;
WiFiUDP        ntpUDP;
NTPClient      timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);   // IST
esp_adc_cal_characteristics_t adc_chars;

/********************  STATE  ***********************/
bool shtc3_ok = false, tsl_ok = false;
bool batt1Active = true;                // start on Batt-1

/********************  HELPERS  *********************/
String nowStr()
{
  timeClient.update();
  time_t t = timeClient.getEpochTime();
  char buf[25];
  strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", localtime(&t));
  return String(buf);
}

float readBattery()
{
  uint32_t raw = analogRead(BATT_PIN);
  uint32_t mV  = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
  return (mV / 1000.0) * DIVIDER;
}

/* -------------- relay matrix -------------------- */
void applyRelays()
{
  if (batt1Active) {
    /* Batt-2 → LOAD   |  Batt-1 → CHARGE */
    digitalWrite(RELAY_LOAD_B1_PIN, LOW);
    digitalWrite(RELAY_LOAD_B2_PIN, LOW);
    digitalWrite(RELAY_CHG_B1_PIN, HIGH);
    digitalWrite(RELAY_CHG_B2_PIN, HIGH);  } else {
    /* Batt-1 → LOAD   |  Batt-2 → CHARGE */
    digitalWrite(RELAY_LOAD_B1_PIN, HIGH);    // energise
    digitalWrite(RELAY_LOAD_B2_PIN, HIGH);   // isolate
    digitalWrite(RELAY_CHG_B1_PIN, LOW);    // Batt-1 not charging
    digitalWrite(RELAY_CHG_B2_PIN, LOW);     // Batt-2 charging

  }
}

/* ------------ Firebase helper ------------------ */
void pushToFirebase(const String& ts,
                    float temp, float hum,
                    uint32_t lux, float rainPct, float soilPct,
                    float vBatt, uint8_t active)
{
  FirebaseJson j;
  j.set("time", ts);
  j.set("temp", temp);
  j.set("hum",  hum);
  j.set("lux",  lux);
  j.set("rain", rainPct);
  j.set("soil", soilPct);
  j.set("battV", vBatt);
  j.set("active", active);                 // 1 or 2

  if (Firebase.RTDB.pushJSON(&fbdo, "/envLogs", &j))
    Serial.println("✔ Firebase push OK");
  else {
    Serial.print("⚠ Firebase error: ");
    Serial.println(fbdo.errorReason());
  }
}

/********************  SETUP  ***********************/
void setup()
{
  Serial.begin(115200);

  pinMode(RAIN_PIN,  INPUT);
  pinMode(SOIL_PIN,  INPUT);
  pinMode(BATT_PIN,  INPUT);

  pinMode(RELAY_LOAD_B1_PIN, OUTPUT);
  pinMode(RELAY_LOAD_B2_PIN, OUTPUT);
  pinMode(RELAY_CHG_B1_PIN, OUTPUT);
  pinMode(RELAY_CHG_B2_PIN, OUTPUT);
  applyRelays();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  timeClient.begin();

  fbCfg.api_key      = API_KEY;
  fbCfg.database_url = DATABASE_URL;
  fbCfg.token_status_callback = tokenStatusCallback;
  Firebase.signUp(&fbCfg, &fbAuth, "", "");
  Firebase.begin(&fbCfg, &fbAuth);
  Firebase.reconnectWiFi(true);

  analogReadResolution(12);
  analogSetPinAttenuation(BATT_PIN, ADC_11db);
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                           ADC_WIDTH_BIT_12, 0, &adc_chars);

  if (shtc3.begin()) { shtc3_ok = true; }
  if (tsl.begin())   { tsl_ok = true; tsl.enableAutoRange(true);
                       tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS); }
}

/********************  LOOP  ***********************/
void loop()
{
  static uint32_t lastReport = 0;
  uint32_t nowMs = millis();

  /* -------- Battery monitor (every loop) -------- */
  float vBatt = readBattery();
  if (vBatt < V_SWITCH) {                 // immediate swap
    batt1Active = !batt1Active;
    applyRelays();
    Serial.printf("[%s] *** Battery %.2f V < %.1f V ⇒ switched. "
                  "Batt-%d → LOAD, Batt-%d → CHARGE ***\n",
                  nowStr().c_str(), vBatt, V_SWITCH,
                  batt1Active ? 1 : 2, batt1Active ? 2 : 1);
    delay(200);                           // allow contacts to settle
    vBatt = readBattery();                // read new battery
  }

  /* -------- Periodic report (print + Firebase) -- */
  if (nowMs - lastReport >= REPORT_MS) {
    lastReport = nowMs;

    /* ─ Sensors ─ */
    float temp = NAN, hum = NAN;
    uint32_t lux = 0;

    if (shtc3_ok) {
      sensors_event_t hEvt, tEvt;
      if (shtc3.getEvent(&hEvt, &tEvt)) {
        temp = tEvt.temperature;
        hum  = hEvt.relative_humidity;
      }
    }
    if (tsl_ok) {
      sensors_event_t lEvt;
      if (tsl.getEvent(&lEvt) && lEvt.light > 0)
        lux = (uint32_t)lEvt.light;
    }

    int rainRaw = analogRead(RAIN_PIN);
    int soilRaw = analogRead(SOIL_PIN);
    float rainPct = 100.0 - rainRaw * 100.0 / 4096.0;
    float soilPct = 100.0 - soilRaw * 100.0 / 4096.0;

    /* ─ Print everything ─ */
    Serial.printf("[%s] Batt-%d %.2f V | Temp %.2f C | Hum %.1f %% | "
                  "Lux %lu | Rain %.1f %% | Soil %.1f %%\n",
                  nowStr().c_str(),
                  batt1Active ? 1 : 2, vBatt,
                  temp, hum, lux, rainPct, soilPct);

    /* ─ Push to Firebase ─ */
    pushToFirebase(nowStr(), temp, hum, lux,
                   rainPct, soilPct, vBatt,
                   batt1Active ? 1 : 2);
  }

  delay(1000);      // check battery every 1 s
}
