/*
 * ============================================================
 *  ESP32 BATTERY MONITOR
 *  Sensors : ACS712-5A | 25V Voltage Module | DS18B20
 *  Display : I2C LCD 20x4  +  Web Dashboard (port 80)
 *  Update  : Every 2 seconds
 * ============================================================
 *
 *  WIRING GUIDE
 *  ─────────────────────────────────────────────────────────
 *  ACS712-5A:
 *    VCC  → 5V
 *    GND  → GND
 *    OUT  → Voltage divider → GPIO 34
 *           (Use two equal 10kΩ resistors as 2:1 divider
 *            so 0–5V output becomes 0–2.5V for ESP32 safety)
 *
 *  25V Voltage Sensor Module:
 *    VCC  → Battery+ (measures up to 25V)
 *    GND  → GND
 *    S    → GPIO 35
 *           (Module output is 0–5V for 0–25V input.
 *            For 12V battery, max output ≈ 2.88V — safe.
 *            Add 2:1 divider if measuring >16V.)
 *
 *  DS18B20:
 *    VCC  → 3.3V
 *    GND  → GND
 *    DATA → GPIO 4  (with 4.7kΩ pull-up to 3.3V)
 *
 *  LCD I2C 20x4:
 *    VCC  → 5V
 *    GND  → GND
 *    SDA  → GPIO 21
 *    SCL  → GPIO 22
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============================================================
//  WiFi Credentials — CHANGE THESE
// ============================================================
const char* WIFI_SSID = "AAA";
const char* WIFI_PASS = "Acube123";

// ============================================================
//  Pin Definitions
// ============================================================
#define PIN_CURRENT   34   // ACS712 output (after 2:1 divider)
#define PIN_VOLTAGE   35   // 25V sensor output
#define PIN_DS18B20    4   // DS18B20 data

// ============================================================
//  Calibration Constants
// ============================================================

// --- ACS712-5A ---
// Sensitivity: 185 mV/A    Zero-current output: 2.5V (on 5V supply)
// With 2:1 voltage divider on OUT pin → 0.5–2.25V at ADC
// Zero-current ADC voltage = 2.5V / 2 = 1.25V
#define ACS712_ZERO_V      2.500f   // Sensor zero-current voltage (before divider)
#define ACS712_SENSITIVITY 0.185f   // V per Amp (185 mV/A for 5A module)
#define ACS712_DIVIDER     2.0f     // Voltage divider ratio on sensor output
#define CURRENT_DEADBAND   0.05f    // A — ignore noise below this threshold

// --- 25V Voltage Sensor Module ---
// Internal resistor divider ratio: 5.0  (25V in → 5V out)
#define VSENSOR_RATIO      5.0f

// --- ADC ---
#define ADC_RESOLUTION     4095.0f
#define ADC_VREF           3.3f
#define ADC_SAMPLES        32       // Oversampling count for noise reduction

// --- Battery Profile (12V Lead-Acid) ---
// Adjust BATT_FULL_V / BATT_EMPTY_V to match your battery chemistry:
//   12V Lead-Acid  : full=12.7V (open circuit) / 14.4V (charged), empty=10.5V
//   3S LiPo        : full=12.6V, empty=9.0V
//   LiFePO4 4S     : full=14.6V, empty=12.0V
#define BATT_FULL_VOLTS    14.4f
#define BATT_EMPTY_VOLTS   10.5f
#define BATT_NOMINAL_AH    7.0f     // Ah — for power display reference

// --- Alerts ---
#define TEMP_WARN_C        45.0f    // °C overheat threshold
#define TEMP_CRIT_C        60.0f
#define LOW_SOC_WARN       20.0f    // % low battery threshold
#define OVERVOLT_V         14.6f    // Overvoltage alert

// ============================================================
//  Object Instances
// ============================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);   // Try 0x3F if 0x27 doesn't work
OneWire            oneWire(PIN_DS18B20);
DallasTemperature  ds18b20(&oneWire);
WebServer          server(80);

// ============================================================
//  Data Structure
// ============================================================
struct BatteryData {
    float voltage     = 0.0f;
    float current     = 0.0f;    // + = charging, - = discharging
    float power       = 0.0f;    // W
    float temperature = 0.0f;    // °C
    float soc         = 0.0f;    // %
    bool  charging    = false;
    bool  tempWarn    = false;
    bool  tempCrit    = false;
    bool  lowBatt     = false;
    bool  overVolt    = false;
    bool  sensorOK    = true;
    unsigned long timestamp = 0; // ms since boot
} batt;

unsigned long lastUpdateMs = 0;
const unsigned long UPDATE_MS = 2000;

// ============================================================
//  SENSOR FUNCTIONS
// ============================================================

float adcToVoltage(int rawAvg) {
    return (rawAvg / ADC_RESOLUTION) * ADC_VREF;
}

int oversampledADC(int pin) {
    long sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(pin);
        delayMicroseconds(100);
    }
    return (int)(sum / ADC_SAMPLES);
}

float readBatteryVoltage() {
    int raw     = oversampledADC(PIN_VOLTAGE);
    float v_adc = adcToVoltage(raw);
    // Sensor inverts the ratio: actual battery V = ADC_V × ratio
    return v_adc * VSENSOR_RATIO;
}

float readCurrent() {
    int raw       = oversampledADC(PIN_CURRENT);
    float v_adc   = adcToVoltage(raw);
    float v_sensor = v_adc * ACS712_DIVIDER;          // Undo voltage divider
    float amps    = (v_sensor - ACS712_ZERO_V) / ACS712_SENSITIVITY;
    if (fabsf(amps) < CURRENT_DEADBAND) amps = 0.0f;  // Kill noise near zero
    return amps;
}

float readTemperature() {
    ds18b20.requestTemperatures();
    float t = ds18b20.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C || t < -50.0f || t > 125.0f) return -99.0f;
    return t;
}

float calcSOC(float v) {
    float s = (v - BATT_EMPTY_VOLTS) / (BATT_FULL_VOLTS - BATT_EMPTY_VOLTS) * 100.0f;
    return constrain(s, 0.0f, 100.0f);
}

void updateSensors() {
    batt.voltage     = readBatteryVoltage();
    batt.current     = readCurrent();
    batt.power       = batt.voltage * fabsf(batt.current);
    batt.temperature = readTemperature();
    batt.soc         = calcSOC(batt.voltage);
    batt.charging    = batt.current > CURRENT_DEADBAND;
    batt.tempWarn    = (batt.temperature >= TEMP_WARN_C);
    batt.tempCrit    = (batt.temperature >= TEMP_CRIT_C);
    batt.lowBatt     = (batt.soc < LOW_SOC_WARN);
    batt.overVolt    = (batt.voltage > OVERVOLT_V);
    batt.sensorOK    = (batt.temperature > -50.0f);
    batt.timestamp   = millis();
}

// ============================================================
//  LCD DISPLAY  (20×4)
// ============================================================

// Custom character bitmaps
byte ICON_THERM[8] = { 0x04, 0x0A, 0x0A, 0x0E, 0x0E, 0x1F, 0x1F, 0x0E };
byte ICON_BOLT[8]  = { 0x02, 0x06, 0x0E, 0x1F, 0x1F, 0x0E, 0x04, 0x04 };
byte ICON_BFULL[8] = { 0x0E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F };
byte ICON_BHALF[8] = { 0x0E, 0x1F, 0x11, 0x11, 0x1F, 0x1F, 0x1F, 0x1F };
byte ICON_BLOW[8]  = { 0x0E, 0x1F, 0x11, 0x11, 0x11, 0x11, 0x1F, 0x1F };
byte ICON_WAVE[8]  = { 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00, 0x00 };
byte ICON_UP[8]    = { 0x04, 0x0E, 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04 };
byte ICON_DOWN[8]  = { 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F, 0x0E, 0x04 };

#define CHR_THERM 0
#define CHR_BOLT  1
#define CHR_BFULL 2
#define CHR_BHALF 3
#define CHR_BLOW  4
#define CHR_WAVE  5
#define CHR_UP    6
#define CHR_DOWN  7

void initLCD() {
    lcd.begin();
    lcd.backlight();
    lcd.createChar(CHR_THERM, ICON_THERM);
    lcd.createChar(CHR_BOLT,  ICON_BOLT);
    lcd.createChar(CHR_BFULL, ICON_BFULL);
    lcd.createChar(CHR_BHALF, ICON_BHALF);
    lcd.createChar(CHR_BLOW,  ICON_BLOW);
    lcd.createChar(CHR_WAVE,  ICON_WAVE);
    lcd.createChar(CHR_UP,    ICON_UP);
    lcd.createChar(CHR_DOWN,  ICON_DOWN);

    lcd.clear();
    lcd.setCursor(2, 0); lcd.print("BATTERY MONITOR");
    lcd.setCursor(4, 1); lcd.print("ESP32 v1.0");
    lcd.setCursor(2, 2); lcd.print("Connecting WiFi");
    lcd.setCursor(4, 3); lcd.print("Please wait...");
}

void lcdShowIP(IPAddress ip) {
    lcd.clear();
    lcd.setCursor(2, 0); lcd.print("BATTERY MONITOR");
    lcd.setCursor(0, 1); lcd.print("WiFi: Connected  ");
    lcd.setCursor(0, 2); lcd.print("IP: ");
    lcd.print(ip);
    lcd.setCursor(0, 3); lcd.print("Web dashboard ready!");
    delay(3000);
    lcd.clear();
}

void updateLCD() {
    char buf[21];

    // --- Row 0: Voltage | SOC ---
    // Battery icon based on SOC
    lcd.setCursor(0, 0);
    if      (batt.soc >= 60) lcd.write(CHR_BFULL);
    else if (batt.soc >= 30) lcd.write(CHR_BHALF);
    else                     lcd.write(CHR_BLOW);
    
    snprintf(buf, sizeof(buf), " %5.2fV  SOC:%3.0f%%", batt.voltage, batt.soc);
    lcd.print(buf);

    // --- Row 1: Current | Power ---
    lcd.setCursor(0, 1);
    lcd.write(batt.charging ? CHR_UP : CHR_DOWN);
    snprintf(buf, sizeof(buf), " %5.2fA  Pwr:%4.1fW", batt.current, batt.power);
    lcd.print(buf);

    // --- Row 2: Temperature ---
    lcd.setCursor(0, 2);
    lcd.write(CHR_THERM);
    if (batt.sensorOK) {
        snprintf(buf, sizeof(buf), " Temp: %5.1f", batt.temperature);
        lcd.print(buf);
        lcd.print((char)0xDF);  // degree symbol
        lcd.print("C      ");
    } else {
        lcd.print(" Temp: -- FAULT --  ");
    }

    // --- Row 3: Status Message ---
    lcd.setCursor(0, 3);
    if (batt.tempCrit) {
        lcd.write(CHR_BOLT);
        lcd.print(" CRITICAL TEMP!!!   ");
    } else if (batt.overVolt) {
        lcd.write(CHR_BOLT);
        lcd.print(" OVERVOLTAGE!!!     ");
    } else if (batt.tempWarn) {
        lcd.write(CHR_WAVE);
        lcd.print(" HIGH TEMP WARNING  ");
    } else if (batt.lowBatt && !batt.charging) {
        lcd.write(CHR_BLOW);
        lcd.print(" LOW BATTERY!!!     ");
    } else if (batt.charging) {
        lcd.write(CHR_BOLT);
        lcd.print(" CHARGING...        ");
    } else {
        lcd.write(CHR_WAVE);
        lcd.print(" DISCHARGING        ");
    }
}

// ============================================================
//  WEB SERVER — HTML Dashboard
// ============================================================

// The dashboard HTML is stored in program memory (PROGMEM) to save RAM
const char HTML_DASHBOARD[] PROGMEM = R"XYZEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Battery Monitor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;400;600;700&display=swap" rel="stylesheet">
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  :root {
    --bg:       #070b12;
    --surface:  #0d1420;
    --card:     #111c2d;
    --border:   #1e3050;
    --green:    #00e887;
    --blue:     #38b6ff;
    --amber:    #f5a623;
    --red:      #ff4560;
    --text:     #c8d8f0;
    --muted:    #4a6080;
    --glow-g:   rgba(0,232,135,0.18);
    --glow-b:   rgba(56,182,255,0.18);
    --glow-a:   rgba(245,166,35,0.18);
    --glow-r:   rgba(255,69,96,0.18);
    --mono:     'Share Tech Mono', monospace;
    --sans:     'Exo 2', sans-serif;
  }

  html, body {
    background: var(--bg);
    color: var(--text);
    font-family: var(--sans);
    min-height: 100vh;
    overflow-x: hidden;
  }

  /* Grid background */
  body::before {
    content: '';
    position: fixed;
    inset: 0;
    background-image:
      linear-gradient(rgba(56,182,255,0.03) 1px, transparent 1px),
      linear-gradient(90deg, rgba(56,182,255,0.03) 1px, transparent 1px);
    background-size: 40px 40px;
    pointer-events: none;
    z-index: 0;
  }

  .wrapper {
    position: relative;
    z-index: 1;
    max-width: 1100px;
    margin: 0 auto;
    padding: 24px 16px;
  }

  /* ── Header ── */
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 28px;
    padding-bottom: 18px;
    border-bottom: 1px solid var(--border);
  }
  .header-left h1 {
    font-size: 1.4rem;
    font-weight: 700;
    letter-spacing: .12em;
    text-transform: uppercase;
    color: var(--blue);
    text-shadow: 0 0 18px var(--glow-b);
  }
  .header-left p {
    font-family: var(--mono);
    font-size: .72rem;
    color: var(--muted);
    margin-top: 3px;
    letter-spacing: .08em;
  }
  .status-pill {
    display: flex;
    align-items: center;
    gap: 8px;
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 999px;
    padding: 6px 16px;
    font-family: var(--mono);
    font-size: .72rem;
    letter-spacing: .06em;
  }
  .status-dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--green);
    box-shadow: 0 0 8px var(--green);
    animation: pulse 2s ease-in-out infinite;
  }
  @keyframes pulse {
    0%,100% { opacity:1; } 50% { opacity:.4; }
  }

  /* ── Battery Visual ── */
  .battery-section {
    display: flex;
    align-items: center;
    gap: 32px;
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 28px 32px;
    margin-bottom: 24px;
    position: relative;
    overflow: hidden;
  }
  .battery-section::before {
    content: '';
    position: absolute;
    inset: 0;
    background: radial-gradient(ellipse at 30% 50%, rgba(56,182,255,0.05) 0%, transparent 70%);
    pointer-events: none;
  }

  .battery-visual {
    flex-shrink: 0;
    position: relative;
    width: 100px; height: 52px;
  }
  .batt-body {
    position: absolute;
    left: 0; top: 0;
    width: 90px; height: 52px;
    border: 3px solid var(--blue);
    border-radius: 6px;
    overflow: hidden;
    box-shadow: 0 0 16px var(--glow-b);
  }
  .batt-fill {
    position: absolute;
    bottom: 0; left: 0;
    width: 0%;
    height: 100%;
    background: var(--green);
    transition: width 1s ease, background 0.5s ease;
    box-shadow: 0 0 20px var(--glow-g) inset;
  }
  .batt-lines {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    z-index: 1;
  }
  .batt-lines span {
    display: block;
    width: 2px; height: 26px;
    background: rgba(0,0,0,0.3);
    border-radius: 1px;
  }
  .batt-tip {
    position: absolute;
    right: -10px; top: 50%;
    transform: translateY(-50%);
    width: 10px; height: 22px;
    background: var(--blue);
    border-radius: 0 3px 3px 0;
    box-shadow: 0 0 8px var(--glow-b);
  }

  .battery-soc {
    flex: 1;
  }
  .soc-value {
    font-family: var(--mono);
    font-size: 4rem;
    font-weight: 400;
    line-height: 1;
    color: var(--green);
    text-shadow: 0 0 30px var(--glow-g);
    transition: color 0.5s;
  }
  .soc-unit {
    font-size: 1.8rem;
    opacity: .6;
  }
  .soc-label {
    font-size: .7rem;
    letter-spacing: .14em;
    text-transform: uppercase;
    color: var(--muted);
    margin-top: 4px;
  }

  .batt-status-badge {
    margin-left: auto;
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 10px;
  }
  .badge {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    padding: 5px 14px;
    border-radius: 6px;
    font-size: .72rem;
    font-weight: 600;
    letter-spacing: .1em;
    text-transform: uppercase;
    border: 1px solid;
  }
  .badge-charging { color:var(--green); border-color:var(--green); background:var(--glow-g); }
  .badge-discharging { color:var(--blue); border-color:var(--blue); background:var(--glow-b); }
  .badge-warn { color:var(--amber); border-color:var(--amber); background:var(--glow-a); }
  .badge-crit { color:var(--red); border-color:var(--red); background:var(--glow-r); animation: blink .8s step-start infinite; }
  @keyframes blink { 50% { opacity:0.4; } }

  /* ── Metrics Grid ── */
  .metrics-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
  }

  .metric-card {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 20px 22px;
    position: relative;
    overflow: hidden;
    transition: border-color 0.4s, box-shadow 0.4s;
  }
  .metric-card::after {
    content: '';
    position: absolute;
    top: 0; left: 0;
    width: 3px; height: 100%;
    border-radius: 3px 0 0 3px;
  }
  .card-voltage::after  { background: var(--blue);  }
  .card-current::after  { background: var(--green); }
  .card-power::after    { background: var(--amber); }
  .card-temp::after     { background: var(--red);   }

  .metric-label {
    font-size: .65rem;
    letter-spacing: .14em;
    text-transform: uppercase;
    color: var(--muted);
    margin-bottom: 10px;
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .metric-icon {
    font-size: .85rem;
  }
  .metric-value {
    font-family: var(--mono);
    font-size: 2.4rem;
    font-weight: 400;
    line-height: 1;
    transition: color 0.4s;
  }
  .metric-unit {
    font-size: 1rem;
    opacity: .55;
    margin-left: 3px;
  }
  .metric-sub {
    font-size: .68rem;
    color: var(--muted);
    margin-top: 8px;
    font-family: var(--mono);
  }

  /* Bar indicator */
  .metric-bar-bg {
    height: 4px;
    background: rgba(255,255,255,0.07);
    border-radius: 2px;
    margin-top: 12px;
    overflow: hidden;
  }
  .metric-bar {
    height: 100%;
    border-radius: 2px;
    transition: width 1s ease;
  }

  /* ── Alerts Section ── */
  .alerts-section {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 18px 22px;
    margin-bottom: 24px;
  }
  .alerts-title {
    font-size: .65rem;
    letter-spacing: .14em;
    text-transform: uppercase;
    color: var(--muted);
    margin-bottom: 14px;
  }
  .alerts-list {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
  }
  .alert-item {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 5px 12px;
    border-radius: 6px;
    font-size: .7rem;
    font-weight: 600;
    letter-spacing: .08em;
    border: 1px solid;
    opacity: 0.35;
    transition: opacity 0.4s, box-shadow 0.4s;
  }
  .alert-item.active {
    opacity: 1;
  }
  .alert-ok   { color:var(--green); border-color:var(--green); }
  .alert-warn { color:var(--amber); border-color:var(--amber); }
  .alert-crit { color:var(--red);   border-color:var(--red);   }
  .alert-item.active.alert-crit { box-shadow: 0 0 12px var(--glow-r); }
  .alert-item.active.alert-warn { box-shadow: 0 0 12px var(--glow-a); }

  /* ── History Sparklines ── */
  .history-section {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 20px 22px;
    margin-bottom: 24px;
  }
  .history-title {
    font-size: .65rem;
    letter-spacing: .14em;
    text-transform: uppercase;
    color: var(--muted);
    margin-bottom: 16px;
  }
  .sparklines {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
  }
  .sparkline-wrap { }
  .sparkline-label {
    font-size: .62rem;
    color: var(--muted);
    letter-spacing: .1em;
    text-transform: uppercase;
    margin-bottom: 6px;
  }
  canvas.spark {
    width: 100%;
    height: 56px;
    display: block;
    border-radius: 4px;
  }

  /* ── Footer ── */
  footer {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-family: var(--mono);
    font-size: .65rem;
    color: var(--muted);
    padding-top: 16px;
    border-top: 1px solid var(--border);
  }
  .uptime { letter-spacing: .06em; }
  .refresh-label { letter-spacing: .06em; }

  @media (max-width: 600px) {
    .battery-section { flex-direction: column; align-items: flex-start; }
    .batt-status-badge { margin-left: 0; flex-direction: row; flex-wrap: wrap; }
    .soc-value { font-size: 3rem; }
    .sparklines { grid-template-columns: 1fr; }
  }
</style>
</head>
<body>
<div class="wrapper">

  <!-- Header -->
  <header>
    <div class="header-left">
      <h1>&#9889; Battery Monitor</h1>
      <p id="ts">Connecting to ESP32...</p>
    </div>
    <div class="status-pill">
      <span class="status-dot" id="liveIndicator"></span>
      <span id="liveLabel">LIVE</span>
    </div>
  </header>

  <!-- Battery Visual -->
  <div class="battery-section">
    <div class="battery-visual">
      <div class="batt-body">
        <div class="batt-fill" id="battFill"></div>
        <div class="batt-lines">
          <span></span><span></span><span></span>
        </div>
      </div>
      <div class="batt-tip"></div>
    </div>

    <div class="battery-soc">
      <div class="soc-value" id="socValue">--<span class="soc-unit">%</span></div>
      <div class="soc-label">State of Charge</div>
    </div>

    <div class="batt-status-badge">
      <span class="badge" id="chargeBadge">⟳ READING...</span>
      <span class="badge badge-warn" id="warnBadge" style="display:none">⚠ ALERT</span>
    </div>
  </div>

  <!-- Metrics -->
  <div class="metrics-grid">
    <div class="metric-card card-voltage">
      <div class="metric-label"><span class="metric-icon">⚡</span>Voltage</div>
      <div class="metric-value" id="voltVal" style="color:var(--blue)">--<span class="metric-unit">V</span></div>
      <div class="metric-sub" id="voltSub">Battery terminal voltage</div>
      <div class="metric-bar-bg">
        <div class="metric-bar" id="voltBar" style="width:0%;background:var(--blue)"></div>
      </div>
    </div>

    <div class="metric-card card-current">
      <div class="metric-label"><span class="metric-icon">↕</span>Current</div>
      <div class="metric-value" id="currVal" style="color:var(--green)">--<span class="metric-unit">A</span></div>
      <div class="metric-sub" id="currSub">+ charge  /  − discharge</div>
      <div class="metric-bar-bg">
        <div class="metric-bar" id="currBar" style="width:0%;background:var(--green)"></div>
      </div>
    </div>

    <div class="metric-card card-power">
      <div class="metric-label"><span class="metric-icon">◈</span>Power</div>
      <div class="metric-value" id="powerVal" style="color:var(--amber)">--<span class="metric-unit">W</span></div>
      <div class="metric-sub" id="powerSub">Instantaneous power</div>
      <div class="metric-bar-bg">
        <div class="metric-bar" id="powerBar" style="width:0%;background:var(--amber)"></div>
      </div>
    </div>

    <div class="metric-card card-temp">
      <div class="metric-label"><span class="metric-icon">◉</span>Temperature</div>
      <div class="metric-value" id="tempVal" style="color:var(--red)">--<span class="metric-unit">°C</span></div>
      <div class="metric-sub" id="tempSub">Battery pack sensor</div>
      <div class="metric-bar-bg">
        <div class="metric-bar" id="tempBar" style="width:0%;background:var(--red)"></div>
      </div>
    </div>
  </div>

  <!-- Alerts -->
  <div class="alerts-section">
    <div class="alerts-title">System Alerts</div>
    <div class="alerts-list">
      <span class="alert-item alert-ok"  id="al-normal">✓ NORMAL</span>
      <span class="alert-item alert-warn" id="al-low">⚡ LOW BATTERY</span>
      <span class="alert-item alert-crit" id="al-ov">⚠ OVERVOLTAGE</span>
      <span class="alert-item alert-warn" id="al-tw">🌡 HIGH TEMP</span>
      <span class="alert-item alert-crit" id="al-tc">🔥 CRITICAL TEMP</span>
      <span class="alert-item alert-warn" id="al-sf">✖ SENSOR FAULT</span>
    </div>
  </div>

  <!-- Sparklines -->
  <div class="history-section">
    <div class="history-title">Trend — last 60 readings</div>
    <div class="sparklines">
      <div class="sparkline-wrap">
        <div class="sparkline-label">Voltage (V)</div>
        <canvas class="spark" id="spVolt" width="400" height="56"></canvas>
      </div>
      <div class="sparkline-wrap">
        <div class="sparkline-label">Current (A)</div>
        <canvas class="spark" id="spCurr" width="400" height="56"></canvas>
      </div>
      <div class="sparkline-wrap">
        <div class="sparkline-label">Power (W)</div>
        <canvas class="spark" id="spPow" width="400" height="56"></canvas>
      </div>
      <div class="sparkline-wrap">
        <div class="sparkline-label">Temperature (°C)</div>
        <canvas class="spark" id="spTemp" width="400" height="56"></canvas>
      </div>
    </div>
  </div>

  <!-- Footer -->
  <footer>
    <span class="uptime">UPTIME: <span id="uptimeVal">--</span></span>
    <span class="refresh-label">AUTO-REFRESH 2s · ESP32 MONITOR</span>
  </footer>
</div>

<script>
const MAX_HIST = 60;
const hist = { v:[], i:[], p:[], t:[] };

function drawSpark(canvasId, data, color) {
  const c = document.getElementById(canvasId);
  if (!c || data.length < 2) return;
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height;
  ctx.clearRect(0,0,W,H);

  const min = Math.min(...data), max = Math.max(...data);
  const range = max - min || 1;

  const toY = v => H - 4 - ((v - min) / range) * (H - 8);
  const toX = (i,n) => (i / (n-1)) * W;

const grad = ctx.createLinearGradient(0,0,0,H);
  const colorAlpha = color.startsWith('#')
    ? color : color.replace('rgb(','rgba(').replace(')','');
  grad.addColorStop(0, color.startsWith('#') ? color + '59' : colorAlpha + ',0.35)');
  grad.addColorStop(1, color.startsWith('#') ? color + '00' : colorAlpha + ',0)');

  ctx.beginPath();
  data.forEach((v,i) => {
    const x = toX(i, data.length), y = toY(v);
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.lineTo(toX(data.length-1, data.length), H);
  ctx.lineTo(0, H);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Line
  ctx.beginPath();
  data.forEach((v,i) => {
    const x = toX(i, data.length), y = toY(v);
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.lineJoin = 'round';
  ctx.stroke();

  const lx = toX(data.length-1, data.length), ly = toY(data[data.length-1]);
  ctx.beginPath();
  ctx.arc(lx, ly, 3, 0, Math.PI*2);
  ctx.fillStyle = color;
  ctx.fill();
}

function socColor(soc) {
  if (soc >= 50) return 'var(--green)';
  if (soc >= 20) return 'var(--amber)';
  return 'var(--red)';
}
function tempColor(t) {
  if (t >= 60) return 'var(--red)';
  if (t >= 45) return 'var(--amber)';
  return 'var(--red)';
}
function fmtUptime(ms) {
  const s = Math.floor(ms/1000);
  const h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sc = s%60;
  return `${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(sc).padStart(2,'0')}`;
}

// ── UI update ─────────────────────────────────────────────
function updateUI(d) {
  // Push history
  const push = (arr, v) => { arr.push(v); if(arr.length>MAX_HIST) arr.shift(); };
  push(hist.v, d.voltage); push(hist.i, d.current);
  push(hist.p, d.power);   push(hist.t, d.temperature);

  // SOC
  document.getElementById('socValue').innerHTML =
    d.soc.toFixed(0) + '<span class="soc-unit">%</span>';
  document.getElementById('socValue').style.color = socColor(d.soc);
  const fill = document.getElementById('battFill');
  fill.style.width = d.soc + '%';
  fill.style.background = d.soc >= 50 ? 'var(--green)'
                        : d.soc >= 20 ? 'var(--amber)' : 'var(--red)';

  // Charge badge
  const cb = document.getElementById('chargeBadge');
  if (d.charging) {
    cb.className = 'badge badge-charging';
    cb.textContent = '↑ CHARGING';
  } else {
    cb.className = 'badge badge-discharging';
    cb.textContent = '↓ DISCHARGING';
  }

  // Warn badge
  const wb = document.getElementById('warnBadge');
  const hasWarn = d.tempWarn || d.tempCrit || d.lowBatt || d.overVolt || !d.sensorOK;
  wb.style.display = hasWarn ? '' : 'none';
  if (d.tempCrit || d.overVolt) { wb.className='badge badge-crit'; wb.textContent='⚠ CRITICAL'; }
  else { wb.className='badge badge-warn'; wb.textContent='⚠ ALERT'; }

  // Voltage card
  document.getElementById('voltVal').innerHTML =
    d.voltage.toFixed(2) + '<span class="metric-unit">V</span>';
  const vp = ((d.voltage - 9) / (15 - 9) * 100).toFixed(0);
  document.getElementById('voltBar').style.width = Math.max(0,Math.min(100,vp)) + '%';
  document.getElementById('voltSub').textContent =
    d.overVolt ? '⚠ OVERVOLTAGE!' : 'Battery terminal voltage';

  // Current card
  const ia = Math.abs(d.current);
  document.getElementById('currVal').innerHTML =
    (d.current >= 0 ? '+' : '') + d.current.toFixed(2) + '<span class="metric-unit">A</span>';
  document.getElementById('currBar').style.width = Math.min(100, ia/5*100).toFixed(0) + '%';
  document.getElementById('currBar').style.background = d.charging ? 'var(--green)' : 'var(--blue)';
  document.getElementById('currSub').textContent =
    d.charging ? 'Charging current' : 'Discharge current';

  // Power card
  document.getElementById('powerVal').innerHTML =
    d.power.toFixed(1) + '<span class="metric-unit">W</span>';
  document.getElementById('powerBar').style.width = Math.min(100, d.power/75*100).toFixed(0) + '%';

  // Temperature card
  const tOK = d.temperature > -50;
  document.getElementById('tempVal').innerHTML = tOK
    ? d.temperature.toFixed(1) + '<span class="metric-unit">°C</span>'
    : '<span style="font-size:1.4rem;color:var(--red)">FAULT</span>';
  document.getElementById('tempVal').style.color = tOK ? tempColor(d.temperature) : 'var(--red)';
  document.getElementById('tempBar').style.width = tOK ? Math.min(100, d.temperature/80*100).toFixed(0)+'%' : '100%';
  document.getElementById('tempBar').style.background = tOK ? tempColor(d.temperature) : 'var(--red)';
  document.getElementById('tempSub').textContent = tOK
    ? (d.tempCrit ? '🔥 CRITICAL!' : d.tempWarn ? '⚠ High temperature' : 'Battery pack sensor')
    : '✖ DS18B20 disconnected';

  // Alert items
  const setAlert = (id, active) => {
    const el = document.getElementById(id);
    if (active) el.classList.add('active'); else el.classList.remove('active');
  };
  const allGood = !hasWarn;
  setAlert('al-normal', allGood);
  setAlert('al-low',  d.lowBatt && !d.charging);
  setAlert('al-ov',   d.overVolt);
  setAlert('al-tw',   d.tempWarn && !d.tempCrit);
  setAlert('al-tc',   d.tempCrit);
  setAlert('al-sf',   !d.sensorOK);

  // Timestamp
  document.getElementById('ts').textContent =
    'Last update: ' + new Date().toLocaleTimeString();
  document.getElementById('uptimeVal').textContent = fmtUptime(d.uptime);

  // Sparklines
  drawSpark('spVolt', hist.v, '#38b6ff');
  drawSpark('spCurr', hist.i, '#00e887');
  drawSpark('spPow',  hist.p, '#f5a623');
  drawSpark('spTemp', hist.t, '#ff4560');
}

// ── Polling ──────────────────────────────────────────────
let failCount = 0;
async function fetchData() {
  try {
    const r = await fetch('/data');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const d = await r.json();
    updateUI(d);
    failCount = 0;
    document.getElementById('liveIndicator').style.background = 'var(--green)';
    document.getElementById('liveLabel').textContent = 'LIVE';
  } catch(e) {
    failCount++;
    if (failCount > 2) {
      document.getElementById('liveIndicator').style.background = 'var(--red)';
      document.getElementById('liveLabel').textContent = 'OFFLINE';
    }
  }
}
fetchData();
setInterval(fetchData, 2000);
</script>
</body>
</html>
)XYZEOF";

// ============================================================
//  Web Handlers
// ============================================================

void handleRoot() {
    server.send_P(200, "text/html", HTML_DASHBOARD);
}

void handleData() {
    // Build JSON response
    char json[380];
    snprintf(json, sizeof(json),
        "{"
        "\"voltage\":%.3f,"
        "\"current\":%.3f,"
        "\"power\":%.2f,"
        "\"temperature\":%.2f,"
        "\"soc\":%.1f,"
        "\"charging\":%s,"
        "\"tempWarn\":%s,"
        "\"tempCrit\":%s,"
        "\"lowBatt\":%s,"
        "\"overVolt\":%s,"
        "\"sensorOK\":%s,"
        "\"uptime\":%lu"
        "}",
        batt.voltage,
        batt.current,
        batt.power,
        batt.temperature,
        batt.soc,
        batt.charging  ? "true" : "false",
        batt.tempWarn  ? "true" : "false",
        batt.tempCrit  ? "true" : "false",
        batt.lowBatt   ? "true" : "false",
        batt.overVolt  ? "true" : "false",
        batt.sensorOK  ? "true" : "false",
        batt.timestamp
    );
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n[BOOT] Battery Monitor v1.0");

    // Init LCD
    initLCD();

    // Init DS18B20
    ds18b20.begin();
    Serial.printf("[DS18B20] Found %d device(s)\n", ds18b20.getDeviceCount());
    ds18b20.setResolution(12);         // 12-bit = 0.0625°C resolution
    ds18b20.setWaitForConversion(true);

    // Configure ADC
    analogReadResolution(12);          // 12-bit ADC
    analogSetAttenuation(ADC_11db);    // 0–3.3V range
    // Optionally set per-pin:
    // analogSetPinAttenuation(PIN_VOLTAGE, ADC_11db);
    // analogSetPinAttenuation(PIN_CURRENT, ADC_11db);

    // Connect WiFi
    Serial.printf("[WiFi] Connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
        if (millis() - wifiStart > 15000) {
            // WiFi timeout — continue without web server
            Serial.println("\n[WiFi] TIMEOUT — running in LCD-only mode");
            lcd.clear();
            lcd.setCursor(0, 0); lcd.print("WiFi TIMEOUT!");
            lcd.setCursor(0, 1); lcd.print("LCD-only mode");
            delay(2000);
            lcd.clear();
            goto skip_wifi;
        }
    }
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    lcdShowIP(WiFi.localIP());

    // Web server routes
    server.on("/",     HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleData);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[WebServer] Started on port 80");

    skip_wifi:

    // Initial sensor read
    updateSensors();
    updateLCD();

    Serial.println("[READY] Battery Monitor running.");
    Serial.printf("  Voltage:     %.3f V\n",  batt.voltage);
    Serial.printf("  Current:     %.3f A\n",  batt.current);
    Serial.printf("  Temperature: %.2f C\n",  batt.temperature);
    Serial.printf("  SOC:         %.1f %%\n", batt.soc);
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
    server.handleClient();

    unsigned long now = millis();
    if (now - lastUpdateMs >= UPDATE_MS) {
        lastUpdateMs = now;
        updateSensors();
        updateLCD();

        // Serial debug log
        Serial.printf("[%8lu ms] V=%.3fV  I=%+.3fA  P=%.2fW  T=%.2f°C  SOC=%.1f%%  %s\n",
            now,
            batt.voltage, batt.current, batt.power,
            batt.temperature, batt.soc,
            batt.charging ? "CHARGING" : "DISCHARGING"
        );
    }

    // WiFi reconnect watchdog
    static unsigned long lastReconnect = 0;
    if (WiFi.status() != WL_CONNECTED && now - lastReconnect > 30000) {
        lastReconnect = now;
        Serial.println("[WiFi] Reconnecting...");
        WiFi.reconnect();
    }
}
