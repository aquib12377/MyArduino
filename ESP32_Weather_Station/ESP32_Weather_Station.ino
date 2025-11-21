/*
  ESP32 Weather Monitoring with Historical Data (using WebServer and Embedded HTML)
  For use with the Arduino IDE.

  Required Libraries:
  - ArduinoJson by Benoît Blanchon
  - Adafruit Unified Sensor by Adafruit
  - DHT sensor library by Adafruit
  - Adafruit BME280 Library by Adafruit
  - WebSockets by Markus Sattler
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <SD.h>
#include "FS.h"

// --- HTML Content ---
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
                    <a href="/history" class="px-3 py-2 rounded transition duration-200 hover:bg-gray-700">Historical Data</a>
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
        const humEl = document.getElementById('hum');
        const presEl = document.getElementById('pres');

        const gateway = `ws://${window.location.hostname}:81/`;
        let websocket;

        const initWebSocket = function() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen    = onOpen;
            websocket.onclose   = onClose;
            websocket.onmessage = onMessage;
        };

        const onOpen = function(event) {
            console.log('Connection opened');
        };

        const onClose = function(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        };

        const onMessage = function(event) {
            const data = JSON.parse(event.data);
            tempEl.textContent = data.temperature.toFixed(2);
            humEl.textContent = data.humidity.toFixed(2);
            presEl.textContent = data.pressure.toFixed(2);
        };

        window.addEventListener('load', initWebSocket);
    </script>
</body>
</html>
)rawliteral";

const char history_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Historical Weather Data</title>
    <script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="bg-gray-100">
    <div class="flex flex-col md:flex-row min-h-screen">
        <div class="w-full md:w-64 bg-gray-800 text-white p-4 shrink-0">
             <div class="flex justify-between items-center md:flex-col md:items-stretch">
                <h1 class="text-xl md:text-2xl font-bold md:mb-10">Weather Station</h1>
                <nav class="flex space-x-2 md:flex-col md:space-y-2 md:space-x-0">
                    <a href="/" class="px-3 py-2 rounded transition duration-200 hover:bg-gray-700">Live Data</a>
                    <a href="/history" class="px-3 py-2 rounded transition duration-200 bg-gray-700">Historical Data</a>
                </nav>
            </div>
        </div>

        <div class="flex-1 p-4 md:p-10">
            <h2 class="text-2xl md:text-3xl font-bold mb-5">Historical Data</h2>
            <div class="bg-white p-6 rounded-lg shadow-md">
                <div class="overflow-x-auto">
                    <table class="w-full text-left whitespace-nowrap">
                        <thead>
                            <tr class="border-b">
                                <th class="py-2 px-2">Timestamp</th>
                                <th class="py-2 px-2">Temperature (&deg;C)</th>
                                <th class="py-2 px-2">Humidity (%)</th>
                                <th class="py-2 px-2">Pressure (hPa)</th>
                            </tr>
                        </thead>
                        <tbody id="history-data">
                            </tbody>
                    </table>
                </div>
                <div class="mt-4 flex justify-between items-center">
                    <button id="prev-btn" class="bg-gray-300 hover:bg-gray-400 text-gray-800 font-bold py-2 px-4 rounded disabled:opacity-50 disabled:cursor-not-allowed">Previous</button>
                    <span id="page-num">Page 1</span>
                    <button id="next-btn" class="bg-gray-300 hover:bg-gray-400 text-gray-800 font-bold py-2 px-4 rounded disabled:opacity-50 disabled:cursor-not-allowed">Next</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        const historyDataEl = document.getElementById('history-data');
        const prevBtn = document.getElementById('prev-btn');
        const nextBtn = document.getElementById('next-btn');
        const pageNumEl = document.getElementById('page-num');

        let currentPage = 1;

        const loadHistory = async function(page) {
            try {
                const response = await fetch(`/data?page=${page}`);
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const data = await response.json();
                
                historyDataEl.innerHTML = '';
                
                data.forEach(row => {
                    const tr = document.createElement('tr');
                    tr.className = 'border-b last:border-b-0';
                    const timestamp = new Date(parseInt(row[0]) * 1000).toLocaleString(); // Assuming timestamp is in seconds
                    tr.innerHTML = `
                        <td class="py-2 px-2">${timestamp}</td>
                        <td class="py-2 px-2">${row[1]}</td>
                        <td class="py-2 px-2">${row[2]}</td>
                        <td class="py-2 px-2">${row[3]}</td>
                    `;
                    historyDataEl.appendChild(tr);
                });
                pageNumEl.textContent = `Page ${page}`;

                prevBtn.disabled = page === 1;
                // Assuming 20 is the page size
                nextBtn.disabled = data.length < 20;
            } catch (error) {
                console.error("Failed to load history:", error);
                historyDataEl.innerHTML = '<tr><td colspan="4" class="text-center py-4">Failed to load data.</td></tr>';
            }
        };

        prevBtn.addEventListener('click', () => {
            if (currentPage > 1) {
                currentPage--;
                loadHistory(currentPage);
            }
        });

        nextBtn.addEventListener('click', () => {
            currentPage++;
            loadHistory(currentPage);
        });
        
        window.addEventListener('load', () => loadHistory(currentPage));
    </script>
</body>
</html>
)rawliteral";


#define BME280_ADDR 0x76

// Registers
#define REG_ID 0xD0
#define REG_RESET 0xE0
#define REG_CTRL_HUM 0xF2
#define REG_STATUS 0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG 0xF5
#define REG_PRESS_MSB 0xF7  // 0xF7..0xF9 pressure
#define REG_TEMP_MSB 0xFA   // 0xFA..0xFC temperature
#define REG_HUM_MSB 0xFD    // 0xFD..0xFE humidity

// Calibration storage
uint16_t dig_T1;
int16_t dig_T2;
int16_t dig_T3;
uint16_t dig_P1;
int16_t dig_P2;
int16_t dig_P3;
int16_t dig_P4;
int16_t dig_P5;
int16_t dig_P6;
int16_t dig_P7;
int16_t dig_P8;
int16_t dig_P9;
uint8_t dig_H1;
int16_t dig_H2;
uint8_t dig_H3;
int16_t dig_H4;
int16_t dig_H5;
int8_t dig_H6;
// Replace with your network credentials
const char *ssid = "MyProject";
const char *password = "12345678";

// Define DHT Sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Define BME280 Sensor
Adafruit_BME280 bme;

// SD Card
File dataFile;

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long lastSaveTime = 0;
const long saveInterval = 15000;  // 15 seconds

unsigned long lastUpdateTime = 0;
const long updateInterval = 2000;  // 2 seconds
// t_fine used in compensation
int32_t t_fine;

// I2C helpers
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
  if (Wire.available()) return Wire.read();
  return 0;
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

// Some calibration values are little-endian; helper:
uint16_t read16_LE(uint8_t reg) {
  uint16_t val = read16(reg);
  return (val >> 8) | (val << 8);
}

int16_t readS16_LE(uint8_t reg) {
  return (int16_t)read16_LE(reg);
}

// Read block of bytes
void readBytes(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(BME280_ADDR, len);
  uint8_t i = 0;
  while (Wire.available() && i < len) {
    buf[i++] = Wire.read();
  }
}

// Read calibration data from sensor
bool readCalibration() {
  // Check ID
  uint8_t id = read8(REG_ID);
  if (id != 0x60) {
    Serial.print("Unexpected chip ID: 0x");
    Serial.println(id, HEX);
    // It might still work with some clones, but warn the user
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
  // H4 and H5 are a bit packed across E4..E7
  int8_t e4 = (int8_t)read8(0xE4);
  int8_t e5 = (int8_t)read8(0xE5);
  int8_t e6 = (int8_t)read8(0xE6);

  // From datasheet:
  // dig_H4 = (E4 << 4) | (E5 & 0x0F)
  // dig_H5 = (E6 << 4) | (E5 >> 4)
  dig_H4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
  dig_H5 = (int16_t)((e6 << 4) | ((e5 & 0xF0) >> 4));
  dig_H6 = (int8_t)read8(0xE7);

  return true;
}

// Read raw ADC samples (20-bit pressure/temp, 16-bit humidity)
bool readRaw(int32_t &adc_P, int32_t &adc_T, int32_t &adc_H) {
  // Burst read from REG_PRESS_MSB (0xF7) for 8 bytes: P[3], T[3], H[2]
  uint8_t buf[8];
  readBytes(REG_PRESS_MSB, buf, 8);
  // buf: 0..2 pressure MSB..LSB, 3..5 temp MSB..LSB, 6..7 hum MSB..LSB
  adc_P = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4);
  adc_T = ((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4);
  adc_H = ((uint32_t)buf[6] << 8) | ((uint32_t)buf[7]);
  return true;
}

// Compensation functions from Bosch datasheet (integer arithmetic)
float compensateTemperature(int32_t adc_T) {
  int32_t var1, var2;
  var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  int32_t T = (t_fine * 5 + 128) >> 8;  // temperature in 0.01 degC
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

  if (var1 == 0) {
    return 0;  // avoid division by zero
  }
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);

  // p is in Q24.8 (i.e., p/256 = Pa). To get Pa:
  float pressure_pa = (float)p / 256.0f;
  return pressure_pa / 100.0f;  // return hPa
}

float compensateHumidity(int32_t adc_H) {
  int32_t v_x1_u32r = t_fine - 76800;

  // Rewritten in smaller steps to avoid bracket errors and make it readable
  int32_t part1 = (adc_H << 14) - ((int32_t)dig_H4 << 20) - ((int32_t)dig_H5 * v_x1_u32r) + 16384;
  part1 = part1 >> 15;

  int32_t tmp = (v_x1_u32r * (int32_t)dig_H6) >> 10;
  int32_t tmp2 = (v_x1_u32r * (int32_t)dig_H3) >> 11;
  tmp2 = tmp2 + 32768;
  tmp = (tmp * tmp2) >> 10;

  tmp = tmp + 2097152;  // add constant
  tmp = (tmp * (int32_t)dig_H2 + 8192) >> 14;

  v_x1_u32r = (part1 * tmp) >> 10;

  v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * (int32_t)dig_H1) >> 4);
  if (v_x1_u32r < 0) v_x1_u32r = 0;
  if (v_x1_u32r > 419430400) v_x1_u32r = 419430400;

  float h = (v_x1_u32r >> 12);
  return (h / 1024.0f);  // percent RH [0..100]
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:
      // Not expecting messages from client
      break;
  }
}

void broadcastSensorData() {
  int32_t adc_P, adc_T, adc_H;
  if (!readRaw(adc_P, adc_T, adc_H)) {
    Serial.println("Failed to read raw data");
    // delay(1000);
  }
  StaticJsonDocument<200> doc;
  doc["temperature"] = dht.readTemperature();
  doc["humidity"] = dht.readHumidity();
  doc["pressure"] = compensatePressure(adc_P);

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
}

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleHistory() {
  server.send_P(200, "text/html", history_html);
}

void handleData() {
  int page = 1;
  if (server.hasArg("page")) {
    page = server.arg("page").toInt();
  }
  int pageSize = 20;

  File file = SD.open("/data.csv");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  StaticJsonDocument<2048> doc;
  JsonArray data = doc.to<JsonArray>();

  for (int i = 0; i < (page - 1) * pageSize; ++i) {
    if (file.available()) {
      file.readStringUntil('\n');
    } else {
      break;
    }
  }

  int count = 0;
  while (file.available() && count < pageSize) {
    String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      JsonArray row = data.createNestedArray();
      int pos = 0;
      int nextPos = 0;
      while ((nextPos = line.indexOf(',', pos)) != -1) {
        row.add(line.substring(pos, nextPos));
        pos = nextPos + 1;
      }
      row.add(line.substring(pos));
      count++;
    }
  }
  file.close();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);

  dht.begin();
  Wire.begin();  // Uses default SDA/SCL pins for ESP32; optionally Wire.begin(SDA, SCL);

  // Optionally try both addresses if sensor not found:
  Serial.print("Attempting device at 0x");
  Serial.println(BME280_ADDR, HEX);

  // Soft reset
  writeRegister(REG_RESET, 0xB6);
  delay(300);

  // Read calibration data
  if (!readCalibration()) {
    Serial.println("Failed reading calibration.");
    while (1) delay(1000);
  }

  // Configure sensor:
  // ctrl_hum (0xF2): humidity oversampling x1 (0x01)
  writeRegister(REG_CTRL_HUM, 0x01);
  // ctrl_meas (0xF4): temp oversampling x1, press oversampling x1, mode normal (0x27 = 0010 0111)
  // For example: osrs_t = 1, osrs_p = 1, mode = normal => 0b00100111 = 0x27
  writeRegister(REG_CTRL_MEAS, 0x27);
  // config (0xF5): t_sb = 1000 ms (0b101), filter off (0), spi3w_en = 0 => 0b10100000 = 0xA0 (but many choose 0xA0 or 0x00)
  writeRegister(REG_CONFIG, 0xA0);

  Serial.println("Sensor configured.");

  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/history", HTTP_GET, handleHistory);
  server.on("/data", HTTP_GET, handleData);
  server.onNotFound(handleNotFound);

  server.begin();
}

void loop() {
  webSocket.loop();
  server.handleClient();
  int32_t adc_P, adc_T, adc_H;
  if (!readRaw(adc_P, adc_T, adc_H)) {
    Serial.println("Failed to read raw data");
    // delay(1000);
  }
  if (millis() - lastUpdateTime > updateInterval) {
    lastUpdateTime = millis();
    broadcastSensorData();
  }

  if (millis() - lastSaveTime > saveInterval) {
    lastSaveTime = millis();
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    float pressure = compensatePressure(adc_P);

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    dataFile = SD.open("/data.csv", FILE_APPEND);
    if (dataFile) {
      dataFile.print(String(millis()));
      dataFile.print(",");
      dataFile.print(temperature);
      dataFile.print(",");
      dataFile.print(humidity);
      dataFile.print(",");
      dataFile.println(pressure);
      dataFile.close();
      Serial.println("Data saved to SD card.");
    } else {
      Serial.println("Error opening data.csv");
    }
  }
}
