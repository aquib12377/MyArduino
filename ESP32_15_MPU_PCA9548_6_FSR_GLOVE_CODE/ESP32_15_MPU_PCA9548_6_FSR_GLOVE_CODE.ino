#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ===================== OPTIONS =====================
#define SHOW_MISSING_IMU_MSG   1
#define PRESENCE_POLL_MS       5000
#define TELEMETRY_MS           1000
#define LCD_REFRESH_MS         1000

// ===================== Wi-Fi =====================
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";
WebServer server(80);
struct ImuRaw{ int16_t ax,ay,az,t,gx,gy,gz; };

// ===================== LCD (I2C) =====================
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ===================== FSRs =====================
#define FSR1_PIN 33
#define FSR2_PIN 39
#define FSR3_PIN 34
#define FSR4_PIN 35
#define FSR5_PIN 32
#define FSR6_PIN 36   // palm
static inline int readFSR(int pin) {
  long sum = 0; const int N = 10;
  for (int i=0;i<N;i++){ sum += analogRead(pin); delayMicroseconds(200); }
  int v = sum / N;
  v = map(v, 0, 4095, 0, 100);
  if(v<0) v=0; if(v>100) v=100;
  return v;
}

// ===================== Button (zero) =====================
#define BTN_PIN 27
const unsigned long DEBOUNCE_MS = 30;
int lastBtn = HIGH;
unsigned long lastBtnChange = 0;
bool zeroSet = false;

// ===================== I2C / MUX / MPU =====================
#define SDA_PIN 21
#define SCL_PIN 22
#define I2C_FREQ 400000UL

constexpr uint8_t MUX_74 = 0x74;   // PCA9548 at 0x74 (non-sequential on PCB)
constexpr uint8_t MUX_71 = 0x71;   // PCA9548 at 0x71 (sequential on PCB)
constexpr uint8_t MPU_ADDR = 0x68;

constexpr uint8_t REG_WHO_AM_I     = 0x75;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

struct Slot { uint8_t mux; uint8_t ch; bool ok; uint8_t who; const char* label; };
#define N_SLOTS 16
Slot slots[N_SLOTS] = {
  // 0x74 group (0..7) – your custom order
  {MUX_74,0,false,0,"INDEX:MCP"}, // 0
  {MUX_74,1,false,0,"INDEX:PIP"}, // 1
  {MUX_74,7,false,0,"INDEX:DIP"}, // 2
  {MUX_74,6,false,0,"SECOND FINGER:MCP"}, // 3
  {MUX_74,5,false,0,"SECOND FINGER:PIP"}, // 4
  {MUX_74,4,false,0,"SECOND FINGER:DIP"}, // 5  <-- source for alias
  {MUX_74,3,false,0,"THIRD FINGER:MCP"}, // 6  <-- alias from MUX5 (display)
  {MUX_74,2,false,0,"THIRD FINGER:PIP"}, // 7  <-- alias from MUX5 (display)
  // 0x71 group (8..15) – sequential
  {MUX_71,0,false,0,"THIRD FINGER:DIP"}, // 8
  {MUX_71,1,false,0,"FOURTH FINGER:MCP"}, // 9
  {MUX_71,2,false,0,"FOURTH FINGER:DIP"}, //10
  {MUX_71,3,false,0,"FOURTH FINGER:PIP"}, //11
  {MUX_71,4,false,0,"THUMP FINGER:MCP"}, //12
  {MUX_71,5,false,0,"THUMP FINGER:DIP"}, //13
  {MUX_71,6,false,0,"THUMP FINGER:PIP"}, //14
  {MUX_71,7,false,0,"NA"}  //15
};

struct AccLP { float x=0, y=0, z=0; bool inited=false; };
AccLP accLP[N_SLOTS];
const float LP_ALPHA = 0.15f;

static inline bool muxWrite(uint8_t mux, uint8_t mask){ Wire.beginTransmission(mux); Wire.write(mask); return Wire.endTransmission()==0; }
static inline void muxOff(uint8_t mux){ muxWrite(mux,0x00); }
static inline bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val){
  Wire.beginTransmission(addr); Wire.write(reg); Wire.write(val);
  return Wire.endTransmission()==0;
}
static inline bool i2cRead(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t n){
  Wire.beginTransmission(addr); Wire.write(reg);
  if (Wire.endTransmission(false)!=0) return false;
  if (Wire.requestFrom((int)addr,(int)n)!=n) return false;
  for(uint8_t i=0;i<n;i++) buf[i]=Wire.read();
  return true;
}
static inline bool mpuInit(uint8_t addr){
  if(!i2cWrite8(addr, REG_PWR_MGMT_1, 0x00)) return false; delay(5);
  if(!i2cWrite8(addr, REG_SMPLRT_DIV, 9))    return false;
  if(!i2cWrite8(addr, REG_CONFIG, 0x03))     return false;
  if(!i2cWrite8(addr, REG_GYRO_CONFIG, 0x00))return false;
  if(!i2cWrite8(addr, REG_ACCEL_CONFIG,0x00))return false;
  return true;
}
static inline bool mpuReadAll(uint8_t addr, ImuRaw& r){
  uint8_t b[14];
  if(!i2cRead(addr, REG_ACCEL_XOUT_H, b, 14)) return false;
  r.ax=(int16_t)(b[0]<<8|b[1]); r.ay=(int16_t)(b[2]<<8|b[3]); r.az=(int16_t)(b[4]<<8|b[5]);
  r.t =(int16_t)(b[6]<<8|b[7]); r.gx=(int16_t)(b[8]<<8|b[9]); r.gy=(int16_t)(b[10]<<8|b[11]); r.gz=(int16_t)(b[12]<<8|b[13]);
  return true;
}
// accel -> g -> low-pass -> normalize -> pitch/roll
static inline bool getNormAccelVec(int slotIdx, float v[3]){
  if(!slots[slotIdx].ok) return false;
  ImuRaw r;
  if(!muxWrite(slots[slotIdx].mux, 1<<slots[slotIdx].ch)) return false;
  bool ok = mpuReadAll(MPU_ADDR, r);
  muxOff(slots[slotIdx].mux);
  if(!ok) return false;
  float ax=r.ax/16384.0f, ay=r.ay/16384.0f, az=r.az/16384.0f;
  AccLP &lp = accLP[slotIdx];
  if(!lp.inited){ lp = {ax,ay,az,true}; }
  else { lp.x += LP_ALPHA*(ax-lp.x); lp.y += LP_ALPHA*(ay-lp.y); lp.z += LP_ALPHA*(az-lp.z); }
  float n = sqrtf(lp.x*lp.x + lp.y*lp.y + lp.z*lp.z); if(n<1e-6f) return false;
  v[0]=lp.x/n; v[1]=lp.y/n; v[2]=lp.z/n; return true;
}
static inline void accelToPitchRoll(const float v[3], float &pitch, float &roll){
  pitch = atan2f(-v[0], sqrtf(v[1]*v[1] + v[2]*v[2])) * 180.0f/PI;
  roll  = atan2f( v[1], v[2]) * 180.0f/PI;
}

// Presence
bool checkImuPresenceOnce(int i, uint8_t &whoOut){
  if(!muxWrite(slots[i].mux, 1<<slots[i].ch)) return false;
  uint8_t who=0; bool ok = i2cRead(MPU_ADDR, REG_WHO_AM_I, &who, 1);
  muxOff(slots[i].mux);
  if(!ok) return false;
  whoOut = who;
  return (who==0x68 || who==0x70);
}
void pollPresence(bool onStartup){
  for(int i=0;i<N_SLOTS;i++){
    if ((i&1)==0) yield();
    uint8_t who=0;
    bool present = checkImuPresenceOnce(i, who);
    bool nowOk = present;
    if(onStartup && present){
      muxWrite(slots[i].mux, 1<<slots[i].ch);
      bool inited = mpuInit(MPU_ADDR);
      muxOff(slots[i].mux);
      nowOk = inited;
    }
    if(nowOk != slots[i].ok){
#if SHOW_MISSING_IMU_MSG
      if(nowOk) Serial.printf("IMU%02d CONNECTED (%s, WHO=0x%02X)\n", i, slots[i].label, who);
      else      Serial.printf("IMU%02d NOT CONNECTED (%s)\n", i, slots[i].label);
#endif
      slots[i].ok = nowOk;
      slots[i].who = nowOk ? who : 0;
      accLP[i].inited = false; // re-init LP on state change
    }
  }
}

// ===================== Zero baseline (per-IMU) =====================
float refV[N_SLOTS][3]; bool refHas[N_SLOTS] = {0};
bool captureBaselineForIMU(int i, float out[3]){
  if(!slots[i].ok) return false;
  const int S=12; float sum[3]={0,0,0}, v[3]; int okc=0;
  for(int k=0;k<S;k++){ if(getNormAccelVec(i,v)){ sum[0]+=v[0]; sum[1]+=v[1]; sum[2]+=v[2]; okc++; } delay(5); }
  if(okc < S/2) return false;
  float n=sqrtf(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]); if(n<1e-6f) return false;
  out[0]=sum[0]/n; out[1]=sum[1]/n; out[2]=sum[2]/n; return true;
}
void setZeroPose(){
  int good=0, total=0;
  for(int i=0;i<N_SLOTS;i++){
    if(!slots[i].ok){ refHas[i]=false; continue; }
    total++;
    if(captureBaselineForIMU(i, refV[i])){ refHas[i]=true; good++; } else refHas[i]=false;
  }
  zeroSet = (good>0);
  Serial.printf("Zero pose %s (%d/%d IMUs captured)\n", zeroSet?"SET":"FAILED", good, total);
}

// ===================== Display/Server Data =====================
struct ImuView { bool ok; float pitch; float roll; };
ImuView gImu[N_SLOTS];
int gFSR[6]={0};
int gImuConnected=0;
bool gZero=false;

// ===================== HTML =====================
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Glove Sensors</title>
<style>
 body{font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif;margin:20px}
 .card{border:1px solid #ddd;border-radius:12px;padding:16px;margin-bottom:12px}
 table{border-collapse:collapse;width:100%} th,td{border-bottom:1px solid #eee;padding:6px 4px}
 .ok{color:#080} .bad{color:#a00}
</style></head><body>
<h1>Glove Sensors</h1>
<div class=card id=summary>Loading…</div>
<div class=card>
  <h3>FSR (0–100)</h3>
  <table><tbody id=fsr></tbody></table>
</div>
<div class=card>
  <h3>IMUs</h3>
  <table><thead><tr><th>#</th><th>Label</th><th>OK</th><th>Flexion</th></tr></thead>
  <tbody id=imus></tbody></table>
</div>
<script>
async function load(){
  try{
    const r=await fetch('/api/telemetry',{cache:'no-store'});
    const j=await r.json();
    document.getElementById('summary').innerHTML =
      `Zero: <b class="${j.zero?'ok':'bad'}">${j.zero?'YES':'NO'}</b> · IMUs: <b>${j.imus_connected}/${j.total_imus}</b> · ${new Date(j.ts).toLocaleTimeString()}`;
    const fsr=['F1','F2','F3','F4','F5','Palm'].map((n,i)=>`<tr><td>${n}</td><td><b>${j.fsr[i]}</b></td></tr>`).join('');
    document.getElementById('fsr').innerHTML = fsr;
    const rows = j.imus.map((o,i)=>`<tr><td>${i}</td><td>${o.label}</td><td>${o.ok?'OK':'NO'}</td><td>${o.ok?o.pitch.toFixed(1)+'°':'—'}</td></tr>`).join('');
    document.getElementById('imus').innerHTML = rows;
  }catch(e){ console.error(e); }
}
setInterval(load,1000); load();
</script></body></html>
)HTML";

// ===================== Timers =====================
unsigned long lastPresenceMs=0, lastTelemetryMs=0, lastLCDms=0;
uint8_t lcdPage = 0; // 0=FSR page, 1..N_SLOTS = each IMU page

// ===================== Helpers =====================
static inline String fmt1(float v){ char b[16]; snprintf(b,sizeof(b),"%.1f",v); return String(b); }

// ===================== Setup =====================
void setup(){
  Serial.begin(115200);
  delay(100);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_FREQ);
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(10);
#endif

  // LCD
  lcd.begin(); lcd.backlight();
  lcd.clear(); lcd.setCursor(0,0); lcd.print("Glove Booting...");
  lcd.setCursor(0,1); lcd.print("WiFi starting");

  // Button
  pinMode(BTN_PIN, INPUT_PULLUP);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  uint32_t t0=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-t0<15000){ delay(300); Serial.print("."); }
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){
    Serial.printf("WiFi OK: %s\n", WiFi.localIP().toString().c_str());
    lcd.clear(); lcd.setCursor(0,0); lcd.print("WiFi OK:");
    lcd.setCursor(0,1); lcd.print(WiFi.localIP().toString().c_str());
    MDNS.begin("glove");
  } else {
    Serial.println("WiFi FAILED (offline)");
    lcd.clear(); lcd.setCursor(0,0); lcd.print("WiFi FAILED");
    lcd.setCursor(0,1); lcd.print("Check SSID/PASS");
  }

  // HTTP
  server.on("/", [](){ server.send_P(200,"text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/telemetry", [](){
    server.sendHeader("Access-Control-Allow-Origin","*");
    String s; s.reserve(1024);
    s += "{";
    s += "\"zero\":" + String(gZero?"true":"false");
    s += ",\"imus_connected\":" + String(gImuConnected);
    s += ",\"total_imus\":" + String(N_SLOTS);
    s += ",\"fsr\":[" + String(gFSR[0])+","+String(gFSR[1])+","+String(gFSR[2])+","+String(gFSR[3])+","+String(gFSR[4])+","+String(gFSR[5])+"]";
    s += ",\"imus\":[";
    for(int i=0;i<N_SLOTS;i++){
      if(i) s += ",";
      s += "{\"label\":\""; s += slots[i].label; s += "\"";
      s += ",\"ok\":" + String(gImu[i].ok?"true":"false");
      s += ",\"pitch\":" + String(gImu[i].pitch,1);
      s += ",\"roll\":" + String(gImu[i].roll,1);
      s += "}";
    }
    s += "],\"ts\":" + String((uint32_t)millis()) + "}";
    server.send(200,"application/json", s);
  });
  server.begin();

  // MUX off
  muxWrite(MUX_74,0x00); muxWrite(MUX_71,0x00);

  Serial.println("\nDetecting and initializing MPUs...");
  pollPresence(true);
  Serial.println("Init complete. BTN (GPIO27) to set ZERO.");
}

// ===================== Loop =====================
void loop(){
  server.handleClient();

  // Presence poll
  if (millis()-lastPresenceMs >= PRESENCE_POLL_MS){
    lastPresenceMs = millis();
    pollPresence(false);
#if SHOW_MISSING_IMU_MSG
    int present=0; for(int i=0;i<N_SLOTS;i++) present += slots[i].ok?1:0;
    Serial.printf("[Presence] IMUs connected: %d/%d\n", present, N_SLOTS);
#endif
  }

  // Button (zero)
  int b = digitalRead(BTN_PIN);
  if (b != lastBtn) { lastBtnChange = millis(); lastBtn = b; }
  if ((millis() - lastBtnChange) > DEBOUNCE_MS){
    if (b == LOW){ while(digitalRead(BTN_PIN)==LOW) { delay(1); server.handleClient(); } setZeroPose(); }
  }

  // Telemetry compute (individual sensors only)
  if (millis()-lastTelemetryMs >= TELEMETRY_MS){
    lastTelemetryMs = millis();

    // FSRs
    gFSR[0]=readFSR(FSR1_PIN); gFSR[1]=readFSR(FSR2_PIN); gFSR[2]=readFSR(FSR3_PIN);
    gFSR[3]=readFSR(FSR4_PIN); gFSR[4]=readFSR(FSR5_PIN); gFSR[5]=readFSR(FSR6_PIN);

    // IMUs: compute pitch/roll per slot
    gImuConnected = 0;
    for(int i=0;i<N_SLOTS;i++){
      float v[3];
      if(slots[i].ok && getNormAccelVec(i,v)){
        float pitch, roll; accelToPitchRoll(v, pitch, roll);
         //If you want zero-referenced angles per IMU, uncomment below:
         if (zeroSet && refHas[i]) { float p0, r0; float vv[3]={refV[i][0],refV[i][1],refV[i][2]}; accelToPitchRoll(vv,p0,r0); pitch -= p0; roll -= r0; }
        gImu[i] = {true, pitch, roll};
        gImuConnected++;
      } else {
        gImu[i] = {false, 0, 0}; 
      }
    }

    // ===== Alias rule for PCA9548 @ 0x74: show MUX4 & MUX3 as MUX5 =====
    // Find indices for 74:MUX5 (ch4), 74:MUX4 (ch3), 74:MUX3 (ch2)
    int idx74_mux5=-1, idx74_mux4=-1, idx74_mux3=-1;
    for(int i=0;i<8;i++){ // first eight are the 0x74 group
      if(slots[i].mux==MUX_74){
        if(slots[i].ch==4) idx74_mux5=i;
        else if(slots[i].ch==3) idx74_mux4=i;
        else if(slots[i].ch==2) idx74_mux3=i;
      }
    }
    if(idx74_mux5!=-1){
      if(idx74_mux4!=-1) gImu[idx74_mux4] = gImu[idx74_mux5];
      if(idx74_mux3!=-1) gImu[idx74_mux3] = gImu[idx74_mux5];
    }

    gZero = zeroSet;
  }

  // LCD: FSR page + per-IMU pages
  if (millis()-lastLCDms >= LCD_REFRESH_MS){
    lastLCDms = millis();
    lcd.clear();

    if(lcdPage==0){
      lcd.setCursor(0,0); lcd.print("FSR: 1 "); lcd.print(gFSR[0]); lcd.print(" 2 "); lcd.print(gFSR[1]);
      lcd.setCursor(0,1); lcd.print("3 "); lcd.print(gFSR[2]); lcd.print(" 4 "); lcd.print(gFSR[3]);
    } else if(lcdPage==1){
      lcd.setCursor(0,0); lcd.print("FSR: 5 "); lcd.print(gFSR[4]); lcd.print(" P "); lcd.print(gFSR[5]);
      lcd.setCursor(0,1); lcd.print("IMU "); lcd.print(gImuConnected); lcd.print("/"); lcd.print(N_SLOTS);
    } else {
      int i = lcdPage - 2; // IMU index
      if(i>=0 && i<N_SLOTS){
        lcd.setCursor(0,0); lcd.print(slots[i].label);
        lcd.setCursor(0,1);
        if(gImu[i].ok){ lcd.print("P:"); lcd.print(fmt1(gImu[i].pitch)); lcd.print(" R:"); lcd.print(fmt1(gImu[i].roll)); }
        else           { lcd.print("NOT CONNECTED"); }
      }
    }

    // rotate: 0 (FSR1/2), 1 (FSR3/4 + summary), 2..(N_SLOTS+1) IMUs
    lcdPage = (lcdPage + 1) % (N_SLOTS + 2);
  }
}
