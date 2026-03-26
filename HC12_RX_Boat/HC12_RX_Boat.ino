// ============================================================
//  HC-12 Receiver — LED & Buzzer Controller
//  HC-12: RX->D2, TX->D3  |  LED->D7  |  Buzzer->D8
// ============================================================

#include <SoftwareSerial.h>

// ── HC-12 (unchanged from your working code) ─────────────────
SoftwareSerial HC12(2, 3); // (RX, TX)
const int SET_PIN = 5;

// ── Outputs ───────────────────────────────────────────────────
const int LED_PIN    = 7;
const int BUZZER_PIN = 8;

// ── Packet parser (same logic as your working receiver) ───────
String incomingBuffer = "";
bool   packetStarted  = false;

// ── Non-blocking auto-OFF timers ──────────────────────────────
bool          ledActive    = false;
unsigned long ledStart     = 0;
const unsigned long LED_DUR = 5000; // LED stays ON 5s

bool          buzzerActive  = false;
unsigned long buzzerStart   = 0;
const unsigned long BUZ_DUR = 2000; // Buzzer stays ON 2s

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  HC12.begin(9600);

  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, HIGH);

  pinMode(LED_PIN,    OUTPUT);
  pinMode(6,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN,    LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(6, LOW);

  Serial.println("=== HC-12 Receiver Ready ===");
  Serial.println("Waiting for commands...");
}

// ── Command handler ───────────────────────────────────────────
void handleCommand(String cmd) {
  cmd.trim();
  Serial.print("[CMD] ");
  Serial.println(cmd);

  if (cmd == "LED_ON") {
    digitalWrite(LED_PIN, HIGH);
    ledActive = true;
    ledStart  = millis();
    Serial.println("[LED] ON — auto-off in 5s");

  } else if (cmd == "BUZ_ON") {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerStart  = millis();
    Serial.println("[BUZZER] ON — auto-off in 2s");

  } else {
    Serial.print("[UNKNOWN CMD] ");
    Serial.println(cmd);
  }
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {

  // ── Auto-OFF timers ───────────────────────────────────────
  if (ledActive && millis() - ledStart >= LED_DUR) {
    digitalWrite(LED_PIN, LOW);
    ledActive = false;
    Serial.println("[LED] Auto OFF");
  }

  if (buzzerActive && millis() - buzzerStart >= BUZ_DUR) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    Serial.println("[BUZZER] Auto OFF");
  }

  // ── Read HC-12 (same parser as your working receiver) ─────
  while (HC12.available()) {
    char c = HC12.read();

    if (c == '<') {
      packetStarted  = true;
      incomingBuffer = "";

    } else if (c == '>') {
      if (packetStarted) {
        handleCommand(incomingBuffer);
      }
      packetStarted  = false;
      incomingBuffer = "";

    } else if (packetStarted) {
      incomingBuffer += c;
      if (incomingBuffer.length() > 64) { // overflow guard (your original value)
        packetStarted  = false;
        incomingBuffer = "";
      }
    }
  }

  // ── Serial monitor passthrough (kept from your original) ──
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      HC12.println(msg);
      Serial.print("[SENT BACK] ");
      Serial.println(msg);
    }
  }
}