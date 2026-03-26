/*
 * ============================================================
 *   🌿 ESP8266 GREENHOUSE MONITORING SYSTEM
 *        + Local Web Server with Auto-Refresh Dashboard
 * ============================================================
 *  Sensors   : DHT11 | MQ2 | Soil Moisture | Rain Sensor
 *  Actuators : Relay (Active LOW) for Water Pump | Buzzer
 *  Display   : I2C LCD 16x2
 *  Network   : WiFi Web Server on port 80 | JSON API /api/data
 * ============================================================
 *
 *  PIN WIRING SUMMARY:
 *  -----------------------------------------------------------
 *  Component        | ESP8266 Pin | GPIO
 *  -----------------------------------------------------------
 *  I2C LCD SDA      | D2          | GPIO4
 *  I2C LCD SCL      | D1          | GPIO5
 *  DHT11            | D5          | GPIO14
 *  MQ2  (digital)   | D4          | GPIO2
 *  Soil Moisture    | D3          | GPIO0
 *  Rain Sensor      | D6          | GPIO12
 *  Relay (act. LOW) | D7          | GPIO13
 *  Buzzer           | D8          | GPIO15
 *  -----------------------------------------------------------
 *
 *  HOW TO ACCESS DASHBOARD:
 *  1. Flash & power on. Open Serial Monitor (115200 baud).
 *  2. It will print something like: "IP Address: 192.168.1.45"
 *  3. Open that IP in any browser on the same WiFi network.
 *  4. Dashboard auto-refreshes every 3 seconds via JSON API.
 *  5. Raw JSON: http://<IP>/api/data
 *
 *  LIBRARIES NEEDED:
 *    - LiquidCrystal_I2C  by Frank de Brabander
 *    - DHT sensor library  by Adafruit
 *    - Adafruit Unified Sensor (dependency)
 *    - ESP8266WiFi        (built-in with ESP8266 board package)
 *    - ESP8266WebServer   (built-in with ESP8266 board package)
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ─── WiFi CREDENTIALS ─────────────────────────────────────
#define WIFI_SSID     "AAA"       // ← change this
#define WIFI_PASSWORD "Acube@123"   // ← change this

// ─── PIN DEFINITIONS ──────────────────────────────────────
#define DHT_PIN       14   // D5
#define MQ2_PIN        2   // D4
#define SOIL_PIN       0   // D3
#define RAIN_PIN      12   // D6
#define RELAY_PIN     13   // D7 — Active LOW
#define BUZZER_PIN    15   // D8

// ─── SENSOR CONFIG ────────────────────────────────────────
#define DHT_TYPE      DHT11

// ─── ALERT THRESHOLDS ─────────────────────────────────────
#define TEMP_ALERT    35.0
#define HUM_ALERT     85.0

// ─── TIMING ───────────────────────────────────────────────
#define READ_INTERVAL    2000
#define LCD_PAGE_TIME    3000

// ─── OBJECTS ──────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);   // try 0x3F if blank
DHT dht(DHT_PIN, DHT_TYPE);
ESP8266WebServer server(80);

// ─── GLOBALS ──────────────────────────────────────────────
float temperature  = 0.0;
float humidity     = 0.0;
bool  gasDetected  = false;
bool  soilDry      = false;
bool  isRaining    = false;
bool  pumpOn       = false;
bool  buzzerOn     = false;

unsigned long lastReadTime = 0;
unsigned long lastPageTime = 0;
uint8_t       lcdPage      = 0;

// ─── CUSTOM LCD CHARS ─────────────────────────────────────
byte dropChar[8] = { 0b00100,0b00100,0b01010,0b01010,
                     0b10001,0b10001,0b10001,0b01110 };
byte leafChar[8] = { 0b00000,0b00110,0b01110,0b11110,
                     0b11100,0b01100,0b00100,0b00100 };


// ══════════════════════════════════════════════════════════
//  WEB ROUTES
// ══════════════════════════════════════════════════════════

// ── /api/data  →  JSON ────────────────────────────────────
void handleAPI() {
  String json = "{";
  json += "\"temperature\":"  + String(temperature, 1) + ",";
  json += "\"humidity\":"     + String(humidity, 1)    + ",";
  json += "\"soilDry\":"      + String(soilDry    ? "true" : "false") + ",";
  json += "\"isRaining\":"    + String(isRaining  ? "true" : "false") + ",";
  json += "\"gasDetected\":"  + String(gasDetected? "true" : "false") + ",";
  json += "\"pumpOn\":"       + String(pumpOn     ? "true" : "false") + ",";
  json += "\"buzzerOn\":"     + String(buzzerOn   ? "true" : "false") + ",";
  json += "\"tempAlert\":"    + String(TEMP_ALERT, 1) + ",";
  json += "\"humAlert\":"     + String(HUM_ALERT, 1);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ── /  →  HTML Dashboard ──────────────────────────────────
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>🌿 Greenhouse Monitor</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');

    :root {
      --bg:       #0a0f0a;
      --panel:    #111a11;
      --border:   #1e3a1e;
      --green:    #4ade80;
      --lime:     #a3e635;
      --yellow:   #fbbf24;
      --red:      #f87171;
      --blue:     #60a5fa;
      --text:     #d1fae5;
      --muted:    #6b7280;
      --mono:     'Share Tech Mono', monospace;
      --sans:     'Exo 2', sans-serif;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      background: var(--bg);
      color: var(--text);
      font-family: var(--sans);
      min-height: 100vh;
      padding: 24px 16px;
    }

    /* ── Header ── */
    header {
      text-align: center;
      margin-bottom: 28px;
    }
    header h1 {
      font-size: 2rem;
      font-weight: 800;
      letter-spacing: 2px;
      color: var(--green);
      text-shadow: 0 0 20px rgba(74,222,128,0.4);
    }
    header p {
      font-family: var(--mono);
      font-size: 0.78rem;
      color: var(--muted);
      margin-top: 6px;
    }
    #statusDot {
      display: inline-block;
      width: 8px; height: 8px;
      border-radius: 50%;
      background: var(--green);
      margin-right: 6px;
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse {
      0%,100% { opacity: 1; }
      50%      { opacity: 0.3; }
    }

    /* ── Grid ── */
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
      gap: 16px;
      max-width: 860px;
      margin: 0 auto 28px;
    }

    /* ── Card ── */
    .card {
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 20px 16px;
      position: relative;
      overflow: hidden;
      transition: border-color 0.3s, box-shadow 0.3s;
    }
    .card::before {
      content: '';
      position: absolute;
      top: 0; left: 0; right: 0;
      height: 3px;
      background: var(--accent, var(--green));
      border-radius: 14px 14px 0 0;
    }
    .card.alert {
      border-color: var(--red);
      box-shadow: 0 0 18px rgba(248,113,113,0.2);
      animation: alertPulse 1s ease-in-out infinite alternate;
    }
    @keyframes alertPulse {
      from { box-shadow: 0 0 8px rgba(248,113,113,0.15); }
      to   { box-shadow: 0 0 24px rgba(248,113,113,0.4); }
    }

    .card-icon { font-size: 1.8rem; margin-bottom: 10px; }
    .card-label {
      font-size: 0.7rem;
      letter-spacing: 2px;
      text-transform: uppercase;
      color: var(--muted);
      margin-bottom: 6px;
    }
    .card-value {
      font-family: var(--mono);
      font-size: 1.6rem;
      font-weight: 600;
      color: var(--accent, var(--green));
      line-height: 1;
    }
    .card-unit {
      font-size: 0.85rem;
      color: var(--muted);
      margin-left: 3px;
    }
    .card-status {
      font-family: var(--mono);
      font-size: 1rem;
      font-weight: 600;
    }
    .badge {
      display: inline-block;
      padding: 3px 10px;
      border-radius: 99px;
      font-size: 0.72rem;
      font-weight: 600;
      letter-spacing: 1px;
      margin-top: 8px;
      text-transform: uppercase;
    }
    .badge-ok      { background: rgba(74,222,128,0.15); color: var(--green); }
    .badge-warn    { background: rgba(251,191,36,0.15);  color: var(--yellow); }
    .badge-danger  { background: rgba(248,113,113,0.2);  color: var(--red); }
    .badge-off     { background: rgba(107,114,128,0.15); color: var(--muted); }
    .badge-on      { background: rgba(96,165,250,0.15);  color: var(--blue); }

    /* ── Alert Banner ── */
    #alertBanner {
      display: none;
      max-width: 860px;
      margin: 0 auto 20px;
      background: rgba(248,113,113,0.12);
      border: 1px solid rgba(248,113,113,0.4);
      border-radius: 10px;
      padding: 12px 18px;
      font-family: var(--mono);
      font-size: 0.85rem;
      color: var(--red);
    }

    /* ── Footer ── */
    footer {
      text-align: center;
      font-family: var(--mono);
      font-size: 0.72rem;
      color: var(--muted);
    }
    footer span { color: var(--green); }
  </style>
</head>
<body>

  <header>
    <h1>🌿 GREENHOUSE MONITOR</h1>
    <p><span id="statusDot"></span>LIVE &nbsp;·&nbsp; Auto-refresh every 3s &nbsp;·&nbsp; ESP8266</p>
  </header>

  <div id="alertBanner"></div>

  <div class="grid">

    <!-- Temperature -->
    <div class="card" id="cardTemp" style="--accent: #4ade80">
      <div class="card-icon">🌡️</div>
      <div class="card-label">Temperature</div>
      <div class="card-value" id="valTemp">--<span class="card-unit">°C</span></div>
      <div id="badgeTemp" class="badge badge-ok">Normal</div>
    </div>

    <!-- Humidity -->
    <div class="card" id="cardHum" style="--accent: #60a5fa">
      <div class="card-icon">💧</div>
      <div class="card-label">Humidity</div>
      <div class="card-value" id="valHum">--<span class="card-unit">%</span></div>
      <div id="badgeHum" class="badge badge-ok">Normal</div>
    </div>

    <!-- Soil -->
    <div class="card" id="cardSoil" style="--accent: #a3e635">
      <div class="card-icon">🌱</div>
      <div class="card-label">Soil Moisture</div>
      <div class="card-status" id="valSoil">--</div>
      <div id="badgeSoil" class="badge badge-ok">OK</div>
    </div>

    <!-- Rain -->
    <div class="card" id="cardRain" style="--accent: #60a5fa">
      <div class="card-icon">🌧️</div>
      <div class="card-label">Rain</div>
      <div class="card-status" id="valRain">--</div>
      <div id="badgeRain" class="badge badge-ok">Clear</div>
    </div>

    <!-- Gas -->
    <div class="card" id="cardGas" style="--accent: #fbbf24">
      <div class="card-icon">💨</div>
      <div class="card-label">Gas / Smoke</div>
      <div class="card-status" id="valGas">--</div>
      <div id="badgeGas" class="badge badge-ok">Safe</div>
    </div>

    <!-- Pump -->
    <div class="card" id="cardPump" style="--accent: #60a5fa">
      <div class="card-icon">⚙️</div>
      <div class="card-label">Water Pump</div>
      <div class="card-status" id="valPump">--</div>
      <div id="badgePump" class="badge badge-off">Idle</div>
    </div>

  </div>

  <footer>
    Last updated: <span id="lastTime">--</span> &nbsp;|&nbsp;
    API: <span>/api/data</span>
  </footer>

<script>
  const API = '/api/data';

  async function fetchData() {
    try {
      const res  = await fetch(API);
      const d    = await res.json();

      // ── Temperature
      document.getElementById('valTemp').innerHTML =
        d.temperature.toFixed(1) + '<span class="card-unit">°C</span>';
      const tempHigh = d.temperature > d.tempAlert;
      document.getElementById('cardTemp').classList.toggle('alert', tempHigh);
      document.getElementById('badgeTemp').className = 'badge ' + (tempHigh ? 'badge-danger' : 'badge-ok');
      document.getElementById('badgeTemp').textContent = tempHigh ? 'Too Hot!' : 'Normal';

      // ── Humidity
      document.getElementById('valHum').innerHTML =
        d.humidity.toFixed(1) + '<span class="card-unit">%</span>';
      const humHigh = d.humidity > d.humAlert;
      document.getElementById('cardHum').classList.toggle('alert', humHigh);
      document.getElementById('badgeHum').className = 'badge ' + (humHigh ? 'badge-warn' : 'badge-ok');
      document.getElementById('badgeHum').textContent = humHigh ? 'High' : 'Normal';

      // ── Soil
      document.getElementById('valSoil').textContent = d.soilDry ? 'DRY' : 'WET';
      document.getElementById('cardSoil').classList.toggle('alert', d.soilDry);
      document.getElementById('badgeSoil').className = 'badge ' + (d.soilDry ? 'badge-warn' : 'badge-ok');
      document.getElementById('badgeSoil').textContent = d.soilDry ? 'Needs Water' : 'Adequate';

      // ── Rain
      document.getElementById('valRain').textContent = d.isRaining ? 'RAINING' : 'CLEAR';
      document.getElementById('badgeRain').className = 'badge ' + (d.isRaining ? 'badge-on' : 'badge-ok');
      document.getElementById('badgeRain').textContent = d.isRaining ? 'Detected' : 'Clear';

      // ── Gas
      document.getElementById('valGas').textContent = d.gasDetected ? 'DANGER!' : 'CLEAR';
      document.getElementById('cardGas').classList.toggle('alert', d.gasDetected);
      document.getElementById('badgeGas').className = 'badge ' + (d.gasDetected ? 'badge-danger' : 'badge-ok');
      document.getElementById('badgeGas').textContent = d.gasDetected ? 'Gas Detected!' : 'Safe';

      // ── Pump
      document.getElementById('valPump').textContent = d.pumpOn ? 'ON' : 'OFF';
      document.getElementById('cardPump').style.setProperty('--accent', d.pumpOn ? '#60a5fa' : '#6b7280');
      document.getElementById('badgePump').className = 'badge ' + (d.pumpOn ? 'badge-on' : 'badge-off');
      document.getElementById('badgePump').textContent = d.pumpOn ? 'Running' : 'Idle';

      // ── Alert banner
      let alerts = [];
      if (d.gasDetected)            alerts.push('⚠ Gas/Smoke detected!');
      if (d.temperature > d.tempAlert) alerts.push('🌡 Temperature too high: ' + d.temperature.toFixed(1) + '°C');
      if (d.humidity > d.humAlert)  alerts.push('💧 Humidity too high: ' + d.humidity.toFixed(1) + '%');
      const banner = document.getElementById('alertBanner');
      if (alerts.length) {
        banner.style.display = 'block';
        banner.innerHTML = '🚨 ALERTS: &nbsp;' + alerts.join(' &nbsp;&nbsp; ');
      } else {
        banner.style.display = 'none';
      }

      // ── Timestamp
      document.getElementById('lastTime').textContent = new Date().toLocaleTimeString();

    } catch (e) {
      document.getElementById('lastTime').textContent = 'Connection lost...';
    }
  }

  fetchData();
  setInterval(fetchData, 3000);   // auto-refresh every 3 seconds
</script>
</body>
</html>
)rawhtml";

  server.send(200, "text/html", html);
}

// ── 404 ───────────────────────────────────────────────────
void handleNotFound() {
  server.send(404, "text/plain", "Not found. Try / or /api/data");
}


// ══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n🌿 Greenhouse Monitor Starting..."));

  // GPIO modes
  pinMode(SOIL_PIN,   INPUT);
  pinMode(RAIN_PIN,   INPUT);
  pinMode(MQ2_PIN,    INPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(RELAY_PIN,  HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, dropChar);
  lcd.createChar(1, leafChar);
  lcd.setCursor(0, 0);
  lcd.print("  GreenHouse  ");
  lcd.setCursor(0, 1);
  lcd.print(" Connecting... ");

  // WiFi
  Serial.printf("\nConnecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.println("✅ WiFi connected!");
    Serial.print("📡 IP Address: http://"); Serial.println(ip);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(ip);
    delay(3000);
  } else {
    Serial.println("❌ WiFi FAILED — running without network.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED!");
    lcd.setCursor(0, 1);
    lcd.print("No Web Server");
    delay(3000);
  }

  // Web server routes
  server.on("/",         handleRoot);
  server.on("/api/data", handleAPI);
  server.onNotFound(     handleNotFound);
  server.begin();
  Serial.println(F("🌐 Web server started on port 80"));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  GreenHouse  ");
  lcd.setCursor(0, 1);
  lcd.print(" Monitor v1   ");
  delay(1500);
  lcd.clear();

  dht.begin();
  Serial.println(F("✅ Setup complete. Monitoring started.\n"));
}


// ══════════════════════════════════════════════════════════
void loop() {
  server.handleClient();   // ← must be called every loop tick

  unsigned long now = millis();

  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;
    readSensors();
    controlActuators();
    printSerial();
  }

  if (now - lastPageTime >= LCD_PAGE_TIME) {
    lastPageTime = now;
    lcdPage = (lcdPage + 1) % 3;
    updateLCD();
  }
}


// ══════════════════════════════════════════════════════════
void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) { temperature = t; humidity = h; }
  else Serial.println(F("[WARN] DHT11 read failed!"));

  soilDry     = (digitalRead(SOIL_PIN) == HIGH);
  isRaining   = (digitalRead(RAIN_PIN) == LOW);
  gasDetected = (digitalRead(MQ2_PIN)  == LOW);
}


// ══════════════════════════════════════════════════════════
void controlActuators() {
  if (soilDry && !isRaining) {
    pumpOn = true;
    digitalWrite(RELAY_PIN, LOW);
  } else {
    pumpOn = false;
    digitalWrite(RELAY_PIN, HIGH);
  }

  bool alertNeeded = gasDetected
                  || (temperature > TEMP_ALERT)
                  || (humidity    > HUM_ALERT);

  if (alertNeeded) {
    buzzerOn = true;
    digitalWrite(BUZZER_PIN, HIGH); delay(200);
    digitalWrite(BUZZER_PIN, LOW);  delay(100);
  } else {
    buzzerOn = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}


// ══════════════════════════════════════════════════════════
void updateLCD() {
  lcd.clear();
  switch (lcdPage) {
    case 0:
      lcd.setCursor(0, 0); lcd.print("Temp:"); lcd.print(temperature, 1); lcd.print("\xDF""C");
      if (temperature > TEMP_ALERT) lcd.print(" !");
      lcd.setCursor(0, 1); lcd.print("Humi:"); lcd.print(humidity, 1); lcd.print("%");
      if (humidity > HUM_ALERT) lcd.print(" !");
      break;
    case 1:
      lcd.setCursor(0, 0); lcd.write(0); lcd.print(" Soil: ");
      lcd.print(soilDry ? "DRY  " : "WET  ");
      lcd.setCursor(0, 1); lcd.print("Rain: ");
      lcd.print(isRaining ? "YES   " : "NO    ");
      break;
    case 2:
      lcd.setCursor(0, 0); lcd.print("Gas: ");
      lcd.print(gasDetected ? "DANGER! " : "OK      ");
      lcd.setCursor(0, 1); lcd.write(1); lcd.print(" Pump: ");
      lcd.print(pumpOn ? "ON " : "OFF");
      break;
  }
}


// ══════════════════════════════════════════════════════════
void printSerial() {
  Serial.println(F("─────────────────────────────"));
  Serial.print(F("Temp     : ")); Serial.print(temperature); Serial.println(F(" °C"));
  Serial.print(F("Humidity : ")); Serial.print(humidity);    Serial.println(F(" %"));
  Serial.print(F("Soil     : ")); Serial.println(soilDry    ? "DRY"    : "WET");
  Serial.print(F("Rain     : ")); Serial.println(isRaining   ? "YES"    : "NO");
  Serial.print(F("Gas/Smoke: ")); Serial.println(gasDetected ? "DANGER" : "OK");
  Serial.print(F("Pump     : ")); Serial.println(pumpOn      ? "ON"     : "OFF");
  Serial.print(F("Buzzer   : ")); Serial.println(buzzerOn    ? "ALERT"  : "OFF");
  Serial.println(F("─────────────────────────────\n"));
}
