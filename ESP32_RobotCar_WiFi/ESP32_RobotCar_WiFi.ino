/*
 * ============================================================
 *  ESP32 WROOM-32 (38-Pin) — WiFi Grass Cutter Robot Controller
 *  Protocol : WebSockets (AsyncWebServer)
 *  Motors   : 2x DC via L298N — EN pins tied to 5V (full speed)
 *  Cutter   : 1x Relay on GPIO 32 (Active LOW)
 *  UI       : Grass cutter themed controller (http://192.168.4.1)
 * ============================================================
 *
 *  WIRING (L298N Motor Driver)
 *  ─────────────────────────────────────────────────────────
 *  L298N    →  ESP32
 *  IN1      →  GPIO 13  (direction, Motor A / Left)
 *  IN2      →  GPIO 12  (direction, Motor A / Left)
 *  IN3      →  GPIO 14  (direction, Motor B / Right)
 *  IN4      →  GPIO 27  (direction, Motor B / Right)
 *  ENA      →  5V  (always on — full speed)
 *  ENB      →  5V  (always on — full speed)
 *  GND      →  GND
 *
 *  WIRING (Relay — Cutter Motor)
 *  ─────────────────────────────────────────────────────────
 *  Relay IN →  GPIO 32  (Active LOW — LOW=ON, HIGH=OFF)
 *  Relay VCC → 5V
 *  Relay GND → GND
 *
 *  REQUIRED LIBRARIES (install via Arduino Library Manager)
 *  ─────────────────────────────────────────────────────────
 *  • ESPAsyncWebServer  (by lacamera / me-no-dev)
 *  • AsyncTCP           (by dvarrel / me-no-dev)
 *
 *  Board: "ESP32 Dev Module" in Arduino IDE
 * ============================================================
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ── WiFi Access Point credentials ──────────────────────────
const char* AP_SSID     = "GrassCutter";
const char* AP_PASSWORD = "cutgrass1";   // min 8 chars; use "" for open AP

// ── Motor A pins (Left Motor) ───────────────────────────────
#define MOTOR_A_IN1  13
#define MOTOR_A_IN2  12

// ── Motor B pins (Right Motor) ──────────────────────────────
#define MOTOR_B_IN3  14
#define MOTOR_B_IN4  27

// ── Cutter Relay pin (Active LOW) ───────────────────────────
#define RELAY_CUTTER 32

// ── State ────────────────────────────────────────────────────
char currentDir    = 'S';
bool cutterEnabled = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ════════════════════════════════════════════════════════════
//  Motor helpers
// ════════════════════════════════════════════════════════════
void motorA(int dir) {
  // dir: 1=forward, -1=backward, 0=stop
  digitalWrite(MOTOR_A_IN1, dir ==  1 ? HIGH : LOW);
  digitalWrite(MOTOR_A_IN2, dir == -1 ? HIGH : LOW);
}

void motorB(int dir) {
  digitalWrite(MOTOR_B_IN3, dir ==  1 ? HIGH : LOW);
  digitalWrite(MOTOR_B_IN4, dir == -1 ? HIGH : LOW);
}

void driveForward()  { motorA( 1); motorB( 1); }
void driveBackward() { motorA(-1); motorB(-1); }
void driveLeft()     { motorA(-1); motorB( 1); }
void driveRight()    { motorA( 1); motorB(-1); }
void driveStop()     { motorA( 0); motorB( 0); }

// ════════════════════════════════════════════════════════════
//  Cutter Relay helpers (Active LOW)
// ════════════════════════════════════════════════════════════
void cutterOn()  { digitalWrite(RELAY_CUTTER, LOW);  cutterEnabled = true;  }
void cutterOff() { digitalWrite(RELAY_CUTTER, HIGH); cutterEnabled = false; }

// ════════════════════════════════════════════════════════════
//  Broadcast current state JSON
// ════════════════════════════════════════════════════════════
void broadcastState() {
  String state = "{\"dir\":\"" + String(currentDir) +
                 "\",\"cutter\":" + (cutterEnabled ? "true" : "false") + "}";
  ws.textAll(state);
}

// ════════════════════════════════════════════════════════════
//  WebSocket command handler
// ════════════════════════════════════════════════════════════
void handleCommand(const String& cmd) {
  // Cutter toggle command
  if (cmd == "CUT_ON") {
    cutterOn();
    broadcastState();
    return;
  }
  if (cmd == "CUT_OFF") {
    cutterOff();
    broadcastState();
    return;
  }

  // Drive commands
  currentDir = cmd.charAt(0);
  switch (currentDir) {
    case 'F': driveForward();  break;
    case 'B': driveBackward(); break;
    case 'L': driveLeft();     break;
    case 'R': driveRight();    break;
    default:  driveStop(); currentDir = 'S'; break;
  }
  broadcastState();
}

void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected\n", client->id());
    // Send current state to new client
    String init = "{\"dir\":\"" + String(currentDir) +
                  "\",\"cutter\":" + (cutterEnabled ? "true" : "false") + "}";
    client->text(init);
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected — stopping\n", client->id());
    driveStop();
    cutterOff();
    currentDir = 'S';
  }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
      data[len] = '\0';
      String msg = String((char*)data);
      if (msg == "PING") {
        client->text("PONG");
        return;
      }
      handleCommand(msg);
    }
  }
}

// ════════════════════════════════════════════════════════════
//  Embedded HTML — Grass Cutter themed UI
// ════════════════════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Grass Cutter</title>
<link href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&family=Syne:wght@400;600;700;800&display=swap" rel="stylesheet">
<style>
  :root {
    --bg:        #0d1a0e;
    --surface:   #131f14;
    --surface2:  #1a2b1b;
    --border:    #1f3320;
    --text:      #e8f5e9;
    --muted:     #4a7a4d;
    --accent:    #4caf50;
    --accent-d:  #388e3c;
    --accent-l:  #1b3d1c;
    --amber:     #ffc107;
    --amber-l:   #3d2e00;
    --red:       #ef5350;
    --red-l:     #3d0f0f;
    --glow:      0 0 20px rgba(76,175,80,0.25);
    --glow-sm:   0 0 10px rgba(76,175,80,0.15);
    --radius:    14px;
  }

  *, *::before, *::after { box-sizing: border-box; margin:0; padding:0; }

  html, body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Syne', sans-serif;
    height: 100%;
    overflow: hidden;
    -webkit-tap-highlight-color: transparent;
    user-select: none;
  }

  /* Subtle grid texture overlay */
  body::before {
    content: '';
    position: fixed;
    inset: 0;
    background-image:
      linear-gradient(rgba(76,175,80,0.03) 1px, transparent 1px),
      linear-gradient(90deg, rgba(76,175,80,0.03) 1px, transparent 1px);
    background-size: 32px 32px;
    pointer-events: none;
    z-index: 0;
  }

  .app {
    position: relative;
    z-index: 1;
    display: flex;
    flex-direction: column;
    height: 100dvh;
    max-width: 420px;
    margin: 0 auto;
    padding: 18px 16px 20px;
    gap: 13px;
  }

  /* ── Header ── */
  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .header-left {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .logo-icon {
    width: 34px; height: 34px;
    background: var(--accent-l);
    border: 1px solid var(--border);
    border-radius: 10px;
    display: flex; align-items: center; justify-content: center;
    box-shadow: var(--glow-sm);
  }
  .logo-icon svg {
    width: 18px; height: 18px;
    stroke: var(--accent); fill: none;
    stroke-width: 2; stroke-linecap: round; stroke-linejoin: round;
  }
  .header-left h1 {
    font-size: 1.05rem;
    font-weight: 800;
    letter-spacing: -0.01em;
    color: var(--text);
    line-height: 1.1;
  }
  .header-left p {
    font-size: 0.62rem;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    margin-top: 1px;
  }
  .conn-badge {
    display: flex;
    align-items: center;
    gap: 5px;
    background: var(--red-l);
    color: var(--red);
    border: 1px solid rgba(239,83,80,0.2);
    font-size: 0.62rem;
    font-weight: 700;
    font-family: 'Space Mono', monospace;
    padding: 5px 10px;
    border-radius: 99px;
    letter-spacing: 0.05em;
    transition: all 0.3s;
  }
  .conn-badge.online {
    background: var(--accent-l);
    color: var(--accent);
    border-color: rgba(76,175,80,0.2);
    box-shadow: var(--glow-sm);
  }
  .conn-dot {
    width: 6px; height: 6px;
    border-radius: 50%;
    background: currentColor;
  }
  .conn-badge.online .conn-dot {
    animation: pulse 1.5s ease-in-out infinite;
  }
  @keyframes pulse {
    0%, 100% { opacity: 1; transform: scale(1); }
    50%       { opacity: 0.5; transform: scale(0.8); }
  }

  /* ── Status strip ── */
  .status-strip {
    display: grid;
    grid-template-columns: 1fr 1fr 1fr;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    overflow: hidden;
  }
  .stat {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    padding: 10px 8px;
  }
  .stat:not(:last-child) { border-right: 1px solid var(--border); }
  .stat-label {
    font-size: 0.55rem;
    text-transform: uppercase;
    letter-spacing: 0.12em;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
  }
  .stat-val {
    font-size: 0.9rem;
    font-weight: 700;
    font-family: 'Space Mono', monospace;
    color: var(--text);
  }
  .stat-val.active  { color: var(--accent); text-shadow: var(--glow-sm); }
  .stat-val.cutting { color: var(--amber); }

  /* ── Cutter toggle ── */
  .cutter-panel {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 13px 16px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    transition: border-color 0.3s, box-shadow 0.3s;
  }
  .cutter-panel.active {
    border-color: var(--amber);
    box-shadow: 0 0 18px rgba(255,193,7,0.2);
  }
  .cutter-info {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .cutter-icon {
    width: 36px; height: 36px;
    border-radius: 10px;
    background: var(--surface2);
    border: 1px solid var(--border);
    display: flex; align-items: center; justify-content: center;
    flex-shrink: 0;
    transition: all 0.3s;
  }
  .cutter-icon svg {
    width: 18px; height: 18px;
    stroke: var(--muted); fill: none;
    stroke-width: 2; stroke-linecap: round; stroke-linejoin: round;
    transition: stroke 0.3s;
  }
  .cutter-panel.active .cutter-icon {
    background: var(--amber-l);
    border-color: rgba(255,193,7,0.3);
  }
  .cutter-panel.active .cutter-icon svg {
    stroke: var(--amber);
  }
  .cutter-text h3 {
    font-size: 0.82rem;
    font-weight: 700;
    color: var(--text);
  }
  .cutter-text p {
    font-size: 0.62rem;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    margin-top: 1px;
  }
  .cutter-panel.active .cutter-text p {
    color: var(--amber);
  }

  /* Toggle switch */
  .toggle {
    position: relative;
    width: 48px; height: 26px;
    flex-shrink: 0;
  }
  .toggle input { opacity: 0; width: 0; height: 0; }
  .toggle-track {
    position: absolute;
    inset: 0;
    background: var(--surface2);
    border: 1px solid var(--border);
    border-radius: 99px;
    cursor: pointer;
    transition: background 0.25s, border-color 0.25s;
  }
  .toggle-track::after {
    content: '';
    position: absolute;
    top: 3px; left: 3px;
    width: 18px; height: 18px;
    background: var(--muted);
    border-radius: 50%;
    transition: transform 0.25s, background 0.25s;
  }
  .toggle input:checked + .toggle-track {
    background: var(--amber-l);
    border-color: var(--amber);
  }
  .toggle input:checked + .toggle-track::after {
    transform: translateX(22px);
    background: var(--amber);
    box-shadow: 0 0 8px rgba(255,193,7,0.5);
  }

  /* ── Middle row ── */
  .middle-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    flex: 1;
    min-height: 0;
  }

  /* ── D-Pad ── */
  .dpad {
    display: grid;
    grid-template-areas:
      ". up ."
      "left stop right"
      ". down .";
    gap: 8px;
    flex-shrink: 0;
  }

  .btn {
    width: 66px; height: 66px;
    border-radius: 14px;
    border: 1px solid var(--border);
    background: var(--surface);
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    font-size: 0.55rem;
    font-weight: 700;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    cursor: pointer;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 6px;
    transition: all 0.1s;
    -webkit-tap-highlight-color: transparent;
  }
  .btn svg {
    width: 20px; height: 20px;
    stroke: currentColor; fill: none;
    stroke-width: 2.2; stroke-linecap: round; stroke-linejoin: round;
  }
  .btn:active, .btn.active {
    background: var(--accent-l);
    border-color: var(--accent);
    color: var(--accent);
    box-shadow: 0 0 14px rgba(76,175,80,0.3), inset 0 0 10px rgba(76,175,80,0.05);
    transform: scale(0.95);
  }

  .btn-stop {
    grid-area: stop;
  }
  .btn-stop:active, .btn-stop.active {
    background: var(--red-l);
    border-color: var(--red);
    color: var(--red);
    box-shadow: 0 0 14px rgba(239,83,80,0.3);
  }

  [data-cmd="F"] { grid-area: up; }
  [data-cmd="B"] { grid-area: down; }
  [data-cmd="L"] { grid-area: left; }
  [data-cmd="R"] { grid-area: right; }

  /* ── Sidebar ── */
  .sidebar {
    display: flex;
    flex-direction: column;
    gap: 8px;
    flex: 1;
    min-width: 0;
  }

  .info-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 10px 12px;
  }
  .ic-label {
    font-size: 0.55rem;
    text-transform: uppercase;
    letter-spacing: 0.12em;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
    margin-bottom: 4px;
  }
  .ic-val {
    font-family: 'Space Mono', monospace;
    font-size: 0.75rem;
    font-weight: 700;
    color: var(--text);
    word-break: break-all;
  }

  /* Motor bars */
  .motors-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 10px 12px;
    flex: 1;
  }
  .motor-rows { display: flex; flex-direction: column; gap: 10px; margin-top: 8px; }
  .motor-row  { display: flex; align-items: center; gap: 8px; }
  .motor-tag  {
    font-family: 'Space Mono', monospace;
    font-size: 0.58rem;
    color: var(--muted);
    width: 18px;
    flex-shrink: 0;
  }
  .motor-track {
    flex: 1; height: 5px;
    background: var(--surface2);
    border-radius: 99px;
    overflow: hidden;
    border: 1px solid var(--border);
  }
  .motor-fill {
    height: 100%;
    width: 0%;
    background: var(--accent);
    border-radius: 99px;
    transition: width 0.15s ease;
    box-shadow: 0 0 8px rgba(76,175,80,0.6);
  }
  .motor-fill.rev {
    background: #ff7043;
    box-shadow: 0 0 8px rgba(255,112,67,0.6);
  }

  /* Cutter RPM indicator */
  .cutter-bar-card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 10px 12px;
    transition: border-color 0.3s;
  }
  .cutter-bar-card.active { border-color: rgba(255,193,7,0.3); }
  .cutter-track {
    height: 5px;
    background: var(--surface2);
    border-radius: 99px;
    overflow: hidden;
    border: 1px solid var(--border);
    margin-top: 8px;
  }
  .cutter-fill {
    height: 100%;
    width: 0%;
    background: var(--amber);
    border-radius: 99px;
    box-shadow: 0 0 8px rgba(255,193,7,0.6);
    transition: width 1.5s ease;
  }

  /* ── Keyboard hint ── */
  .kbd-hint {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 5px;
    font-size: 0.6rem;
    color: var(--muted);
    font-family: 'Space Mono', monospace;
  }
  .kbd {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 2px 5px;
    font-family: 'Space Mono', monospace;
    font-size: 0.55rem;
    color: var(--muted);
  }
</style>
</head>
<body>
<div class="app">

  <!-- Header -->
  <div class="header">
    <div class="header-left">
      <div class="logo-icon">
        <!-- Lawn mower / blade icon -->
        <svg viewBox="0 0 24 24">
          <circle cx="12" cy="12" r="3"/>
          <path d="M12 2v4M12 18v4M2 12h4M18 12h4M5.64 5.64l2.83 2.83M15.53 15.53l2.83 2.83M5.64 18.36l2.83-2.83M15.53 8.47l2.83-2.83"/>
        </svg>
      </div>
      <div>
        <h1>Grass Cutter</h1>
        <p id="ipLabel">192.168.4.1</p>
      </div>
    </div>
    <div class="conn-badge" id="connBadge">
      <div class="conn-dot"></div>
      <span id="connLabel">OFFLINE</span>
    </div>
  </div>

  <!-- Status strip -->
  <div class="status-strip">
    <div class="stat">
      <div class="stat-label">Drive</div>
      <div class="stat-val" id="statDir">—</div>
    </div>
    <div class="stat">
      <div class="stat-label">Blade</div>
      <div class="stat-val" id="statCutter">OFF</div>
    </div>
    <div class="stat">
      <div class="stat-label">Ping</div>
      <div class="stat-val" id="statPing">—</div>
    </div>
  </div>

  <!-- Cutter blade toggle -->
  <div class="cutter-panel" id="cutterPanel">
    <div class="cutter-info">
      <div class="cutter-icon">
        <svg viewBox="0 0 24 24">
          <circle cx="12" cy="12" r="3"/>
          <path d="M12 2v4M12 18v4M2 12h4M18 12h4M5.64 5.64l2.83 2.83M15.53 15.53l2.83 2.83M5.64 18.36l2.83-2.83M15.53 8.47l2.83-2.83"/>
        </svg>
      </div>
      <div class="cutter-text">
        <h3>Cutter Blade</h3>
        <p id="cutterSubtext">Relay OFF — GPIO 32</p>
      </div>
    </div>
    <label class="toggle">
      <input type="checkbox" id="cutterToggle" onchange="toggleCutter(this.checked)">
      <div class="toggle-track"></div>
    </label>
  </div>

  <!-- Middle: D-pad + sidebar -->
  <div class="middle-row">

    <!-- D-Pad -->
    <div class="dpad">
      <button class="btn" data-cmd="F" id="btnF"
              onpointerdown="press('F')" onpointerup="release()" onpointerleave="release()">
        <svg viewBox="0 0 24 24"><line x1="12" y1="19" x2="12" y2="5"/><polyline points="5 12 12 5 19 12"/></svg>
        FWD
      </button>
      <button class="btn" data-cmd="L" id="btnL"
              onpointerdown="press('L')" onpointerup="release()" onpointerleave="release()">
        <svg viewBox="0 0 24 24"><line x1="19" y1="12" x2="5" y2="12"/><polyline points="12 5 5 12 12 19"/></svg>
        LEFT
      </button>
      <button class="btn btn-stop" data-cmd="S" id="btnS"
              onpointerdown="press('S')" onpointerup="noop()">
        <svg viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/></svg>
        STOP
      </button>
      <button class="btn" data-cmd="R" id="btnR"
              onpointerdown="press('R')" onpointerup="release()" onpointerleave="release()">
        <svg viewBox="0 0 24 24"><line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/></svg>
        RIGHT
      </button>
      <button class="btn" data-cmd="B" id="btnB"
              onpointerdown="press('B')" onpointerup="release()" onpointerleave="release()">
        <svg viewBox="0 0 24 24"><line x1="12" y1="5" x2="12" y2="19"/><polyline points="19 12 12 19 5 12"/></svg>
        REV
      </button>
    </div>

    <!-- Sidebar -->
    <div class="sidebar">
      <div class="info-card">
        <div class="ic-label">IP Address</div>
        <div class="ic-val">192.168.4.1</div>
      </div>
      <div class="motors-card">
        <div class="ic-label">Drive Motors</div>
        <div class="motor-rows">
          <div class="motor-row">
            <div class="motor-tag">L</div>
            <div class="motor-track"><div class="motor-fill" id="barL"></div></div>
          </div>
          <div class="motor-row">
            <div class="motor-tag">R</div>
            <div class="motor-track"><div class="motor-fill" id="barR"></div></div>
          </div>
        </div>
      </div>
      <div class="cutter-bar-card" id="cutterBarCard">
        <div class="ic-label">Blade Speed</div>
        <div class="cutter-track">
          <div class="cutter-fill" id="cutterBar"></div>
        </div>
      </div>
    </div>
  </div>

  <!-- Keyboard hint -->
  <div class="kbd-hint">
    <span class="kbd">W</span><span class="kbd">A</span><span class="kbd">S</span><span class="kbd">D</span>
    &nbsp;·&nbsp; hold to drive &nbsp;·&nbsp;
    <span class="kbd">C</span> blade
  </div>

</div>

<script>
  const WSURL = `ws://${location.hostname}/ws`;
  let ws, pingTimer, pingStart;
  let currentCmd    = 'S';
  let cutterState   = false;

  // ── WebSocket ──────────────────────────────────────────
  function connect() {
    ws = new WebSocket(WSURL);
    ws.onopen = () => {
      setConnected(true);
      pingLoop();
    };
    ws.onclose = () => {
      setConnected(false);
      clearInterval(pingTimer);
      setTimeout(connect, 3000);
    };
    ws.onerror = () => ws.close();
    ws.onmessage = e => {
      if (e.data === 'PONG') {
        document.getElementById('statPing').textContent = (Date.now() - pingStart) + 'ms';
        return;
      }
      try { applyState(JSON.parse(e.data)); } catch(_) {}
    };
  }

  function pingLoop() {
    pingTimer = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        pingStart = Date.now();
        ws.send('PING');
      }
    }, 2000);
  }

  function send(msg) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(msg);
  }

  // ── Drive control ──────────────────────────────────────
  function press(cmd) {
    if (currentCmd === cmd) return;
    currentCmd = cmd;
    document.querySelectorAll('.btn').forEach(b => b.classList.remove('active'));
    document.getElementById('btn' + cmd)?.classList.add('active');
    send(cmd);
  }

  function release() {
    if (currentCmd === 'S') return;
    press('S');
  }

  function noop() {}

  // ── Cutter toggle ──────────────────────────────────────
  function toggleCutter(on) {
    send(on ? 'CUT_ON' : 'CUT_OFF');
  }

  // ── State / UI ─────────────────────────────────────────
  const DIR_LABEL = { F:'FWD', B:'REV', L:'LEFT', R:'RIGHT', S:'—' };

  function applyState(d) {
    const dir    = d.dir    || 'S';
    const cutter = d.cutter || false;
    const moving = dir !== 'S';

    // Drive status
    const dirEl = document.getElementById('statDir');
    dirEl.textContent = DIR_LABEL[dir] || dir;
    dirEl.className   = 'stat-val' + (moving ? ' active' : '');

    // Motor bars
    const lFwd = dir === 'F' || dir === 'R';
    const lRev = dir === 'B' || dir === 'L';
    const rFwd = dir === 'F' || dir === 'L';
    const rRev = dir === 'B' || dir === 'R';
    setBar('barL', lFwd || lRev, lRev);
    setBar('barR', rFwd || rRev, rRev);

    // Cutter state
    cutterState = cutter;
    const cutterEl    = document.getElementById('statCutter');
    cutterEl.textContent = cutter ? 'ON' : 'OFF';
    cutterEl.className   = 'stat-val' + (cutter ? ' cutting' : '');

    document.getElementById('cutterPanel').classList.toggle('active', cutter);
    document.getElementById('cutterBarCard').classList.toggle('active', cutter);
    document.getElementById('cutterSubtext').textContent =
      cutter ? 'Relay ON — Cutting' : 'Relay OFF — GPIO 32';
    document.getElementById('cutterBar').style.width = cutter ? '100%' : '0%';

    // Sync toggle UI (without re-triggering onchange)
    const tog = document.getElementById('cutterToggle');
    if (tog.checked !== cutter) tog.checked = cutter;
  }

  function setBar(id, on, rev) {
    const el = document.getElementById(id);
    el.style.width  = on ? '100%' : '0%';
    el.className    = 'motor-fill' + (rev ? ' rev' : '');
  }

  function setConnected(ok) {
    const badge = document.getElementById('connBadge');
    badge.className = 'conn-badge' + (ok ? ' online' : '');
    document.getElementById('connLabel').textContent = ok ? 'ONLINE' : 'OFFLINE';
    if (!ok) {
      document.getElementById('statPing').textContent = '—';
      applyState({ dir: 'S', cutter: false });
    }
  }

  // ── Keyboard ───────────────────────────────────────────
  const keyMap = {
    ArrowUp:'F', ArrowDown:'B', ArrowLeft:'L', ArrowRight:'R',
    w:'F', s:'B', a:'L', d:'R', ' ':'S'
  };
  document.addEventListener('keydown', e => {
    if (e.key === 'c' || e.key === 'C') {
      const tog = document.getElementById('cutterToggle');
      tog.checked = !tog.checked;
      toggleCutter(tog.checked);
      return;
    }
    if (keyMap[e.key]) { e.preventDefault(); press(keyMap[e.key]); }
  });
  document.addEventListener('keyup', e => {
    const mapped = keyMap[e.key];
    if (mapped && mapped !== 'S' && currentCmd === mapped) release();
  });

  connect();
</script>
</body>
</html>
)rawliteral";

// ════════════════════════════════════════════════════════════
//  Setup
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] ESP32 Grass Cutter Robot");

  // Motor GPIO
  pinMode(MOTOR_A_IN1, OUTPUT); pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN3, OUTPUT); pinMode(MOTOR_B_IN4, OUTPUT);
  driveStop();

  // Relay GPIO — Active LOW, so initialise HIGH (relay OFF)
  pinMode(RELAY_CUTTER, OUTPUT);
  digitalWrite(RELAY_CUTTER, HIGH);   // ensure cutter is OFF on boot

  // Start Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : nullptr);
  Serial.printf("[WiFi] AP: %s  IP: %s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // HTTP root
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  // Optional REST fallback — GET /cmd?v=F|B|L|R|S|CUT_ON|CUT_OFF
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("v")) {
      handleCommand(req->getParam("v")->value());
      req->send(200, "text/plain", "OK");
    } else {
      req->send(400, "text/plain", "Missing param: v");
    }
  });

  server.begin();
  Serial.println("[HTTP] Server started — open http://192.168.4.1");
}

// ════════════════════════════════════════════════════════════
//  Loop
// ════════════════════════════════════════════════════════════
void loop() {
  ws.cleanupClients();
  delay(50);
}