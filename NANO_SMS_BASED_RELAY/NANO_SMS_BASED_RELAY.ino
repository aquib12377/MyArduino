#include <SoftwareSerial.h>

// ======= SIM900A PINS (as per your wiring) =======
static const uint8_t SIM_RX_PIN = 10;   // Arduino RX  <- SIM TX
static const uint8_t SIM_TX_PIN = 11u;  // Arduino TX  -> SIM RX (use divider)

SoftwareSerial sim(SIM_RX_PIN, SIM_TX_PIN); // (RX, TX)

// ======= RELAY SETTINGS =======
static const uint8_t RELAY_PIN = 2;     // CHANGE THIS PIN if needed
static const bool RELAY_ACTIVE_LOW = true;

// Buffer for reading lines from SIM900A
char lineBuf[220];
uint16_t linePos = 0;

// Set true if you want to delete SMS after reading it
bool DELETE_AFTER_READ = true;

// ---------- Relay helpers ----------
void relaySet(bool on)
{
  // Active-low relay: ON = LOW, OFF = HIGH
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  }

  Serial.print("🔌 Relay is now: ");
  Serial.println(on ? "ON" : "OFF");
}

void handleSmsCommand(String body)
{
  body.trim();
  body.toUpperCase();

  // Basic commands
  if (body == "ON") {
    relaySet(true);
  }
  else if (body == "OFF") {
    relaySet(false);
  }
  else {
    Serial.print("⚠️ Unknown SMS command: ");
    Serial.println(body);
  }
}

// ---------- SIM helpers ----------
void flushSIM()
{
  while (sim.available()) sim.read();
}

void sendAT(const char* cmd, uint32_t waitMs = 800)
{
  sim.println(cmd);
  uint32_t t = millis();
  while (millis() - t < waitMs) {
    while (sim.available()) {
      Serial.write(sim.read());
    }
  }
}

void processLine(char* ln)
{
  // Trim trailing CR/LF
  size_t n = strlen(ln);
  while (n > 0 && (ln[n - 1] == '\r' || ln[n - 1] == '\n')) {
    ln[n - 1] = '\0';
    n--;
  }
  if (n == 0) return;

  Serial.print("[SIM] ");
  Serial.println(ln);

  // Example: +CMTI: "SM",3
  if (strncmp(ln, "+CMTI:", 6) == 0) {
    char* comma = strchr(ln, ',');
    if (comma) {
      int idx = atoi(comma + 1);
      if (idx > 0) {
        Serial.print("📩 New SMS at index: ");
        Serial.println(idx);
        readSMS(idx);
      }
    }
  }
}

void readSMS(int index)
{
  sim.print("AT+CMGR=");
  sim.println(index);

  String header = "";
  String body   = "";
  bool gotHeader = false;

  uint32_t start = millis();
  while (millis() - start < 6000) {
    while (sim.available()) {
      char c = (char)sim.read();

      if (c == '\n') {
        lineBuf[linePos] = '\0';
        linePos = 0;

        String ln = String(lineBuf);
        ln.trim();
        if (ln.length() == 0) continue;

        Serial.print("[CMGR] ");
        Serial.println(ln);

        if (ln.startsWith("+CMGR:")) {
          header = ln;
          gotHeader = true;
        } else if (gotHeader && body.length() == 0 && !ln.startsWith("OK") && !ln.startsWith("ERROR")) {
          body = ln; // SMS content (text mode)
        } else if (ln == "OK" || ln.startsWith("ERROR")) {
          start = millis() - 6000; // exit
          break;
        }
      } else {
        if (linePos < sizeof(lineBuf) - 1) {
          if (c != '\r') lineBuf[linePos++] = c;
        }
      }
    }
  }

  // Parse sender from header:
  // +CMGR: "REC UNREAD","+9198xxxxxxx","","26/02/02,15:30:00+22"
  String sender = "UNKNOWN";
  if (header.length()) {
    int firstComma = header.indexOf(',');
    if (firstComma >= 0) {
      int q1 = header.indexOf('"', firstComma + 1);
      int q2 = header.indexOf('"', q1 + 1);
      if (q1 >= 0 && q2 > q1) sender = header.substring(q1 + 1, q2);
    }
  }

  Serial.println("----- SMS READ -----");
  Serial.print("From: ");
  Serial.println(sender);
  Serial.print("Msg : ");
  Serial.println(body.length() ? body : "(no body parsed)");
  Serial.println("--------------------");

  // ✅ Trigger relay logic from SMS body
  if (body.length()) {
    handleSmsCommand(body);
  }

  if (DELETE_AFTER_READ) {
    sim.print("AT+CMGD=");
    sim.println(index);
    delay(300);
    Serial.println("🗑️ Deleted SMS (AT+CMGD).");
  }
}

void setupSIM900A()
{
  Serial.println("Setting up SIM900A...");

  sendAT("AT", 800);
  sendAT("AT+CMGF=1", 800);                 // SMS Text mode
  sendAT("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1200);
  sendAT("AT+CNMI=2,1,0,0,0", 1200);        // Notify new SMS (+CMTI)

  Serial.println("SIM900A ready ✅");
}

void setup()
{
  Serial.begin(115200);
  sim.begin(9600);

  // ✅ Relay init
  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false); // default OFF

  delay(1200);
  flushSIM();
  setupSIM900A();

  Serial.println("Listening for incoming SMS...");
}

void loop()
{
  // Read incoming data line-by-line
  while (sim.available()) {
    char c = (char)sim.read();
    Serial.print(c);

    if (c == '\n') {
      lineBuf[linePos] = '\0';
      linePos = 0;
      processLine(lineBuf);
    } else {
      if (linePos < sizeof(lineBuf) - 1) {
        if (c != '\r') lineBuf[linePos++] = c;
      }
    }
  }

  // Passthrough from Serial Monitor to SIM900A
  if (Serial.available()) {
    sim.write(Serial.read());
  }
}
