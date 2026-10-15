// Arduino Nano — two active-LOW relays

const byte IR_PIN = 2;
const byte RAIN_PIN = 3;

const byte IR_RELAY_PIN = 9;
const byte RAIN_RELAY_PIN = 10;

const byte RAIN_DETECTED_LEVEL = LOW;

const unsigned long IR_ON_DURATION = 5000;
const unsigned long PRINT_INTERVAL = 500;

unsigned long irStartTime = 0;
unsigned long lastPrintTime = 0;

bool irRelayOn = false;
bool irArmed = true;

void setup() {
  // IR relay OFF; rain relay ON by default.
  digitalWrite(IR_RELAY_PIN, HIGH);
  digitalWrite(RAIN_RELAY_PIN, LOW);

  pinMode(IR_RELAY_PIN, OUTPUT);
  pinMode(RAIN_RELAY_PIN, OUTPUT);

  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(RAIN_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println(F("IR timer and rain relay control started"));
}

void loop() {
  unsigned long now = millis();

  int irReading = digitalRead(IR_PIN);
  int rainReading = digitalRead(RAIN_PIN);

  bool irDetected = irReading == LOW;
  bool rainDetected = rainReading == RAIN_DETECTED_LEVEL;

  // Turn IR relay OFF after 5 seconds.
  if (irRelayOn && now - irStartTime >= IR_ON_DURATION) {
    irRelayOn = false;
    Serial.println(F("IR relay OFF: 5 seconds completed"));
  }

  // Rearm only when the timer has finished and the object is removed.
  if (!irRelayOn && !irDetected) {
    irArmed = true;
  }

  // Start one 5-second cycle per detection.
  if (irDetected && irArmed && !irRelayOn) {
    irRelayOn = true;
    irArmed = false;
    irStartTime = now;

    Serial.println(F("IR relay ON: 5-second timer started"));
  }

  digitalWrite(IR_RELAY_PIN, irRelayOn ? LOW : HIGH);

  // Rain relay: ON when dry, OFF when wet.
  bool rainRelayOn = !rainDetected;
  digitalWrite(RAIN_RELAY_PIN, rainRelayOn ? LOW : HIGH);

  // Print status without blocking.
  if (now - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = now;

    Serial.print(F("IR D2: "));
    Serial.print(irReading);
    Serial.print(F(" | Detected: "));
    Serial.print(irDetected ? F("YES") : F("NO"));
    Serial.print(F(" | Relay D9: "));
    Serial.print(irRelayOn ? F("ON") : F("OFF"));

    if (irRelayOn) {
      Serial.print(F(" | Remaining ms: "));
      Serial.print(IR_ON_DURATION - (now - irStartTime));
    }

    Serial.print(F(" || Rain D3: "));
    Serial.print(rainReading);
    Serial.print(F(" | Wet: "));
    Serial.print(rainDetected ? F("YES") : F("NO"));
    Serial.print(F(" | Relay D10: "));
    Serial.println(rainRelayOn ? F("ON") : F("OFF"));
  }
}