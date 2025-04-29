// IoT RFID‑Based Smart Rationing System for ESP32 (SIM800L – raw AT commands)
// Uses built‑in synchronous WebServer (no ESPAsyncWebServer)
// Serial debug prints + enhanced RFID handling borrowed from verified sample code
// -----------------------------------------------------------------------------------
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

//-------------------------------------
// ---------- CONFIG SECTION ----------
//-------------------------------------
const char *ssid     = "MyProject";
const char *password = "12345678";

#define SS_PIN   5
#define RST_PIN  4
#define BTN1_PIN 12   // 1 unit
#define BTN2_PIN 14   // 3 units
#define SERVO_RICE_PIN 27
#define SERVO_OIL_PIN  26

#define SIM800_TX 17
#define SIM800_RX 16
#define GSM_BAUD 9600

struct UserInfo {
  String uid;          // uppercase hex string without spaces
  String phone;
  String name;
  float riceTaken;
  float oilTaken;
};
std::vector<UserInfo> users = {
  {"73D5F903", "917738425243","Aditya Parde", 0, 0}, // example UID from sample code (Javeria)
  {"C9580104", "918452859413","Neelam", 0, 0}, // example UID from sample code (Javeria)
  {"F387F703", "919004359307","Rishika Parde", 0, 0}, // example UID from sample code (Javeria)
  {"CC27F703", "918080258925","HariOm Tondee", 0, 0}
};

float riceStock = 10.0; // kg
float oilStock  = 10.0; // litres
String lastStateJson;

//-------------------------------------
// -------------- OBJECTS ------------
//-------------------------------------
MFRC522 rfid(SS_PIN, RST_PIN);
Servo servoRice;
Servo servoOil;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);
//HardwareSerial GSM(2); // UART2 for SIM800L

//-------------------------------------
// ------------- HELPERS -------------
//-------------------------------------
String buildStateJson(const String &uid = "") {
  String json = "{\"rice\":" + String(riceStock, 1) + ",\"oil\":" + String(oilStock, 1);
  if (uid.length()) json += ",\"uid\":\"" + uid + "\"";
  json += "}";
  Serial.println("[STATE] " + json);
  return json;
}

void broadcastState(const String &uid = "") {
  lastStateJson = buildStateJson(uid);
}

void lcdStatus(const String &line1, const String &line2="") {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(line1);
  lcd.setCursor(0,1); lcd.print(line2);
  Serial.println("[LCD] " + line1 + " | " + line2);
}

void slowMove(Servo &s, int startAng, int endAng, int step=1, int delayMs=15) {
  for(int a=startAng; (startAng<endAng)? a<=endAng : a>=endAng; a+=(startAng<endAng? step:-step)) {
    s.write(a);
    delay(delayMs);
  }
}

//-------------------------------------
// ----- SIM800L AT‑command helpers ---
//-------------------------------------
void gsmWrite(const String &cmd) {
  Serial2.println(cmd);
  Serial.println("[GSM TX] " + cmd);
}

void flushGSM(unsigned long timeout = 1000) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeout) {
    while (Serial2.available()) {
      char c = Serial2.read();
      Serial.write(c);
    }
  }
}

void test_sim800_module() {
  const char *cmds[] = {"AT", "AT+CSQ", "AT+CCID", "AT+CREG?", "ATI", "AT+CBC"};
  for (auto c: cmds) {
    gsmWrite(c);
    flushGSM();
  }
}

void sendSMS_AT(const String &phone, const String &msg) {
  Serial2.println("AT+CMGF=1"); // Configuring TEXT mode
  delay(500);
  //updateSerial();
  Serial2.println("AT+CMGS=\"+"+phone+"\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  //updateSerial();
  Serial2.print(msg); //text content
  //updateSerial();
delay(500);
Serial.println();
  Serial.println("Message Sent");
  Serial2.write(26);
  delay(3000);
}

//-------------------------------------
// -------- DISPENSING & LOGIC -------
//-------------------------------------
void dispense(float qty, const String &uid) {
  Serial.printf("[DISPENSE] UID %s Qty %.0f\n", uid.c_str(), qty);
  int openTime = (qty==1)? 5000 : 15000; // ms

  slowMove(servoRice, 0, 90);
  slowMove(servoOil,  0, 90);
  delay(openTime);
  slowMove(servoRice, 90, 0);
  slowMove(servoOil,  90, 0);

  riceStock -= qty; if (riceStock<0) riceStock=0;
  oilStock  -= qty; if (oilStock<0)  oilStock=0;
  Serial.printf("[STOCK] Rice %.1f kg | Oil %.1f L\n", riceStock, oilStock);

  for (auto &u: users) {
    if (u.uid==uid) {
      u.riceTaken += qty;
      u.oilTaken  += qty;
      Serial.printf("[USER] %s totals -> Rice %.1f | Oil %.1f\n", uid.c_str(), u.riceTaken, u.oilTaken);
    }
  }
  broadcastState(uid);

  for (auto &u: users) if (u.uid==uid) { sendSMS_AT(u.phone, "Thanks for purchasing "+ String(u.name) +" with quantity " + String(qty,0) + "unit(s) each of Rice & Oil."); break; }
}

//-------------------------------------
// ----------- WEB HANDLERS ----------
//-------------------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><title>Ration Stock</title>
<style>body{font-family:sans-serif;text-align:center;margin-top:40px}h1{font-size:2em}#log{margin-top:20px}</style></head><body>
<h1>Smart Rationing Dashboard</h1>
<h2 id="stock">Loading...</h2>
<div id="log"></div>
<script>
async function refresh(){
  const d=await fetch('/state').then(r=>r.json());
  document.getElementById('stock').textContent=`Rice: ${d.rice} kg | Oil: ${d.oil} ℓ`;
  if(d.uid){const p=document.getElementById('log');p.textContent=`UID ${d.uid} purchased`;document.getElementById('log').prepend(p);}
}
setInterval(refresh,1000);refresh();
</script></body></html>)rawliteral";

void handleRoot(){
  Serial.println("[HTTP] GET /");
  server.send_P(200, "text/html", index_html);
}

void handleState(){
  server.send(200, "application/json", lastStateJson);
}

//-------------------------------------
// ---------- RFID HELPERS -----------
//-------------------------------------
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

String uidToString(byte *uid, byte uidSize) {
  String s="";
  for(byte i=0;i<uidSize;i++){
    if(uid[i]<0x10) s += "0"; // leading zero
    s += String(uid[i],HEX);
  }
  s.toUpperCase();
  return s;
}

//-------------------------------------
// ------------- SETUP --------------
//-------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] Starting up...");

  Serial2.begin(GSM_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  lcd.begin(); lcd.backlight(); lcdStatus("Smart Rationing", "Booting...");

  servoRice.attach(SERVO_RICE_PIN);
  servoOil.attach(SERVO_OIL_PIN);
  servoRice.write(0); servoOil.write(0);

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("[RFID] Reader initialised");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcdStatus("WiFi Connecting", "...");
  Serial.print("[WiFi] Connecting");
  while (WiFi.status()!=WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.println("[WiFi] Connected IP: " + WiFi.localIP().toString());
  lcdStatus("WiFi OK", WiFi.localIP().toString());

  broadcastState();

  server.on("/", handleRoot);
  server.on("/state", HTTP_GET, handleState);
  server.begin();
  Serial.println("[HTTP] Server started");
Serial2.println("AT+CMGF=1"); // Configuring TEXT mode
delay(500);
  //updateSerial();
  Serial2.println("AT+CMGS=\"+917738425243\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  //updateSerial();
  Serial2.print("Project Started - Smart Ration"); //text content2
  delay(500);
  //updateSerial();
Serial2.println();
  Serial.println("Message Sent");
  Serial2.write(26);
  delay(3000);
  //test_sim800_module();
  lcdStatus("System Ready", "Scan Card");
  Serial.println("[BOOT] Complete");
}

//-------------------------------------
// ------------- LOOP ---------------
//-------------------------------------
void loop() {
  server.handleClient();
static uint32_t lastBeat = 0;
if (millis() - lastBeat > 1000) {
  Serial.print(".");
  lastBeat = millis();
}
  // -------- Enhanced RFID handling --------
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())     return;

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print("[RFID] PICC type: ");
  Serial.println(rfid.PICC_GetTypeName(piccType));

  

  Serial.print("[RFID] UID:");
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println();

  String uidStr = uidToString(rfid.uid.uidByte, rfid.uid.size);
  Serial.println("[CARD] Detected UID: " + uidStr);
  lcdStatus("UID:", uidStr);

  // --------- Quantity selection ---------
  uint32_t t0 = millis();
  float qty = 0;
  while (millis() - t0 < 10000 && qty == 0) {
    if (!digitalRead(BTN1_PIN)) { qty = 1; Serial.println("[BTN] Quantity 1 selected"); }
    if (!digitalRead(BTN2_PIN)) { qty = 3; Serial.println("[BTN] Quantity 3 selected"); }
  }
  if (qty == 0) {
    Serial.println("[TIMEOUT] No button pressed");
    lcdStatus("Timeout", "Try again");
    delay(1500);
    lcdStatus("Scan Card");
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  lcdStatus("Dispensing", String(qty,0) + " unit(s)");
  dispense(qty, uidStr);

  lcdStatus("Done!", "Scan Next");
  delay(1500);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
