#include <DFRobotDFPlayerMini.h>

// ── Pin Definitions ──────────────────────────────────────────
#define PIN_PLAY     33
#define PIN_PAUSE    25
#define PIN_NEXT     26
#define PIN_VOL      32
#define PIN_LED      27

// ── State ─────────────────────────────────────────────────────
DFRobotDFPlayerMini player;
bool isPlaying = false;
int  lastVolume = -1;
unsigned long lastDebounce = 0;
const int DEBOUNCE_MS = 200;

void setup() {
  Serial.begin(115200);
  Serial.println("\n==== DFPlayer Controller Starting ====");

  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17
  Serial.println("Serial2 Initialized (RX=16, TX=17)");

  pinMode(PIN_PLAY,  INPUT_PULLUP);
  pinMode(PIN_PAUSE, INPUT_PULLUP);
  pinMode(PIN_NEXT,  INPUT_PULLUP);
  pinMode(PIN_LED,   OUTPUT);

  digitalWrite(PIN_LED, HIGH);
  Serial.println("LED OFF at startup");

  if (!player.begin(Serial2)) {
    Serial.println("❌ DFPlayer init failed!");
    while (true);
  }

  player.volume(15);
  lastVolume = 15;
  Serial.println("✅ DFPlayer Ready.");
  Serial.println("Default Volume: 15");
}

void loop() {

  // ── Volume Pot ────────────────────────────────────────────
  int raw = analogRead(PIN_VOL);          
  int vol = map(raw, 0, 4095, 0, 30);    

  if (vol != lastVolume) {
    player.volume(vol);
    Serial.print("Volume Changed -> Raw: ");
    Serial.print(raw);
    Serial.print(" | Mapped Volume: ");
    Serial.println(vol);
    lastVolume = vol;
  }

  // ── Play Button ──────────────────────────────────────────
  if (digitalRead(PIN_PLAY) == LOW && debounce()) {
    Serial.println("▶ Play Button Pressed");
    player.start();
    digitalWrite(PIN_LED, LOW);
    Serial.println("Music Started | LED ON");
    isPlaying = true;
  }

  // ── Pause Button ─────────────────────────────────────────
  if (digitalRead(PIN_PAUSE) == LOW && debounce()) {
    Serial.println("⏸ Pause Button Pressed");
digitalWrite(PIN_LED, HIGH);
    if (isPlaying) {
      player.stop();
      Serial.println("Music Paused");
      isPlaying = false;
    }
  }

  // ── Next Button ──────────────────────────────────────────
  if (digitalRead(PIN_NEXT) == LOW && debounce()) {
    Serial.println("⏭ Next Button Pressed");
    player.next();
    Serial.println("Playing Next Track");
    isPlaying = true;
  }
}

bool debounce() {
  if (millis() - lastDebounce > DEBOUNCE_MS) {
    lastDebounce = millis();
    Serial.println("Debounce OK");
    return true;
  }
  return false;
}