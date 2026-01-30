#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRonbotDFPlayerMini.h"

/* ========= PIN DEFINITIONS ========= */
const int VOICE_PIN  = 4;   // Input from Arduino (HIGH when voice detected)
const int RELAY_PIN  = 5;   // Active LOW relay for vibration motor

const int TRIG_PIN   = 12;  // Ultrasonic trigger
const int ECHO_PIN   = 14;  // Ultrasonic echo

const int BUZZER_PIN = 18;  // Buzzer (active HIGH)

const int BTN1_PIN   = 32;  // Button 1 – Name
const int BTN2_PIN   = 33;  // Button 2 – Address
const int BTN3_PIN   = 25;  // Button 3 – Buzzer manual
const int BTN4_PIN   = 26;  // Button 4 – Mobile

// DFPlayer Mini UART pins (ESP32 UART1)
const int MP3_RX_PIN = 16;  // ESP32 RX1  <- DFPlayer TX
const int MP3_TX_PIN = 17;  // ESP32 TX1  -> DFPlayer RX

/* ========= OLED SETUP ========= */
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ========= MP3 PLAYER ========= */
HardwareSerial MP3Serial(1);        // UART1 on ESP32
DFRobotDFPlayerMini dfPlayer;

// Choose track numbers on SD card:
// /01/0001.mp3 => track 1 (YES)
// /01/0002.mp3 => track 2 (NO)
const uint16_t TRACK_YES = 1;
const uint16_t TRACK_NO  = 2;

/* ========= LOGIC SETTINGS ========= */
const unsigned long VIBRATION_DURATION_MS = 2000;  // vibration after voice (2s)
const unsigned long BUTTON_DEBOUNCE_MS    = 50;
const unsigned long ULTRA_INTERVAL_MS     = 200;   // ultrasonic check interval
const float         OBSTACLE_THRESHOLD_CM = 40.0;  // distance for buzzer beeping
const unsigned long BEEP_INTERVAL_MS      = 200;   // buzzer beep toggle when obstacle

/* ========= STATE VARIABLES ========= */
bool relayOn               = false;
unsigned long relayOffTime = 0;

bool prevVoiceState        = false;

bool prevBtn1 = false;
bool prevBtn2 = false;
bool prevBtn3 = false;
bool prevBtn4 = false;

unsigned long lastBtnReadTime = 0;

unsigned long lastUltraTime = 0;
float lastDistanceCm = -1;

bool buzzerForced      = false; // Btn3 toggles this
bool buzzerBeepState   = false;
unsigned long lastBeepToggleTime = 0;

/* ====== FORWARD DECLARATIONS ====== */
void showText(const char *line1, const char *line2 = "");
bool readButton(int pin);
float readDistanceCm();
void handleVoiceTrigger();
void handleUltrasonicAndBuzzer();
void handleButtons();
void playYesAudio();
void playNoAudio();

/* ==================================================== */
void setup() {
  // Serial for debugging
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== ESP32 Assistive System Booting ===");

  // Pins
  pinMode(VOICE_PIN, INPUT);         // From Arduino; keep <=3.3V!
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);     // Active LOW -> OFF initially
  Serial.println("Relay set to OFF (HIGH)");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("Ultrasonic pins configured");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);     // Buzzer OFF initially
  Serial.println("Buzzer set to OFF");

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(BTN4_PIN, INPUT_PULLUP);
  Serial.println("Button pins configured (INPUT_PULLUP)");

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 init failed");
    while (true) { delay(100); }
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  showText("System Init...", "Please wait");
  Serial.println("OLED initialized");

  // MP3 (DFPlayer Mini) init
  MP3Serial.begin(9600, SERIAL_8N1, MP3_RX_PIN, MP3_TX_PIN);
  Serial.println("MP3 serial started at 9600");

  if (!dfPlayer.begin(MP3Serial)) {
    Serial.println("DFPlayer init failed!");
    showText("MP3 Error", "Check DFPlayer");
    // System can still run without audio
  } else {
    Serial.println("DFPlayer ready");
    dfPlayer.volume(25);  // 0-30 (adjust as needed)
    Serial.println("DFPlayer volume set to 25");
  }

  showText("System Ready", "Waiting voice...");
  Serial.println("ESP32 Assistive System Ready");
}

/* ==================================================== */
void loop() {
  handleVoiceTrigger();
  handleUltrasonicAndBuzzer();
  handleButtons();
}

/* ========= VOICE TRIGGER HANDLING ========= */
void handleVoiceTrigger() {
  bool voiceState = digitalRead(VOICE_PIN); // HIGH when voice detected

  // Debug voice pin state occasionally
  if (voiceState != prevVoiceState) {
    Serial.print("VOICE_PIN state changed: ");
    Serial.println(voiceState ? "HIGH" : "LOW");
  }

  if (voiceState && !prevVoiceState) {
    // Rising edge: voice detected
    Serial.println("[VOICE] Trigger received from Arduino");

    // Show "Jeevan" and turn on vibration motor (active LOW)
    showText("Jeevan", "Voice detected");
    digitalWrite(RELAY_PIN, LOW);
    relayOn = true;
    relayOffTime = millis() + VIBRATION_DURATION_MS;

    Serial.println("[RELAY] Turned ON (LOW) for vibration");
  }
  prevVoiceState = voiceState;

  // Turn off relay after duration
  if (relayOn && millis() > relayOffTime) {
    digitalWrite(RELAY_PIN, HIGH);
    relayOn = false;
    Serial.println("[RELAY] Turned OFF (HIGH) after duration");
  }
}

/* ========= ULTRASONIC + BUZZER HANDLING ========= */
void handleUltrasonicAndBuzzer() {
  unsigned long now = millis();

  // Read distance at interval
  if (now - lastUltraTime >= ULTRA_INTERVAL_MS) {
    lastUltraTime = now;
    lastDistanceCm = readDistanceCm();

    Serial.print("[ULTRA] Distance: ");
    Serial.print(lastDistanceCm);
    Serial.println(" cm");
  }

  // Buzzer control
  if (buzzerForced) {
    // Manual ON from Btn3
    digitalWrite(BUZZER_PIN, HIGH);
    // Only print occasionally to avoid spam
    static unsigned long lastForcedPrint = 0;
    if (now - lastForcedPrint > 1000) {
      lastForcedPrint = now;
      Serial.println("[BUZZER] Forced ON by Btn3");
    }
    return;
  }

  // Automatic beeping based on distance
  if (lastDistanceCm > 0 && lastDistanceCm < OBSTACLE_THRESHOLD_CM) {
    // Toggle beep at intervals
    if (now - lastBeepToggleTime >= BEEP_INTERVAL_MS) {
      lastBeepToggleTime = now;
      buzzerBeepState = !buzzerBeepState;
      digitalWrite(BUZZER_PIN, buzzerBeepState ? HIGH : LOW);

      Serial.print("[BUZZER] Obstacle < ");
      Serial.print(OBSTACLE_THRESHOLD_CM);
      Serial.print("cm -> ");
      Serial.println(buzzerBeepState ? "ON (beep)" : "OFF (beep gap)");
    }
  } else {
    if (buzzerBeepState) {
      Serial.println("[BUZZER] Obstacle cleared -> OFF");
    }
    // No obstacle, buzzer OFF
    buzzerBeepState = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

/* ========= BUTTON HANDLING ========= */
void handleButtons() {
  unsigned long now = millis();
  if (now - lastBtnReadTime < BUTTON_DEBOUNCE_MS) return;
  lastBtnReadTime = now;

  bool b1 = readButton(BTN1_PIN);
  bool b2 = readButton(BTN2_PIN);
  bool b3 = readButton(BTN3_PIN);
  bool b4 = readButton(BTN4_PIN);

  // Debug button states (optional, comment out if too noisy)
  Serial.print("[BTN] States -> ");
  Serial.print("B1:");
  Serial.print(b1);
  Serial.print(" B2:");
  Serial.print(b2);
  Serial.print(" B3:");
  Serial.print(b3);
  Serial.print(" B4:");
  Serial.println(b4);

  // --- Combination checks first ---

  // Btn1 + Btn2 => YES
  static bool prevYesCombo = false;
  bool yesCombo = b1 && b2 && !b3 && !b4;
  if (yesCombo && !prevYesCombo) {
    showText("Yes", "");
    Serial.println("[COMBO] Btn1+Btn2 -> YES");
    playYesAudio();
  }
  prevYesCombo = yesCombo;

  // Btn2 + Btn3 => NO
  static bool prevNoCombo = false;
  bool noCombo = b2 && b3 && !b1 && !b4;
  if (noCombo && !prevNoCombo) {
    showText("No", "");
    Serial.println("[COMBO] Btn2+Btn3 -> NO");
    playNoAudio();
  }
  prevNoCombo = noCombo;

  // If any combo is active, skip single-button actions
  if (yesCombo || noCombo) {
    prevBtn1 = b1;
    prevBtn2 = b2;
    prevBtn3 = b3;
    prevBtn4 = b4;
    return;
  }

  // --- Single button actions (on rising press) ---
  if (b1 && !prevBtn1) {
    // Btn1: display Name
    showText("Name:", "Jeevan");  // change text as needed
    Serial.println("[BTN1] Name displayed: Jeevan");
  }

  if (b2 && !prevBtn2) {
    // Btn2: display Address
    showText("Address:", "Mumbai, India"); // change to real address
    Serial.println("[BTN2] Address displayed: Mumbai, India");
  }

  if (b3 && !prevBtn3) {
    // Btn3: toggle buzzerForced
    buzzerForced = !buzzerForced;
    if (buzzerForced) {
      digitalWrite(BUZZER_PIN, HIGH);
      showText("Buzzer", "Manual ON");
      Serial.println("[BTN3] Buzzer forced ON");
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      showText("Buzzer", "Manual OFF");
      Serial.println("[BTN3] Buzzer forced OFF");
    }
    Serial.printf("[BTN3] buzzerForced = %s\n", buzzerForced ? "ON" : "OFF");
  }

  if (b4 && !prevBtn4) {
    // Btn4: display Mobile number
    showText("Mobile:", "+91-9876543210");   // change to real number
    Serial.println("[BTN4] Mobile displayed: +91-9876543210");
  }

  prevBtn1 = b1;
  prevBtn2 = b2;
  prevBtn3 = b3;
  prevBtn4 = b4;
}

/* ========= HELPERS ========= */

// Buttons: one side GND, other side pin with INPUT_PULLUP.
// Pressed => LOW, Not pressed => HIGH.
bool readButton(int pin) {
  bool pressed = (digitalRead(pin) == LOW);
  return pressed;
}

// Basic HC-SR04 reading
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL); // timeout ~30ms
  if (duration == 0) {
    Serial.println("[ULTRA] No echo (timeout)");
    return -1; // no echo
  }
  // Speed of sound ~343 m/s -> 29.1 us/cm, /2 for round-trip
  float distance = (duration / 2.0) / 29.1;
  return distance;
}

void showText(const char *line1, const char *line2) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(line1);

  if (line2 && line2[0] != '\0') {
    display.setTextSize(1);
    display.setCursor(0, 32);
    display.println(line2);
  }

  display.display();

  Serial.print("[OLED] Line1: ");
  Serial.print(line1);
  Serial.print(" | Line2: ");
  Serial.println(line2 ? line2 : "(null)");
}

/* ========= AUDIO FUNCTIONS ========= */

void playYesAudio() {
  if (!dfPlayer.available()) {
    Serial.println("[DFP] playYesAudio: dfPlayer not reporting available, sending play anyway");
  }
  dfPlayer.play(TRACK_YES);  // Track 1 on SD card
  Serial.println("[DFP] Playing YES audio (track 1)");
}

void playNoAudio() {
  if (!dfPlayer.available()) {
    Serial.println("[DFP] playNoAudio: dfPlayer not reporting available, sending play anyway");
  }
  dfPlayer.play(TRACK_NO);   // Track 2 on SD card
  Serial.println("[DFP] Playing NO audio (track 2)");
}
