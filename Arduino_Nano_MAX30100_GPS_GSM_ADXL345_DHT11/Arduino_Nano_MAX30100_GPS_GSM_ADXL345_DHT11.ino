/********************************************************************
   MULTI‑SENSOR TELEMETRY + SMS   |   Arduino Nano (ATmega328P)
   – ADXL345   – DHT11   – NEO‑6M GPS   – MAX30105 Heart‑Rate
   – SIM800L SMS every 5 min (SoftwareSerial on D2/D3)
********************************************************************/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>          // SIM800L + GPS

/* ---------- MAX30105 heart‑rate includes (your original code) ---------- */
#include "MAX30105.h"
#include "heartRate.h"
MAX30105 particleSensor;

const byte  RATE_SIZE  = 4;          // moving average window
byte        rates[RATE_SIZE];
byte        rateSpot   = 0;
long        lastBeat   = 0;
float       beatsPerMinute = 0;
int         beatAvg        = 0;

/* ---------- USER SETTINGS ---------- */
#define DHT_PIN        A3
#define DHT_TYPE       DHT11
#define SMS_INTERVAL   30000UL       // 5 min
#define PHONE_NUMBER   "+917977845638" // <-- change me

/* ---------- GLOBAL OBJECTS ---------- */
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
DHT                        dht(DHT_PIN, DHT_TYPE);

/* --- serial busses --- */
SoftwareSerial gpsSerial(11, 12);     // GPS  RX=D11, TX=D12  (9600)
TinyGPSPlus    gps;

SoftwareSerial sim800(2, 3);          // GSM  RX=D2,  TX=D3   (9600)

/* ---------- RUNTIME DATA ---------- */
uint32_t lastSMS   = 0;
uint32_t lastPrint = 0;

/* ---------- SIM800L helper from your original sketch ---------- */
void updateSerial()
{
  // forward Serial <‑‑> SIM800L for debugging if needed
  while (Serial.available())
    sim800.write(Serial.read());

  while (sim800.available())
    Serial.write(sim800.read());
}

void test_sim800_module()
{
  sim800.println("AT");
  updateSerial();
}

void send_SMS(const String &body)
{
  sim800.println("AT+CMGF=1");        // text mode
  updateSerial();
  delay(1000);
  sim800.print("AT+CMGS=\"");
  sim800.print(PHONE_NUMBER);
  sim800.println("\"");
  updateSerial();

  sim800.print(body);
  updateSerial();

  sim800.write(26);                   // Ctrl‑Z
  Serial.println(F("Message Sent"));
  delay(5000);
}

/* ---------- INITIALISATION ---------- */
void setup()
{
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("\n=== Nano Telemetry – MAX30105 + SIM800L ==="));

  /* I²C devices ---------------------------------------------------- */
  Wire.begin();
  Wire.setClock(400000);

  if (!accel.begin()) {
    Serial.println(F("ADXL345 NOT found")); while (true);
  }
  accel.setRange(ADXL345_RANGE_16_G);

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30105 NOT found")); while (true);
  }
  particleSensor.setup();                    // default config
  particleSensor.setPulseAmplitudeRed(0x0A); // dim red LED
  particleSensor.setPulseAmplitudeGreen(0);  // green off

  dht.begin();

  /* GPS ------------------------------------------------------------ */
  gpsSerial.begin(9600);

  /* SIM800L -------------------------------------------------------- */
  sim800.begin(9600);
  delay(1000);
  test_sim800_module();                      // quick AT test

  Serial.println(F("Setup complete. Place finger on MAX30105 sensor."));
}

/* ---------- SENSOR PACKAGE ---------- */
String getSensorData()
{
  /* 1. Accelerometer */
  sensors_event_t event;
  accel.getEvent(&event);

  /* 2. DHT11 */
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  /* 3. GPS */
  String gpsStr = F("NoFix");
  if (gps.location.isValid()) {
    gpsStr = String(gps.location.lat(), 6) + "," +
             String(gps.location.lng(), 6);
  }

  /* 4. Heart‑rate (values already updated in loop) */
  /* Assemble CSV */
  String data = String(event.acceleration.x, 1) + "," +
                String(event.acceleration.y, 1) + "," +
                String(event.acceleration.z, 1) + "," +
                String(t, 1) + "," +
                String(h, 0) + "," +
                gpsStr + "," +
                String(beatAvg);
  return data;
}

/* ------------- pretty console logger ---------------- */
void printSensorData()
{
  /* 1. Accelerometer (g) */
  sensors_event_t event;
  accel.getEvent(&event);

  /* 2. DHT11 */
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  /* 3. GPS */
  bool  gpsFix = gps.location.isValid();
  float lat    = gps.location.lat();
  float lon    = gps.location.lng();

  /* 4. Heart‑rate */
  int bpmAvg = beatsPerMinute;          // already updated in loop()

  /* ---------- formatted print ---------- */
  Serial.println(F("────────────────────────────────────────"));
  Serial.print (F("Accel  :  X=")); Serial.print(event.acceleration.x, 1);
  Serial.print (F(" g  Y="));        Serial.print(event.acceleration.y, 1);
  Serial.print (F(" g  Z="));        Serial.print(event.acceleration.z, 1);
  Serial.println(F(" g"));

  Serial.print (F("Temp   :  "));   Serial.print(t, 1);  Serial.println(F(" °C"));
  Serial.print (F("Humidity:  "));  Serial.print(h, 0);  Serial.println(F(" %RH"));

  Serial.print (F("GPS    :  "));
  if (gpsFix) {
    Serial.print(lat, 6); Serial.print(F(", "));
    Serial.println(lon, 6);
  } else {
    Serial.println(F("No fix"));
  }

  Serial.print (F("Heart  :  "));   Serial.print(bpmAvg); Serial.println(F(" BPM"));
  Serial.println();                  // blank line between samples
}

/* ---------- MAIN LOOP ---------- */
void loop()
{
  /* -------- Heart‑rate algorithm (your original flow) ------------ */
  long irValue = particleSensor.getIR();
  if (checkForBeat(irValue))
  {
    long delta = millis() - lastBeat;
    lastBeat   = millis();
    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20)
    {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      beatAvg = 0;
      for (byte i = 0; i < RATE_SIZE; i++) beatAvg += rates[i];
      beatAvg /= RATE_SIZE;
    }
  }

  /* -------- GPS feed -------------------------------------------- */
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  /* -------- Console print (1 Hz) -------------------------------- */
  if (millis() - lastPrint > 1000) {

    printSensorData();
    lastPrint = millis();
  }

  /* -------- Periodic SMS (5 min) -------------------------------- */
  if (millis() - lastSMS > SMS_INTERVAL) {
    String sms = F("Nano Data:\nX,Y,Z,T,H,Lat,Lon,BPMavg\n");
    sms += getSensorData();
    send_SMS(sms);
    lastSMS = millis();
  }

  updateSerial(); // keep Serial<‑‑>SIM800L bridge alive (optional)
}
