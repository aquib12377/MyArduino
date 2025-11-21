/*
  ESP32 Weather Monitoring (Live only, no SD / no History page)
  - WebServer + WebSocket (live JSON to page)
  - DHT11 + BME280 (register-level reads as in your code)

  Required Libraries:
  - ArduinoJson
  - Adafruit Unified Sensor
  - DHT sensor library
  - Adafruit BME280 Library
  - WebSockets by Markus Sattler
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_BME280.h>
#include <Wire.h>

// --- HTML Content (Live only) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Weather Monitoring Dashboard</title>
  <script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="bg-gray-100">
  <div class="flex flex-col md:flex-row min-h-screen">
    <div class="w-full md:w-64 bg-gray-800 text-white p-4 shrink-0">
      <div class="flex justify-between items-center md:flex-col md:items-stretch">
        <h1 class="text-xl md:text-2xl font-bold md:mb-10">Weather Station</h1>
        <nav class="flex space-x-2 md:flex-col md:space-y-2 md:space-x-0">
          <a href="/" class="px-3 py-2 rounded transition duration-200 bg-gray-700">Live Data</a>
        </nav>
      </div>
    </div>

    <div class="flex-1 p-4 md:p-10">
      <h2 class="text-2xl md:text-3xl font-bold mb-5">Live Weather Data</h2>
      <div class="grid grid-cols-1 md:grid-cols-3 gap-6">
        <div class="bg-white p-6 rounded-lg shadow-md">
          <h3 class="text-lg font-semibold text-gray-700">Temperature</h3>
          <p class="text-4xl font-bold text-gray-900 mt-2"><span id="temp">--</span> &deg;C</p>
        </div>
        <div class="bg-white p-6 rounded-lg shadow-md">
          <h3 class="text-lg font-semibold text-gray-700">Humidity</h3>
          <p class="text-4xl font-bold text-gray-900 mt-2"><span id="hum">--</span> %</p>
        </div>
        <div class="bg-white p-6 rounded-lg shadow-md">
          <h3 class="text-lg font-semibold text-gray-700">Pressure</h3>
          <p class="text-4xl font-bold text-gray-900 mt-2"><span id="pres">--</span> hPa</p>
        </div>
      </div>
    </div>
  </div>

  <script>
    const tempEl = document.getElementById('temp');
    const humEl  = document.getElementById('hum');
    const presEl = document.getElementById('pres');

    const gateway = `ws://${window.location.hostname}:81/`;
    let websocket;

    const initWebSocket = () => {
      websocket = new WebSocket(gateway);
      websocket.onopen    = () => console.log('WS open');
      websocket.onclose   = () => { console.log('WS closed'); setTimeout(initWebSocket, 2000); };
      websocket.onmessage = (ev) => {
        try {
          const data = JSON.parse(ev.data);
          tempEl.textContent = data.temperature.toFixed(2);
          humEl.textContent  = data.humidity.toFixed(2);
          presEl.textContent = data.pressure.toFixed(2);
        } catch(e) {}
      };
    };
    window.addEventListener('load', initWebSocket);
  </script>
</body>
</html>
)rawliteral";

// --------- BME280 (register addresses) ----------
#define BME280_ADDR   0x76
#define REG_ID        0xD0
#define REG_RESET     0xE0
#define REG_CTRL_HUM  0xF2
#define REG_STATUS    0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_PRESS_MSB 0xF7
#define REG_TEMP_MSB  0xFA
#define REG_HUM_MSB   0xFD

// Calibration storage
uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3; int16_t dig_P4; int16_t dig_P5; int16_t dig_P6; int16_t dig_P7; int16_t dig_P8; int16_t dig_P9;
uint8_t dig_H1;  int16_t dig_H2; uint8_t dig_H3; int16_t dig_H4; int16_t dig_H5; int8_t dig_H6;

// WiFi
const char *ssid     = "MyProject";
const char *password = "12345678";

// DHT
#define DHTPIN  4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// (Not used directly, but library kept per your setup)
Adafruit_BME280 bme;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long lastUpdateTime = 0;
const long updateInterval = 2000;  // 2 seconds

// t_fine used in compensation
int32_t t_fine;

// ---------- I2C helpers ----------
void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
uint8_t read8(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}
uint16_t read16(uint8_t reg) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, (uint8_t)2);
  uint16_t msb = Wire.read();
  uint16_t lsb = Wire.read();
  return (msb << 8) | lsb;
}
uint16_t read16_LE(uint8_t reg) {
  uint16_t v = read16(reg);
  return (v >> 8) | (v << 8);
}
int16_t readS16_LE(uint8_t reg) {
  return (int16_t)read16_LE(reg);
}
void readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, len);
  uint8_t i = 0;
  while (Wire.available() && i < len) buf[i++] = Wire.read();
}

// ---------- Calibration / raw reads ----------
bool readCalibration() {
  uint8_t id = read8(REG_ID);
  if (id != 0x60) {
    Serial.print("BME280 ID: 0x"); Serial.println(id, HEX);
  }
  dig_T1 = read16_LE(0x88);
  dig_T2 = readS16_LE(0x8A);
  dig_T3 = readS16_LE(0x8C);

  dig_P1 = read16_LE(0x8E);
  dig_P2 = readS16_LE(0x90);
  dig_P3 = readS16_LE(0x92);
  dig_P4 = readS16_LE(0x94);
  dig_P5 = readS16_LE(0x96);
  dig_P6 = readS16_LE(0x98);
  dig_P7 = readS16_LE(0x9A);
  dig_P8 = readS16_LE(0x9C);
  dig_P9 = readS16_LE(0x9E);

  dig_H1 = read8(0xA1);
  dig_H2 = (int16_t)read16_LE(0xE1);
  dig_H3 = read8(0xE3);
  int8_t e4 = (int8_t)read8(0xE4);
  int8_t e5 = (int8_t)read8(0xE5);
  int8_t e6 = (int8_t)read8(0xE6);
  dig_H4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
  dig_H5 = (int16_t)((e6 << 4) | ((e5 & 0xF0) >> 4));
  dig_H6 = (int8_t)read8(0xE7);

  return true;
}

bool readRaw(int32_t &adc_P, int32_t &adc_T, int32_t &adc_H) {
  uint8_t buf[8];
  readBytes(REG_PRESS_MSB, buf, 8);
  adc_P = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4);
  adc_T = ((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4);
  adc_H = ((uint32_t)buf[6] << 8) | ((uint32_t)buf[7]);
  return true;
}

// ---------- Compensation (Bosch) ----------
float compensateTemperature(int32_t adc_T) {
  int32_t var1, var2;
  var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  int32_t T = (t_fine * 5 + 128) >> 8;
  return T / 100.0f;
}
float compensatePressure(int32_t adc_P) {
  int64_t var1, var2, p;
  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + ((int64_t)dig_P4 << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1) * ((int64_t)dig_P1)) >> 33;
  if (var1 == 0) return 0;
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);
  float pressure_pa = (float)p / 256.0f;
  return pressure_pa / 100.0f;  // hPa
}
float compensateHumidity(int32_t adc_H) {
  int32_t v_x1_u32r = t_fine - 76800;
  int32_t part1 = (adc_H << 14) - ((int32_t)dig_H4 << 20) - ((int32_t)dig_H5 * v_x1_u32r) + 16384;
  part1 >>= 15;
  int32_t tmp = (v_x1_u32r * (int32_t)dig_H6) >> 10;
  int32_t tmp2 = (v_x1_u32r * (int32_t)dig_H3) >> 11;
  tmp2 += 32768;
  tmp = (tmp * tmp2) >> 10;
  tmp += 2097152;
  tmp = (tmp * (int32_t)dig_H2 + 8192) >> 14;
  v_x1_u32r = (part1 * tmp) >> 10;
  v_x1_u32r -= (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * (int32_t)dig_H1) >> 4);
  if (v_x1_u32r < 0) v_x1_u32r = 0;
  if (v_x1_u32r > 419430400) v_x1_u32r = 419430400;
  float h = (v_x1_u32r >> 12);
  return (h / 1024.0f);
}

// ---------- WebSocket & HTTP ----------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: Serial.printf("[%u] Disconnected\n", num); break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected %d.%d.%d.%d\n", num, ip[0],ip[1],ip[2],ip[3]);
    } break;
    default: break;
  }
}

void broadcastSensorData() {
  int32_t adc_P, adc_T, adc_H;
  if (!readRaw(adc_P, adc_T, adc_H)) {
    Serial.println("Raw read failed"); return;
  }
  // If you want BME temp/hum instead of DHT, you can use compensateTemperature/compensateHumidity here.
  StaticJsonDocument<200> doc;
  doc["temperature"] = dht.readTemperature();
  doc["humidity"]    = dht.readHumidity();
  doc["pressure"]    = compensatePressure(adc_P);

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}
void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin();

  // BME280 basic init via registers
  Serial.print("BME280 at 0x"); Serial.println(BME280_ADDR, HEX);
  writeRegister(REG_RESET, 0xB6); delay(300);
  if (!readCalibration()) {
    Serial.println("Calibration read failed");
    // continue anyway
  }
  writeRegister(REG_CTRL_HUM,  0x01); // osrs_h x1
  writeRegister(REG_CTRL_MEAS, 0x27); // osrs_t x1, osrs_p x1, normal mode
  writeRegister(REG_CONFIG,    0xA0); // t_sb 1000ms, filter off

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(800);
    Serial.println("Connecting to WiFi…");
  }
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  webSocket.loop();
  server.handleClient();

  if (millis() - lastUpdateTime > updateInterval) {
    lastUpdateTime = millis();
    broadcastSensorData();
  }
}
