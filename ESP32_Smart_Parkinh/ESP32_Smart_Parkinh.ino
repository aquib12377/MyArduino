#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <WebServer.h>
WebServer server(80);

// ===== WiFi Access Point =====
const char* AP_SSID = "ESP32-Parking";
const char* AP_PASS = "12345678";

// ===== WebSocket =====
WebSocketsServer webSocket = WebSocketsServer(81);

// ===== LCD 16x2 =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== Servo =====
Servo gateServo;
const int SERVO_PIN = 18;
const int GATE_CLOSED_ANGLE = 180;
const int GATE_OPEN_ANGLE = 90;

// ===== IR Pins =====
// Slot IR sensors: LOW = object detected (IR beam broken), HIGH = clear
// Entry/Exit IR:   LOW = car present,                      HIGH = clear
const int SLOT_PINS[4] = { 32, 33, 25, 26 };
const int ENTRY_IR_PIN = 27;
const int EXIT_IR_PIN = 14;

// ===== EEPROM =====
const int EEPROM_SIZE = 32;
const int EEPROM_BASE_ADDR = 0;

// 0 = free, 1 = occupied, 2 = reserved
byte slotState[4] = { 0, 0, 0, 0 };

// ===== Timing =====
unsigned long lastSensorRead = 0;
unsigned long lastLcdUpdate = 0;
unsigned long lastBroadcast = 0;
const unsigned long SENSOR_INTERVAL = 80;
const unsigned long LCD_INTERVAL = 250;
const unsigned long BROADCAST_INTERVAL = 500;

// ===== Gate =====
bool gateOpen = false;
unsigned long gateOpenUntil = 0;
const unsigned long GATE_OPEN_TIME = 4000;  // keep gate open 4 s

// ===== Previous IR readings (for edge detection) =====
// BUG FIX: these are now initialised from real pin reads in setup(),
// NOT hardcoded to HIGH.  That way the first sensor scan never sees a
// false "transition" from a stale EEPROM value.
int lastSlotRead[4] = { HIGH, HIGH, HIGH, HIGH };  // overwritten in setup()
int lastEntryRead = HIGH;
int lastExitRead = HIGH;

// ===== LCD custom chars =====
byte carChar[8] = { B00000, B11111, B10001, B11111, B10101, B10001, B11111, B00000 };
byte arrowChar[8] = { B00100, B00010, B11111, B00010, B00100, B00000, B00000, B00000 };

// ======================================================
// HELPERS
// ======================================================

String slotModeText(byte s) {
  if (s == 0) return "FREE";
  if (s == 1) return "OCC";
  return "RES";
}

int freeSlotsCount() {
  int c = 0;
  for (int i = 0; i < 4; i++)
    if (slotState[i] == 0) c++;
  return c;
}

void saveSlotToEEPROM(int idx) {
  EEPROM.write(EEPROM_BASE_ADDR + idx, slotState[idx]);
  EEPROM.commit();
}

void loadSlotsFromEEPROM() {
  for (int i = 0; i < 4; i++) {
    byte v = EEPROM.read(EEPROM_BASE_ADDR + i);
    if (v > 2) v = 0;
    slotState[i] = v;
  }
}

// ======================================================
// GATE
// ======================================================

void openGate() {
  gateServo.write(GATE_OPEN_ANGLE);
  gateOpen = true;
  gateOpenUntil = millis() + GATE_OPEN_TIME;
}

void closeGate() {
  gateServo.write(GATE_CLOSED_ANGLE);
  gateOpen = false;
}

void handleGateTimer() {
  if (gateOpen && millis() > gateOpenUntil) {
    closeGate();
  }
}

// ======================================================
// JSON
// ======================================================

String buildStateJson() {
  String json = "{";
  json += "\"free\":" + String(freeSlotsCount()) + ",";
  json += "\"gate\":\"" + String(gateOpen ? "OPEN" : "CLOSED") + "\",";
  json += "\"slots\":[";
  for (int i = 0; i < 4; i++) {
    json += "{\"id\":" + String(i + 1) + ",";
    json += "\"state\":\"" + slotModeText(slotState[i]) + "\"}";
    if (i < 3) json += ",";
  }
  json += "]}";
  return json;
}

void broadcastState() {
  String msg = buildStateJson();

  webSocket.broadcastTXT(msg);
}

// ======================================================
// LCD
// ======================================================

void startupAnimation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  ESP32 PARKING ");
  for (int i = 0; i < 16; i++) {
    lcd.scrollDisplayLeft();
    delay(120);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM STARTING");
  lcd.setCursor(0, 1);
  lcd.write(byte(1));
  lcd.print(" LOADING ");
  lcd.write(byte(1));
  delay(1000);
  lcd.clear();
}

// Non-blocking gate animation — just shows text, no delay loop.
// Servo was already commanded in handleEntryExit() BEFORE this runs.
void showGateAnimation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GATE OPENING...");
  lcd.setCursor(0, 1);
  lcd.write(byte(0));
  lcd.print(" WELCOME! ");
  lcd.write(byte(0));
  // No long delay here.  LCD will be overwritten by updateLCD() on next cycle.
}

void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("F:");
  lcd.print(freeSlotsCount());
  lcd.print("/4 ");
  lcd.print("G:");
  lcd.print(gateOpen ? "O" : "C");
  lcd.print(" ");
  for (int i = 0; i < 4; i++) {
    if (slotState[i] == 0) lcd.print("F");
    else if (slotState[i] == 1) lcd.print("O");
    else lcd.print("R");
  }

  lcd.setCursor(0, 1);
  lcd.print("S1");
  lcd.print(slotModeText(slotState[0]));
  lcd.print(" S2");
  lcd.print(slotModeText(slotState[1]));

  delay(5);
}

// ======================================================
// SLOT IR LOGIC
// ======================================================
//
// How it works:
//   • IR sensor modules output LOW  when an object breaks the beam (car present).
//   • IR sensor modules output HIGH when the beam is clear (no car).
//   • Pins use INPUT_PULLUP, so a disconnected pin safely reads HIGH (= clear).
//
// State machine (per slot):
//   FREE (0)     → IR goes LOW (car parks) → OCCUPIED (1), saved to EEPROM
//   OCCUPIED (1) → IR goes HIGH (car leaves) → FREE (0),     saved to EEPROM
//   RESERVED (2) → only changed via manual command, IR ignored for this slot

void handleSlotLogic() {
  for (int i = 0; i < 4; i++) {
    int r = digitalRead(SLOT_PINS[i]);

    if (r != lastSlotRead[i]) {
      delay(15);  // debounce
      r = digitalRead(SLOT_PINS[i]);
      if (r == lastSlotRead[i]) continue;  // glitch — ignore
      lastSlotRead[i] = r;

      if (r == LOW && slotState[i] == 0) {
        // Beam broken & slot was free → car has parked → OCCUPIED
        slotState[i] = 1;
        saveSlotToEEPROM(i);

      } else if (r == HIGH && slotState[i] == 1) {
        // Beam clear again & slot was occupied → car has left → FREE
        slotState[i] = 0;
        saveSlotToEEPROM(i);
      }
      // RESERVED (2) slots: IR changes are ignored; state only changes via WebSocket cmd
    }
  }
}

// ======================================================
// ENTRY / EXIT IR LOGIC
// ======================================================
//
// How it works:
//   Entry IR pin 27 — mounted at the entrance boom gate.
//   Exit IR pin 14  — mounted at the exit boom gate.
//
//   When a car pulls up to either sensor the beam is broken → pin reads LOW.
//   We detect the FALLING EDGE (HIGH→LOW) = "car just arrived".
//
//   BUG FIX: openGate() is called FIRST (servo moves immediately),
//   then showGateAnimation() updates the LCD — no blocking delay before servo.
//
//   Entry gate only opens if at least one slot is free (no point letting
//   a car in if the car park is full).
//   Exit gate always opens (car must be able to leave).

void handleEntryExit() {
  int entryNow = digitalRead(ENTRY_IR_PIN);
  int exitNow = digitalRead(EXIT_IR_PIN);

  // --- ENTRY: falling edge = car arrives at entrance ---
  if (entryNow == LOW && lastEntryRead == HIGH) {
    if (freeSlotsCount() > 0) {
      openGate();           // ← servo moves IMMEDIATELY
      showGateAnimation();  // ← LCD update after (no delay inside)
    } else {
      // Car park full — flash "FULL" on LCD, do NOT open gate
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  CAR PARK FULL ");
      lcd.setCursor(0, 1);
      lcd.print("  PLEASE WAIT   ");
    }
  }

  // --- EXIT: falling edge = car arrives at exit ---
  if (exitNow == LOW && lastExitRead == HIGH) {
    openGate();           // ← servo moves IMMEDIATELY
    showGateAnimation();  // ← LCD update after
  }

  lastEntryRead = entryNow;
  lastExitRead = exitNow;
}

// ======================================================
// HTML PAGE (served over HTTP on port 80)
// ======================================================

String htmlPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SmartPark — ESP32 Control</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=DM+Sans:wght@300;400;500;600&display=swap" rel="stylesheet">
<style>
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

  :root {
    --bg:        #0a0c10;
    --surface:   #111318;
    --surface2:  #181c24;
    --border:    rgba(255,255,255,0.06);
    --border2:   rgba(255,255,255,0.12);
    --text:      #e8eaf0;
    --muted:     #5a6070;
    --amber:     #f59e0b;
    --amber-dim: rgba(245,158,11,0.12);
    --green:     #22c55e;
    --green-dim: rgba(34,197,94,0.1);
    --red:       #ef4444;
    --red-dim:   rgba(239,68,68,0.1);
    --orange:    #f97316;
    --orange-dim:rgba(249,115,22,0.1);
    --blue:      #60a5fa;
    --blue-dim:  rgba(96,165,250,0.1);
    --mono:      'Space Mono', monospace;
    --sans:      'DM Sans', sans-serif;
    --r:         12px;
    --r-sm:      8px;
  }

  html, body { height: 100%; background: var(--bg); color: var(--text); font-family: var(--sans); }
  body { display: flex; flex-direction: column; min-height: 100vh; overflow-x: hidden; }

  body::before {
    content: '';
    position: fixed; inset: 0;
    background-image:
      linear-gradient(rgba(255,255,255,0.025) 1px, transparent 1px),
      linear-gradient(90deg, rgba(255,255,255,0.025) 1px, transparent 1px);
    background-size: 40px 40px;
    pointer-events: none;
    z-index: 0;
  }

  .page { position: relative; z-index: 1; max-width: 1180px; margin: 0 auto; padding: 28px 20px 48px; width: 100%; }

  header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 32px; flex-wrap: wrap; gap: 16px; }
  .logo { display: flex; align-items: center; gap: 12px; }
  .logo-icon { width: 40px; height: 40px; background: var(--amber); border-radius: var(--r-sm); display: flex; align-items: center; justify-content: center; flex-shrink: 0; }
  .logo-icon svg { width: 22px; height: 22px; }
  .logo-text h1 { font-family: var(--mono); font-size: 18px; font-weight: 700; letter-spacing: -0.5px; color: #fff; }
  .logo-text p  { font-size: 12px; color: var(--muted); font-family: var(--mono); margin-top: 1px; }
  .header-right { display: flex; align-items: center; gap: 12px; }

  .conn-badge { display: flex; align-items: center; gap: 8px; padding: 8px 14px; border-radius: 999px; border: 1px solid var(--border2); background: var(--surface); font-family: var(--mono); font-size: 11px; letter-spacing: 0.5px; transition: all 0.3s; }
  .conn-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--green); animation: pulse-green 2s infinite; }
  .conn-badge.offline .conn-dot { background: var(--red); animation: none; }
  .conn-badge.offline { border-color: rgba(239,68,68,0.3); }

  @keyframes pulse-green {
    0%   { box-shadow: 0 0 0 0 rgba(34,197,94,0.5); }
    70%  { box-shadow: 0 0 0 6px rgba(34,197,94,0); }
    100% { box-shadow: 0 0 0 0 rgba(34,197,94,0); }
  }

  .time-badge { font-family: var(--mono); font-size: 12px; color: var(--muted); padding: 8px 14px; border: 1px solid var(--border); border-radius: 999px; background: var(--surface); letter-spacing: 0.5px; }

  .stats { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; margin-bottom: 24px; }
  @media (max-width: 700px) { .stats { grid-template-columns: repeat(2, 1fr); } }

  .stat-card { background: var(--surface); border: 1px solid var(--border); border-radius: var(--r); padding: 20px; position: relative; overflow: hidden; }
  .stat-card::after { content: ''; position: absolute; bottom: 0; left: 0; right: 0; height: 2px; border-radius: 0 0 var(--r) var(--r); background: var(--accent, transparent); }
  .stat-card.accent-green { --accent: var(--green); }
  .stat-card.accent-amber { --accent: var(--amber); }
  .stat-card.accent-blue  { --accent: var(--blue);  }
  .stat-card.accent-red   { --accent: var(--red);   }

  .stat-label { font-size: 11px; text-transform: uppercase; letter-spacing: 1px; color: var(--muted); margin-bottom: 12px; font-family: var(--mono); }
  .stat-value { font-family: var(--mono); font-size: 36px; font-weight: 700; line-height: 1; color: #fff; }
  .stat-value.green { color: var(--green); }
  .stat-value.amber { color: var(--amber); }
  .stat-value.blue  { color: var(--blue);  }
  .stat-value.red   { color: var(--red);   }
  .stat-sub { margin-top: 6px; font-size: 12px; color: var(--muted); }
  .stat-icon { position: absolute; top: 16px; right: 16px; opacity: 0.08; font-size: 40px; line-height: 1; }

  .main-grid { display: grid; grid-template-columns: 1fr 320px; gap: 16px; align-items: start; }
  @media (max-width: 900px) { .main-grid { grid-template-columns: 1fr; } }

  .section-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 14px; }
  .section-title { font-family: var(--mono); font-size: 11px; text-transform: uppercase; letter-spacing: 1.5px; color: var(--muted); }

  .slot-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px; }
  @media (max-width: 500px) { .slot-grid { grid-template-columns: 1fr; } }

  .slot-card { background: var(--surface); border: 1px solid var(--border); border-radius: var(--r); padding: 20px; cursor: default; transition: border-color 0.4s, background 0.4s, transform 0.2s; position: relative; overflow: hidden; }
  .slot-card:hover { transform: translateY(-1px); }
  .slot-card::before { content: ''; position: absolute; inset: 0; background: var(--slot-bg, transparent); border-radius: var(--r); transition: background 0.4s; pointer-events: none; }
  .slot-card.free { border-color: rgba(34,197,94,0.25);  --slot-bg: var(--green-dim); }
  .slot-card.occ  { border-color: rgba(239,68,68,0.25);  --slot-bg: var(--red-dim);   }
  .slot-card.res  { border-color: rgba(249,115,22,0.3);  --slot-bg: var(--orange-dim);}

  .slot-top { display: flex; align-items: flex-start; justify-content: space-between; position: relative; }
  .slot-num { font-family: var(--mono); font-size: 11px; color: var(--muted); letter-spacing: 1px; text-transform: uppercase; }
  .slot-badge { font-family: var(--mono); font-size: 10px; font-weight: 700; letter-spacing: 1px; padding: 4px 10px; border-radius: 999px; text-transform: uppercase; transition: background 0.4s, color 0.4s; }
  .badge-free { background: rgba(34,197,94,0.15); color: var(--green); }
  .badge-occ  { background: rgba(239,68,68,0.15); color: var(--red);   }
  .badge-res  { background: rgba(249,115,22,0.15); color: var(--orange);}

  .slot-icon { margin: 18px 0 4px; }
  .parking-graphic { width: 100%; height: 72px; }

  .slot-label { font-family: var(--mono); font-size: 22px; font-weight: 700; line-height: 1; margin-top: 14px; transition: color 0.4s; }
  .slot-label.free { color: var(--green); }
  .slot-label.occ  { color: var(--red);   }
  .slot-label.res  { color: var(--orange);}

  .slot-detail { font-size: 12px; color: var(--muted); margin-top: 5px; font-family: var(--mono); }
  .ir-dot { display: inline-block; width: 6px; height: 6px; border-radius: 50%; background: var(--muted); margin-right: 5px; vertical-align: middle; transition: background 0.3s; }
  .ir-dot.active { background: var(--red); box-shadow: 0 0 6px var(--red); }

  .sidebar { display: flex; flex-direction: column; gap: 14px; }
  .panel { background: var(--surface); border: 1px solid var(--border); border-radius: var(--r); padding: 20px; }

  .gate-visual { margin: 18px 0; display: flex; align-items: flex-end; justify-content: center; height: 90px; position: relative; }
  .gate-road { width: 100%; height: 12px; background: var(--surface2); border-radius: 4px; border: 1px solid var(--border2); position: relative; overflow: hidden; }
  .gate-road::after { content: ''; position: absolute; top: 50%; left: 0; width: 100%; height: 2px; background: repeating-linear-gradient(90deg, var(--amber) 0 16px, transparent 16px 32px); transform: translateY(-50%); animation: road-move 0.8s linear infinite; }
  @keyframes road-move { from { background-position: 0 0; } to { background-position: 32px 0; } }
  .gate-post { position: absolute; bottom: 12px; left: 32px; width: 10px; height: 56px; background: var(--surface2); border: 1px solid var(--border2); border-radius: 3px 3px 0 0; }
  .gate-arm { position: absolute; bottom: 62px; left: 41px; width: 120px; height: 10px; background: linear-gradient(90deg, var(--amber) 0%, #fbbf24 100%); border-radius: 3px; transform-origin: 0 50%; transition: transform 0.8s cubic-bezier(0.4, 0, 0.2, 1); }
  .gate-arm.open { transform: rotate(-88deg); }
  .gate-stripes { position: absolute; top: 0; left: 0; right: 0; bottom: 0; background: repeating-linear-gradient(90deg, transparent 0 14px, rgba(0,0,0,0.3) 14px 20px); border-radius: 3px; pointer-events: none; }

  .gate-status-row { display: flex; align-items: center; justify-content: space-between; margin-top: 8px; }
  .gate-label { font-family: var(--mono); font-size: 22px; font-weight: 700; transition: color 0.4s; }
  .gate-label.open   { color: var(--green); }
  .gate-label.closed { color: var(--muted); }

  .gate-btn { font-family: var(--mono); font-size: 11px; letter-spacing: 0.5px; padding: 8px 16px; border-radius: var(--r-sm); border: 1px solid var(--border2); background: var(--surface2); color: var(--text); cursor: pointer; transition: all 0.2s; text-transform: uppercase; }
  .gate-btn:hover { border-color: var(--amber); color: var(--amber); }

  .log-scroll { margin-top: 12px; max-height: 160px; overflow-y: auto; display: flex; flex-direction: column; gap: 6px; }
  .log-scroll::-webkit-scrollbar { width: 3px; }
  .log-scroll::-webkit-scrollbar-thumb { background: var(--border2); border-radius: 4px; }

  .log-entry { display: flex; gap: 10px; font-family: var(--mono); font-size: 11px; line-height: 1.5; padding: 6px 10px; border-radius: var(--r-sm); background: var(--surface2); border-left: 2px solid var(--border2); animation: slide-in 0.3s ease; }
  @keyframes slide-in { from { opacity: 0; transform: translateX(-8px); } to { opacity: 1; transform: translateX(0); } }
  .log-entry.info { border-left-color: var(--blue);  }
  .log-entry.warn { border-left-color: var(--amber); }
  .log-entry.ok   { border-left-color: var(--green); }
  .log-entry.err  { border-left-color: var(--red);   }
  .log-time { color: var(--muted); flex-shrink: 0; }
  .log-msg  { color: var(--text); }

  .sensor-row { display: flex; gap: 10px; margin-top: 12px; }
  .sensor-chip { flex: 1; padding: 12px; border-radius: var(--r-sm); background: var(--surface2); border: 1px solid var(--border); display: flex; flex-direction: column; gap: 8px; transition: border-color 0.3s; }
  .sensor-chip.triggered { border-color: var(--amber); }
  .sensor-chip-label { font-family: var(--mono); font-size: 10px; color: var(--muted); letter-spacing: 1px; text-transform: uppercase; }
  .sensor-chip-value { font-family: var(--mono); font-size: 12px; font-weight: 700; color: var(--muted); transition: color 0.3s; }
  .sensor-chip.triggered .sensor-chip-value { color: var(--amber); }

  .occ-bar-wrap { margin-top: 12px; display: flex; flex-direction: column; gap: 6px; }
  .occ-bar-track { height: 8px; background: var(--surface2); border-radius: 999px; overflow: hidden; border: 1px solid var(--border); }
  .occ-bar-fill { height: 100%; border-radius: 999px; transition: width 0.7s cubic-bezier(0.4,0,0.2,1), background 0.4s; background: var(--green); }
  .occ-bar-fill.half { background: var(--amber); }
  .occ-bar-fill.full { background: var(--red);   }
  .occ-bar-labels { display: flex; justify-content: space-between; font-family: var(--mono); font-size: 11px; color: var(--muted); }

  footer { margin-top: auto; padding: 20px; text-align: center; font-family: var(--mono); font-size: 11px; color: var(--muted); letter-spacing: 0.5px; position: relative; z-index: 1; }

  @keyframes flicker { 0%,100%{opacity:1;} 92%{opacity:1;} 93%{opacity:0.7;} 94%{opacity:1;} 96%{opacity:0.6;} 97%{opacity:1;} }
  .live-tag { display: inline-flex; align-items: center; gap: 6px; font-family: var(--mono); font-size: 10px; color: var(--green); letter-spacing: 1px; text-transform: uppercase; animation: flicker 5s infinite; }
  .live-tag::before { content: ''; display: inline-block; width: 6px; height: 6px; border-radius: 50%; background: var(--green); animation: pulse-green 1.5s infinite; }

  .demo-notice { display: flex; align-items: center; gap: 10px; background: rgba(245,158,11,0.07); border: 1px solid rgba(245,158,11,0.2); border-radius: var(--r-sm); padding: 10px 14px; margin-bottom: 20px; font-family: var(--mono); font-size: 11px; color: var(--amber); letter-spacing: 0.5px; }
  .demo-notice.hidden { display: none; }
</style>
</head>
<body>
<div class="page">

  <header>
    <div class="logo">
      <div class="logo-icon">
        <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
          <rect x="3" y="3" width="18" height="18" rx="3" stroke="#0a0c10" stroke-width="2"/>
          <path d="M8 17V9h4.5a2.5 2.5 0 0 1 0 5H8" stroke="#0a0c10" stroke-width="2" stroke-linecap="round"/>
        </svg>
      </div>
      <div class="logo-text">
        <h1>SMARTPARK</h1>
        <p>ESP32 · PARKING CONTROL</p>
      </div>
    </div>
    <div class="header-right">
      <div id="conn" class="conn-badge">
        <span class="conn-dot"></span>
        <span id="conn-text">CONNECTING...</span>
      </div>
      <div class="time-badge" id="clock">--:--:--</div>
    </div>
  </header>

  <div class="demo-notice" id="demo-notice">
    ⚡ DEMO MODE — Simulating live sensor data. Connect to ESP32 for real-time updates.
  </div>

  <div class="stats">
    <div class="stat-card accent-green">
      <div class="stat-label">Available</div>
      <div class="stat-value green" id="s-free">0</div>
      <div class="stat-sub">of 4 slots</div>
      <div class="stat-icon">✓</div>
    </div>
    <div class="stat-card accent-red">
      <div class="stat-label">Occupied</div>
      <div class="stat-value red" id="s-occ">0</div>
      <div class="stat-sub">slots in use</div>
      <div class="stat-icon">P</div>
    </div>
    <div class="stat-card accent-amber">
      <div class="stat-label">Reserved</div>
      <div class="stat-value amber" id="s-res">0</div>
      <div class="stat-sub">pre-booked</div>
      <div class="stat-icon">R</div>
    </div>
    <div class="stat-card accent-blue">
      <div class="stat-label">Gate</div>
      <div class="stat-value blue" id="s-gate" style="font-size:22px;margin-top:5px">CLOSED</div>
      <div class="stat-sub">servo status</div>
      <div class="stat-icon">↑</div>
    </div>
  </div>

  <div class="main-grid">
    <div>
      <div class="section-header">
        <span class="section-title">Parking Slots</span>
        <span class="live-tag">Live Feed</span>
      </div>
      <div class="slot-grid">

        <div class="slot-card free" id="card-1">
          <div class="slot-top">
            <span class="slot-num">SLOT — 01</span>
            <span class="slot-badge badge-free" id="badge-1">FREE</span>
          </div>
          <div class="slot-icon">
            <svg class="parking-graphic" viewBox="0 0 200 72" xmlns="http://www.w3.org/2000/svg">
              <rect x="4" y="4" width="192" height="64" rx="4" fill="none" stroke="rgba(255,255,255,0.06)" stroke-width="1"/>
              <g id="car-1" opacity="0" style="transition:opacity 0.4s">
                <rect x="40" y="20" width="120" height="32" rx="6" fill="rgba(239,68,68,0.2)" stroke="rgba(239,68,68,0.4)" stroke-width="1"/>
                <rect x="56" y="14" width="88" height="22" rx="4" fill="rgba(239,68,68,0.15)" stroke="rgba(239,68,68,0.3)" stroke-width="1"/>
                <circle cx="62" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <circle cx="138" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <rect x="48" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
                <rect x="132" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
              </g>
              <text id="plabel-1" x="100" y="40" text-anchor="middle" dominant-baseline="middle" fill="rgba(34,197,94,0.2)" font-size="40" font-weight="700" font-family="monospace">P</text>
              <circle cx="16" cy="36" r="4" fill="rgba(255,255,255,0.1)"/>
            </svg>
          </div>
          <div class="slot-label free" id="label-1">AVAILABLE</div>
          <div class="slot-detail" id="detail-1"><span class="ir-dot" id="ird-1"></span>IR: CLEAR</div>
        </div>

        <div class="slot-card free" id="card-2">
          <div class="slot-top">
            <span class="slot-num">SLOT — 02</span>
            <span class="slot-badge badge-free" id="badge-2">FREE</span>
          </div>
          <div class="slot-icon">
            <svg class="parking-graphic" viewBox="0 0 200 72" xmlns="http://www.w3.org/2000/svg">
              <rect x="4" y="4" width="192" height="64" rx="4" fill="none" stroke="rgba(255,255,255,0.06)" stroke-width="1"/>
              <g id="car-2" opacity="0" style="transition:opacity 0.4s">
                <rect x="40" y="20" width="120" height="32" rx="6" fill="rgba(239,68,68,0.2)" stroke="rgba(239,68,68,0.4)" stroke-width="1"/>
                <rect x="56" y="14" width="88" height="22" rx="4" fill="rgba(239,68,68,0.15)" stroke="rgba(239,68,68,0.3)" stroke-width="1"/>
                <circle cx="62" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <circle cx="138" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <rect x="48" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
                <rect x="132" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
              </g>
              <text id="plabel-2" x="100" y="40" text-anchor="middle" dominant-baseline="middle" fill="rgba(34,197,94,0.2)" font-size="40" font-weight="700" font-family="monospace">P</text>
              <circle cx="16" cy="36" r="4" fill="rgba(255,255,255,0.1)"/>
            </svg>
          </div>
          <div class="slot-label free" id="label-2">AVAILABLE</div>
          <div class="slot-detail" id="detail-2"><span class="ir-dot" id="ird-2"></span>IR: CLEAR</div>
        </div>

        <div class="slot-card free" id="card-3">
          <div class="slot-top">
            <span class="slot-num">SLOT — 03</span>
            <span class="slot-badge badge-free" id="badge-3">FREE</span>
          </div>
          <div class="slot-icon">
            <svg class="parking-graphic" viewBox="0 0 200 72" xmlns="http://www.w3.org/2000/svg">
              <rect x="4" y="4" width="192" height="64" rx="4" fill="none" stroke="rgba(255,255,255,0.06)" stroke-width="1"/>
              <g id="car-3" opacity="0" style="transition:opacity 0.4s">
                <rect x="40" y="20" width="120" height="32" rx="6" fill="rgba(239,68,68,0.2)" stroke="rgba(239,68,68,0.4)" stroke-width="1"/>
                <rect x="56" y="14" width="88" height="22" rx="4" fill="rgba(239,68,68,0.15)" stroke="rgba(239,68,68,0.3)" stroke-width="1"/>
                <circle cx="62" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <circle cx="138" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <rect x="48" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
                <rect x="132" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
              </g>
              <text id="plabel-3" x="100" y="40" text-anchor="middle" dominant-baseline="middle" fill="rgba(34,197,94,0.2)" font-size="40" font-weight="700" font-family="monospace">P</text>
              <circle cx="16" cy="36" r="4" fill="rgba(255,255,255,0.1)"/>
            </svg>
          </div>
          <div class="slot-label free" id="label-3">AVAILABLE</div>
          <div class="slot-detail" id="detail-3"><span class="ir-dot" id="ird-3"></span>IR: CLEAR</div>
        </div>

        <div class="slot-card free" id="card-4">
          <div class="slot-top">
            <span class="slot-num">SLOT — 04</span>
            <span class="slot-badge badge-free" id="badge-4">FREE</span>
          </div>
          <div class="slot-icon">
            <svg class="parking-graphic" viewBox="0 0 200 72" xmlns="http://www.w3.org/2000/svg">
              <rect x="4" y="4" width="192" height="64" rx="4" fill="none" stroke="rgba(255,255,255,0.06)" stroke-width="1"/>
              <g id="car-4" opacity="0" style="transition:opacity 0.4s">
                <rect x="40" y="20" width="120" height="32" rx="6" fill="rgba(239,68,68,0.2)" stroke="rgba(239,68,68,0.4)" stroke-width="1"/>
                <rect x="56" y="14" width="88" height="22" rx="4" fill="rgba(239,68,68,0.15)" stroke="rgba(239,68,68,0.3)" stroke-width="1"/>
                <circle cx="62" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <circle cx="138" cy="56" r="8" fill="rgba(239,68,68,0.3)" stroke="rgba(239,68,68,0.5)" stroke-width="1"/>
                <rect x="48" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
                <rect x="132" y="16" width="20" height="14" rx="2" fill="rgba(96,165,250,0.3)"/>
              </g>
              <text id="plabel-4" x="100" y="40" text-anchor="middle" dominant-baseline="middle" fill="rgba(34,197,94,0.2)" font-size="40" font-weight="700" font-family="monospace">P</text>
              <circle cx="16" cy="36" r="4" fill="rgba(255,255,255,0.1)"/>
            </svg>
          </div>
          <div class="slot-label free" id="label-4">AVAILABLE</div>
          <div class="slot-detail" id="detail-4"><span class="ir-dot" id="ird-4"></span>IR: CLEAR</div>
        </div>

      </div>

      <div class="panel" style="margin-top:14px;">
        <div class="section-title">Occupancy Rate</div>
        <div class="occ-bar-wrap">
          <div class="occ-bar-track">
            <div class="occ-bar-fill" id="occ-bar" style="width:0%"></div>
          </div>
          <div class="occ-bar-labels">
            <span>0%</span>
            <span id="occ-pct">0% occupied</span>
            <span>100%</span>
          </div>
        </div>
      </div>
    </div>

    <div class="sidebar">
      <div class="panel">
        <div class="section-title">Gate Control</div>
        <div class="gate-visual">
          <div class="gate-post"></div>
          <div class="gate-arm" id="gate-arm"><div class="gate-stripes"></div></div>
          <div class="gate-road"></div>
        </div>
        <div class="gate-status-row">
          <div class="gate-label closed" id="gate-label">CLOSED</div>
          <button class="gate-btn" id="gate-btn" onclick="manualGate()">OPEN</button>
        </div>
      </div>

      <div class="panel">
        <div class="section-title">IR Sensors</div>
        <div class="sensor-row">
          <div class="sensor-chip" id="entry-chip">
            <div class="sensor-chip-label">Entry IR</div>
            <div class="sensor-chip-value" id="entry-val">CLEAR</div>
          </div>
          <div class="sensor-chip" id="exit-chip">
            <div class="sensor-chip-label">Exit IR</div>
            <div class="sensor-chip-value" id="exit-val">CLEAR</div>
          </div>
        </div>
      </div>

      <div class="panel">
        <div class="section-header">
          <span class="section-title">Event Log</span>
          <span id="log-count" style="font-family:var(--mono);font-size:10px;color:var(--muted);">0 events</span>
        </div>
        <div class="log-scroll" id="log-scroll"></div>
      </div>
    </div>
  </div>
</div>

<footer>
  ESP32 SMARTPARK &nbsp;·&nbsp; ws://192.168.4.1:81 &nbsp;·&nbsp; <span id="uptime">00:00:00</span>
</footer>

<script>
const STATE = { slots:[{state:'FREE'},{state:'FREE'},{state:'FREE'},{state:'FREE'}], gate:'CLOSED', free:4, demo:true };
let logCount = 0;
const startTime = Date.now();

function addLog(msg, type='info') {
  const scroll = document.getElementById('log-scroll');
  const time = new Date().toTimeString().slice(0,8);
  const el = document.createElement('div');
  el.className = 'log-entry ' + type;
  el.innerHTML = `<span class="log-time">${time}</span><span class="log-msg">${msg}</span>`;
  scroll.prepend(el);
  if (scroll.children.length > 50) scroll.lastChild.remove();
  logCount++;
  document.getElementById('log-count').textContent = logCount + ' event' + (logCount!==1?'s':'');
}

function applySlotState(i, state) {
  const idx = i + 1;
  const card=document.getElementById('card-'+idx), badge=document.getElementById('badge-'+idx);
  const label=document.getElementById('label-'+idx), car=document.getElementById('car-'+idx);
  const pl=document.getElementById('plabel-'+idx), detail=document.getElementById('detail-'+idx);
  const prev = card.dataset.state;
  if (prev === state) return;
  card.dataset.state = state;
  card.className = 'slot-card'; badge.className = 'slot-badge'; label.className = 'slot-label';
  if (state === 'FREE') {
    card.classList.add('free'); badge.classList.add('badge-free'); badge.textContent='FREE';
    label.classList.add('free'); label.textContent='AVAILABLE'; car.style.opacity='0';
    pl.setAttribute('fill','rgba(34,197,94,0.2)');
    detail.innerHTML='<span class="ir-dot" id="ird-'+idx+'"></span>IR: CLEAR';
    if (prev) addLog('Slot '+idx+' is now free','ok');
  } else if (state === 'OCC') {
    card.classList.add('occ'); badge.classList.add('badge-occ'); badge.textContent='OCC';
    label.classList.add('occ'); label.textContent='OCCUPIED'; car.style.opacity='1';
    pl.setAttribute('fill','rgba(239,68,68,0.05)');
    detail.innerHTML='<span class="ir-dot active" id="ird-'+idx+'"></span>IR: BLOCKED';
    if (prev) addLog('Slot '+idx+' occupied','err');
  } else if (state === 'RES') {
    card.classList.add('res'); badge.classList.add('badge-res'); badge.textContent='RES';
    label.classList.add('res'); label.textContent='RESERVED'; car.style.opacity='0';
    pl.setAttribute('fill','rgba(249,115,22,0.2)');
    detail.innerHTML='<span class="ir-dot" id="ird-'+idx+'"></span>IR: RESERVED';
    if (prev) addLog('Slot '+idx+' reserved','warn');
  }
}

function applyGateState(g) {
  const arm=document.getElementById('gate-arm'), label=document.getElementById('gate-label');
  const sgate=document.getElementById('s-gate'), btn=document.getElementById('gate-btn');
  if (g==='OPEN') { arm.classList.add('open'); label.textContent='OPEN'; label.className='gate-label open'; sgate.textContent='OPEN'; btn.textContent='CLOSE'; }
  else { arm.classList.remove('open'); label.textContent='CLOSED'; label.className='gate-label closed'; sgate.textContent='CLOSED'; btn.textContent='OPEN'; }
}

function applyData(d) {
  const prevFree = STATE.free;
  d.slots.forEach((s,i)=>{ STATE.slots[i]=s; applySlotState(i,s.state); });
  STATE.gate=d.gate; applyGateState(d.gate);
  STATE.free=d.free;
  const occ=d.slots.filter(s=>s.state==='OCC').length;
  const res=d.slots.filter(s=>s.state==='RES').length;
  document.getElementById('s-free').textContent=d.free;
  document.getElementById('s-occ').textContent=occ;
  document.getElementById('s-res').textContent=res;
  const pct=Math.round((occ/4)*100);
  const bar=document.getElementById('occ-bar');
  bar.style.width=pct+'%';
  bar.className='occ-bar-fill'+(pct>=100?' full':pct>=50?' half':'');
  document.getElementById('occ-pct').textContent=pct+'% occupied';
  if (prevFree===0 && d.free>0) addLog('Slots available — gate ready','info');
}

function manualGate() {
  if (!STATE.demo && ws && ws.readyState===WebSocket.OPEN) {
    ws.send(JSON.stringify({cmd:'GATE_TOGGLE'})); return;
  }
  const newState=STATE.gate==='OPEN'?'CLOSED':'OPEN';
  applyData({...{slots:STATE.slots,gate:newState,free:STATE.free}});
  addLog('Manual gate '+newState.toLowerCase(),'info');
}

function updateClock() {
  document.getElementById('clock').textContent=new Date().toTimeString().slice(0,8);
  const e=Math.floor((Date.now()-startTime)/1000);
  document.getElementById('uptime').textContent=
    String(Math.floor(e/3600)).padStart(2,'0')+':'+
    String(Math.floor((e%3600)/60)).padStart(2,'0')+':'+
    String(e%60).padStart(2,'0');
}
setInterval(updateClock,1000); updateClock();

let ws;
function connectWS() {
  try {
    ws=new WebSocket('ws://'+window.location.hostname+':81/');
    ws.onopen=()=>{ document.getElementById('conn').classList.remove('offline'); document.getElementById('conn-text').textContent='LIVE CONNECTED'; document.getElementById('demo-notice').classList.add('hidden'); STATE.demo=false; stopDemo(); addLog('WebSocket connected','ok'); };
    ws.onclose=()=>{ document.getElementById('conn').classList.add('offline'); document.getElementById('conn-text').textContent='RECONNECTING...'; setTimeout(connectWS,2000); if(!STATE.demo)startDemo(); };
    ws.onerror=()=>{ ws.close(); };
    ws.onmessage=(e)=>{ try{applyData(JSON.parse(e.data));}catch(err){} };
  } catch(e){ startDemo(); }
}

let demoTimer, demoStep=0;
const scenarios=[
  {gate:'CLOSED',free:2,slots:[{state:'OCC'},{state:'FREE'},{state:'FREE'},{state:'OCC'}]},
  {gate:'OPEN',  free:2,slots:[{state:'OCC'},{state:'FREE'},{state:'FREE'},{state:'OCC'}]},
  {gate:'CLOSED',free:1,slots:[{state:'OCC'},{state:'OCC'},{state:'FREE'},{state:'OCC'}]},
  {gate:'OPEN',  free:1,slots:[{state:'OCC'},{state:'OCC'},{state:'FREE'},{state:'OCC'}]},
  {gate:'CLOSED',free:0,slots:[{state:'OCC'},{state:'OCC'},{state:'OCC'},{state:'OCC'}]},
  {gate:'OPEN',  free:1,slots:[{state:'OCC'},{state:'OCC'},{state:'FREE'},{state:'OCC'}]},
  {gate:'CLOSED',free:2,slots:[{state:'FREE'},{state:'OCC'},{state:'FREE'},{state:'OCC'}]},
  {gate:'OPEN',  free:3,slots:[{state:'FREE'},{state:'OCC'},{state:'FREE'},{state:'FREE'}]},
  {gate:'CLOSED',free:3,slots:[{state:'FREE'},{state:'OCC'},{state:'FREE'},{state:'FREE'}]},
  {gate:'CLOSED',free:2,slots:[{state:'FREE'},{state:'OCC'},{state:'RES'},{state:'FREE'}]},
];

function startDemo() {
  STATE.demo=true;
  document.getElementById('conn').classList.remove('offline');
  document.getElementById('conn-text').textContent='DEMO MODE';
  document.getElementById('demo-notice').classList.remove('hidden');
  addLog('Demo simulation started','warn');
  applyData({gate:'CLOSED',free:2,slots:[{state:'OCC'},{state:'FREE'},{state:'FREE'},{state:'OCC'}]});
  demoTimer=setInterval(()=>{
    const s=scenarios[demoStep%scenarios.length];
    applyData(s);
    if(s.gate==='OPEN'){const c=Math.random()>0.5?'entry':'exit';flashSensor(c);addLog(c.charAt(0).toUpperCase()+c.slice(1)+' IR triggered — gate opened','warn');}
    demoStep++;
  },3500);
}
function stopDemo(){ clearInterval(demoTimer); }
function flashSensor(id){
  const chip=document.getElementById(id+'-chip'),val=document.getElementById(id+'-val');
  chip.classList.add('triggered'); val.textContent='TRIGGERED';
  setTimeout(()=>{ chip.classList.remove('triggered'); val.textContent='CLEAR'; },2000);
}

addLog('Dashboard initialised — connecting to ESP32...','info');
connectWS();
setTimeout(()=>{ if(STATE.demo)startDemo(); },2000);
</script>
</body>
</html>
)rawliteral";
}

// ======================================================
// WEBSOCKET EVENT HANDLER
// ======================================================

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      String msg1 = buildStateJson();
      webSocket.sendTXT(num, msg1);
      break;
    }
    case WStype_TEXT: {
      String msg = String((char*)payload);
      if (msg.indexOf("GATE_TOGGLE") >= 0) {
        if (gateOpen) closeGate();
        else          openGate();
        broadcastState();
      }
      break;
    }
    default: break;
  }
}

// ======================================================
// WEB SERVER
// ======================================================

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void setupWebServer() {
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(1000);
  server.on("/", handleRoot);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);

  // EEPROM — load saved slot states
  EEPROM.begin(EEPROM_SIZE);
  loadSlotsFromEEPROM();

  // Slot IR pins
  for (int i = 0; i < 4; i++) pinMode(SLOT_PINS[i], INPUT_PULLUP);
  pinMode(ENTRY_IR_PIN, INPUT_PULLUP);
  pinMode(EXIT_IR_PIN, INPUT_PULLUP);

  // BUG FIX 1: Read actual IR states NOW and sync slotState[] accordingly.
  //
  // Why: lastSlotRead[] was always set to HIGH at compile time.  If the real
  // pin is also HIGH (no car), no edge is ever detected in the first scan, so
  // a stale EEPROM value of "1 = occupied" was never corrected.
  //
  // Now we read each pin directly and override whatever EEPROM said.
  // RESERVED slots (2) are trusted from EEPROM — only FREE/OCCUPIED are
  // set from the live IR reading on boot.
  for (int i = 0; i < 4; i++) {
    int reading = digitalRead(SLOT_PINS[i]);
    lastSlotRead[i] = reading;  // initialise edge-detection baseline

    // Only override FREE/OCCUPIED from the live IR reading.
    // Leave RESERVED as-is (it was set intentionally).
    if (slotState[i] != 2) {
      slotState[i] = (reading == LOW) ? 1 : 0;  // LOW=beam broken=occupied
    }
  }

  // Servo
  gateServo.attach(SERVO_PIN);
  gateServo.write(GATE_CLOSED_ANGLE);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, carChar);
  lcd.createChar(1, arrowChar);

  // Network
  setupWebServer();
  startupAnimation();

  updateLCD();
  broadcastState();

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  server.handleClient();
  webSocket.loop();

  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    handleSlotLogic();
    handleEntryExit();
    handleGateTimer();
  }

  if (millis() - lastLcdUpdate >= LCD_INTERVAL) {
    lastLcdUpdate = millis();
    updateLCD();
  }

  if (millis() - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = millis();
    broadcastState();
  }
}
