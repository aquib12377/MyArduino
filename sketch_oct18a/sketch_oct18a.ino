/*
  ESP32 + DHT11 Dashboard (WebServer + WebSocketsServer + I2C LCD)
  - HTTP served by synchronous WebServer (port 80)
  - Realtime push via WebSocketsServer (port 81)
  - Modern, self-contained UI (no external assets)
  - I2C LCD shows WiFi/IP, then rotates: Temp/Hum  ➜  Heat/Uptime
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- WiFi ----------
const char* WIFI_SSID     = "MyProject";
const char* WIFI_PASSWORD = "12345678";

// ---------- DHT ----------
#define DHT_PIN  4
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ---------- Servers ----------
WebServer server(80);
WebSocketsServer ws(81);  // WebSocket on a separate port

// ---------- Timing ----------
const uint32_t SAMPLE_MS = 2000;
uint32_t lastSample = 0;

// ---------- LCD (I2C) ----------
#define LCD_ADDR  0x27   // Try 0x27 first; if blank, try 0x3F
#define LCD_COLS  16     // Set 20 for a 20x4
#define LCD_ROWS  2
#define I2C_SDA   21
#define I2C_SCL   22

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
bool lcdOk = false;

// LCD page rotation
uint32_t lastLcdFlip = 0;
const uint32_t LCD_FLIP_MS = 3000;  // change every 3s
uint8_t lcdPage = 0;

// Track latest values for LCD
float lastT = NAN, lastH = NAN, lastHI = NAN;
uint32_t bootMillis;

// WebSocket client count (for LCD if desired)
volatile uint16_t wsClientCount = 0;

// ---------- HTML (inline) ----------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 • DHT11 Dashboard</title>
<style>
:root{--bg:#0b1220;--card:#121a2a;--muted:#7a8aaa;--text:#e6edf3;--accent:#4ea1ff;--ok:#1dc773;--warn:#ffb020;--bad:#ff5470;--ring:#22314d;--grid:#18233a}
*{box-sizing:border-box}html,body{height:100%}
body{margin:0;background:radial-gradient(1000px 600px at 10% -5%,#0f1930 0%,#0b1220 50%),linear-gradient(160deg,#0b1220 0%,#0e1526 80%);color:var(--text);font:14px/1.45 system-ui,Segoe UI,Roboto,Arial,sans-serif}
.container{max-width:1100px;margin:24px auto;padding:0 16px}
.header{display:flex;gap:12px;align-items:center;justify-content:space-between;margin-bottom:18px}
.brand{display:flex;gap:12px;align-items:center}
.logo{width:34px;height:34px;border-radius:10px;background:linear-gradient(135deg,#4ea1ff,#7c5cff);box-shadow:0 0 0 6px #1a2540}
.title{font-weight:700;font-size:20px;letter-spacing:.3px}
.badges{display:flex;gap:8px;flex-wrap:wrap}
.badge{padding:6px 10px;border-radius:999px;background:var(--ring);color:var(--muted);font-size:12px}
.badge.ok{background:rgba(29,199,115,.12);color:#9ef8c0;border:1px solid rgba(29,199,115,.25)}
.badge.warn{background:rgba(255,176,32,.12);color:#ffe3ad;border:1px solid rgba(255,176,32,.25)}
.badge.bad{background:rgba(255,84,112,.12);color:#ffc2cd;border:1px solid rgba(255,84,112,.25)}
.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:14px}
.card{grid-column:span 4;background:var(--card);border:1px solid rgba(78,161,255,.07);border-radius:18px;padding:16px;box-shadow:0 8px 24px rgba(0,0,0,.25), inset 0 1px 0 rgba(255,255,255,.02)}
.card.wide{grid-column:span 12}
.card h3{margin:0 0 4px 0;font-size:13px;color:var(--muted);font-weight:600;letter-spacing:.3px}
.kpi{display:flex;align-items:flex-end;gap:10px}
.kpi .value{font-size:36px;font-weight:800;letter-spacing:.5px}
.kpi .unit{opacity:.8;margin-bottom:6px}
.hr{height:1px;background:linear-gradient(90deg,transparent,var(--ring),transparent);margin:10px 0 12px}
.gauge{position:relative;height:10px;border-radius:999px;background:var(--grid);overflow:hidden;outline:1px solid rgba(255,255,255,.04)}
.gauge > i{display:block;height:100%;width:0;background:linear-gradient(90deg,#4ea1ff,#7c5cff)}
.chart-wrap{position:relative;background:var(--card);border-radius:14px;padding:8px;border:1px solid rgba(78,161,255,.07)}
canvas{display:block;width:100%;height:200px;background:linear-gradient(180deg,rgba(255,255,255,.02),transparent 40%),linear-gradient(0deg,transparent 24px,rgba(255,255,255,.04) 24px) repeat-y;background-size:100% 40px}
.footer{margin-top:16px;color:var(--muted);font-size:12px;text-align:right}
a{color:var(--accent);text-decoration:none}
@media (max-width:900px){.card{grid-column:span 6}.card.wide{grid-column:span 12}}
@media (max-width:620px){.card{grid-column:span 12}}
.pill{display:inline-flex;align-items:center;gap:6px;padding:6px 10px;border-radius:999px;background:var(--ring);color:var(--muted);font-size:12px}
.dot{width:8px;height:8px;border-radius:50%}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}
.small{font-size:12px;color:var(--muted)}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="brand">
      <div class="logo"></div>
      <div class="title">ESP32 • DHT11 Dashboard</div>
    </div>
    <div class="badges">
      <div id="wifiBadge" class="badge">Wi-Fi: —</div>
      <div id="wsBadge" class="badge bad">WS: Disconnected</div>
      <div id="clientsBadge" class="badge">Clients: 0</div>
      <div id="uptimeBadge" class="badge">Uptime: 0s</div>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <h3>Temperature</h3>
      <div class="kpi"><div class="value" id="temp">—</div><div class="unit">°C</div></div>
      <div class="hr"></div>
      <div class="gauge"><i id="tempGauge"></i></div>
      <div class="small">Comfort 20–26°C</div>
    </div>
    <div class="card">
      <h3>Humidity</h3>
      <div class="kpi"><div class="value" id="hum">—</div><div class="unit">%</div></div>
      <div class="hr"></div>
      <div class="gauge"><i id="humGauge"></i></div>
      <div class="small">Comfort 30–60%</div>
    </div>
    <div class="card">
      <h3>Heat Index</h3>
      <div class="kpi"><div class="value" id="heat">—</div><div class="unit">°C</div></div>
      <div class="hr"></div>
      <div class="gauge"><i id="heatGauge"></i></div>
      <div class="small" id="heatNote">—</div>
    </div>
    <div class="card wide">
      <h3>Live Trend (last 60 samples)</h3>
      <div class="chart-wrap"><canvas id="chart" width="1000" height="240"></canvas></div>
      <div class="footer"><span class="pill"><span class="dot ok"></span> Temp</span> &nbsp; <span class="pill"><span class="dot bad"></span> Hum</span></div>
    </div>
  </div>

  <div class="footer small"><span id="ip"></span> • <span id="ts"></span></div>
</div>

<script>
(() => {
  const $ = id => document.getElementById(id);
  const el = {
    wifi: $("wifiBadge"), ws: $("wsBadge"), clients: $("clientsBadge"), uptime: $("uptimeBadge"),
    temp: $("temp"), hum: $("hum"), heat: $("heat"), tempG: $("tempGauge"), humG: $("humGauge"), heatG: $("heatGauge"),
    heatNote: $("heatNote"), ip: $("ip"), ts: $("ts")
  };

  const store = { t: [], h: [], ts: [], maxPoints: 60, startTime: Date.now() };

  // WebSocket on port 81 (required by WebSocketsServer)
  let ws, retry = 0;
  function connectWS(){
    const proto = location.protocol === "https:" ? "wss" : "ws";
    ws = new WebSocket(`${proto}://${location.hostname}:81/`);
    ws.onopen = () => { retry = 0; el.ws.textContent = "WS: Connected"; el.ws.className = "badge ok"; };
    ws.onclose = () => { el.ws.textContent = "WS: Disconnected"; el.ws.className = "badge bad"; setTimeout(connectWS, Math.min(10000, 500 * ++retry)); };
    ws.onerror = () => ws.close();
    ws.onmessage = ev => {
      try{
        const d = JSON.parse(ev.data);
        if (d.type === "meta"){
          if (d.clients !== undefined) el.clients.textContent = "Clients: " + d.clients;
          if (d.ip) el.ip.textContent = "ESP IP: " + d.ip;
          return;
        }
        const { temp, hum, heat, ts } = d;
        updateKPI(temp, hum, heat, ts);
      }catch(_){}
    };
  }

  function updateKPI(temp, hum, heat, ts){
    if (typeof temp !== "number" || typeof hum !== "number") return;
    el.temp.textContent = temp.toFixed(1);
    el.hum.textContent  = hum.toFixed(1);
    el.heat.textContent = (typeof heat==="number"?heat.toFixed(1):"—");

    el.heatNote.textContent = (heat>=32) ? "Caution: High heat index" : (heat>=27) ? "Warm" : "Comfortable";

    const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
    el.tempG.style.width = clamp((temp/40)*100,0,100)+"%";
    el.humG.style.width  = clamp(hum,0,100)+"%";
    el.heatG.style.width = clamp(((heat||0)/50)*100,0,100)+"%";

    const t = Date.now();
    store.t.push(temp); store.h.push(hum); store.ts.push(t);
    if (store.t.length>store.maxPoints){ store.t.shift(); store.h.shift(); store.ts.shift(); }
    drawChart();

    const dt = new Date(ts || t);
    el.ts.textContent = "Last update: " + dt.toLocaleTimeString();
  }

  const cvs = document.getElementById("chart"), ctx = cvs.getContext("2d");
  function drawChart(){
    const W=cvs.width,H=cvs.height; ctx.clearRect(0,0,W,H);
    ctx.globalAlpha = 0.25; ctx.lineWidth=1;
    for(let y=H-40;y>0;y-=40){ ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke(); }
    ctx.globalAlpha = 1;
    if (store.t.length<2) return;
    const pad=14,n=store.t.length,xStep=(W-pad*2)/(n-1);
    const map=(v,min,max)=>H-pad-((v-min)/(max-min))*(H-pad*2);

    function line(data,min,max,stroke){
      ctx.beginPath(); ctx.lineWidth=2; ctx.strokeStyle=stroke;
      for(let i=0;i<n;i++){ const x=pad+i*xStep, y=map(data[i],min,max); if(i===0)ctx.moveTo(x,y); else ctx.lineTo(x,y); }
      ctx.stroke();
    }
    line(store.t,0,45,"#7ae2a2");   // temp
    line(store.h,0,100,"#ff9db3");  // hum
  }

  // Uptime badge
  setInterval(() => {
    const s = Math.floor((Date.now()-store.startTime)/1000);
    const hh=Math.floor(s/3600), mm=Math.floor((s%3600)/60), ss=s%60;
    el.uptime.textContent = `Uptime: ${hh}h ${mm}m ${ss}s`;
  }, 1000);

  el.wifi.textContent = navigator.onLine ? "Wi-Fi: Online" : "Wi-Fi: Offline";
  connectWS();
})();
</script>
</body></html>
)HTML";

// ---------- Utility ----------
String ipToString(const IPAddress& ip){
  char buf[32];
  snprintf(buf,sizeof(buf),"%u.%u.%u.%u", ip[0],ip[1],ip[2],ip[3]);
  return String(buf);
}

float heatIndexC(float tC, float rh){
  float T = tC * 1.8 + 32.0, R = rh;
  float HI = -42.379 + 2.04901523*T + 10.14333127*R
             - 0.22475541*T*R - 0.00683783*T*T - 0.05481717*R*R
             + 0.00122874*T*T*R + 0.00085282*T*R*R - 0.00000199*T*T*R*R;
  return (HI - 32.0) / 1.8;
}

// ---------- WebSocket events ----------
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  switch(type){
    case WStype_CONNECTED: {
      wsClientCount = ws.connectedClients();
      String meta = String("{\"type\":\"meta\",\"clients\":") + wsClientCount
                  + ",\"ip\":\"" + ipToString(WiFi.localIP()) + "\"}";
      ws.sendTXT(num, meta);
      ws.broadcastTXT(meta);
    } break;

    case WStype_DISCONNECTED: {
      wsClientCount = ws.connectedClients();
      String metaAll = String("{\"type\":\"meta\",\"clients\":") + wsClientCount
                     + ",\"ip\":\"" + ipToString(WiFi.localIP()) + "\"}";
      ws.broadcastTXT(metaAll);
    } break;

    case WStype_TEXT:
      // (Optional) handle inbound text
      break;

    default: break;
  }
}

// ---------- LCD helpers ----------
void lcdPrintCentered(uint8_t row, const String& s) {
  if (!lcdOk) return;
  uint8_t len = s.length();
  uint8_t start = 0;
  if (LCD_COLS > len) start = (LCD_COLS - len) / 2;
  lcd.setCursor(start, row);
  lcd.print(s);
}

String formatUptime(uint32_t ms) {
  uint32_t s = ms / 1000;
  uint16_t hh = s / 3600;
  uint8_t mm = (s % 3600) / 60;
  uint8_t ss = s % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hh, mm, ss);
  return String(buf);
}

void showBootInfoOnLCD() {
  if (!lcdOk) return;
  lcd.clear();
  lcdPrintCentered(0, "WiFi Connecting");
  lcdPrintCentered(1, WIFI_SSID);
}

void showIPOnLCD() {
  if (!lcdOk) return;
  lcd.clear();
  lcdPrintCentered(0, "ESP32 Online");
  String ip = "IP: " + WiFi.localIP().toString();
  lcdPrintCentered(1, ip);
}

// Rotate two info pages on LCD
void updateLCD() {
  if (!lcdOk) return;

  // Change page every LCD_FLIP_MS
  if (millis() - lastLcdFlip < LCD_FLIP_MS) return;
  lastLcdFlip = millis();
  lcdPage = (lcdPage + 1) % 2;

  lcd.clear();
  if (lcdPage == 0) {
    // Page 1: Temp & Hum
    // Example: "T:25.1C  H:48%"
    char line1[21], line2[21];
    if (isnan(lastT) || isnan(lastH)) {
      snprintf(line1, sizeof(line1), "Reading sensors");
      snprintf(line2, sizeof(line2),  "Please wait...");
    } else {
      snprintf(line1, sizeof(line1), "T:%5.1fC  H:%4.1f%%", lastT, lastH);
      snprintf(line2, sizeof(line2), "Clients:%u", wsClientCount);
    }
    lcdPrintCentered(0, String(line1));
    lcdPrintCentered(1, String(line2));
  } else {
    // Page 2: Heat Index & Uptime
    char line1[21], line2[21];
    if (isnan(lastHI)) {
      snprintf(line1, sizeof(line1), "HeatIdx:  --.-C");
    } else {
      snprintf(line1, sizeof(line1), "HeatIdx:%6.1fC", lastHI);
    }
    String up = "Up " + formatUptime(millis() - bootMillis);
    snprintf(line2, sizeof(line2), "%s", up.c_str());
    lcdPrintCentered(0, String(line1));
    lcdPrintCentered(1, String(line2));
  }
}

void setup(){
  Serial.begin(115200);
  delay(200);

  // I2C + LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(50);
  
    // Many libs don't return a status; attempt init sequence anyway
    lcd.begin();
    lcd.backlight();
    lcdOk = true; // assume OK if no exception
  
  showBootInfoOnLCD();

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("\nConnecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED){
    delay(400); Serial.print(".");
  }
  Serial.printf("\nWiFi connected: %s  IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  showIPOnLCD();
  bootMillis = millis();

  // HTTP route: index
  server.on("/", HTTP_GET, [](){
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  // (Optional) simple health endpoint
  server.on("/health", HTTP_GET, [](){
    server.send(200, "text/plain", "ok");
  });

  server.begin();
  Serial.println("HTTP server started on :80");

  // WebSocket server (port 81)
  ws.begin();
  ws.onEvent(onWsEvent);
  Serial.println("WebSocket server started on :81");
}

void loop(){
  server.handleClient();
  ws.loop();

  // Periodic DHT read + broadcast
  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_MS){
    lastSample = now;

    float h = dht.readHumidity();
    float t = dht.readTemperature(); // Celsius

    if (isnan(h) || isnan(t)){
      Serial.println("DHT read failed");
      // Show transient error on LCD (don’t spam/clear too frequently)
      if (lcdOk) {
        lcd.clear();
        lcdPrintCentered(0, "DHT read error");
        lcdPrintCentered(1, "Retrying...");
      }
      return;
    }

    float hi = heatIndexC(t, h);

    // Save for LCD pages
    lastT = t; lastH = h; lastHI = hi;

    // Build JSON and broadcast
    char json[160];
    snprintf(json, sizeof(json),
             R"({"temp":%.2f,"hum":%.2f,"heat":%.2f,"ts":%lu})",
             t, h, hi, (unsigned long)now);
    ws.broadcastTXT(json);

    // Periodic meta broadcast (every ~10s)
    static uint8_t metaTick = 0;
    if (++metaTick >= 5){
      metaTick = 0;
      String metaAll = String("{\"type\":\"meta\",\"clients\":") + ws.connectedClients()
                     + ",\"ip\":\"" + ipToString(WiFi.localIP()) + "\"}";
      ws.broadcastTXT(metaAll);
      wsClientCount = ws.connectedClients();
    }
  }

  // LCD rotate pages (non-blocking)
  updateLCD();
}
