// Motor Sequential Cycler
// Arduino Nano + ULN2803A
// Button toggles ON/OFF, motors cycle 2 min each in sequence

constexpr uint8_t PIN_BUTTON  = 2;
constexpr uint8_t PIN_MOTOR1  = 6;
constexpr uint8_t PIN_MOTOR2  = 8;
constexpr uint8_t PIN_MOTOR3  = 9;

constexpr unsigned long MOTOR_RUN_MS   = 2UL * 60UL * 1000UL; // 2 minutes
constexpr unsigned long DEBOUNCE_MS    = 50;

const uint8_t MOTOR_PINS[3] = { PIN_MOTOR1, PIN_MOTOR2, PIN_MOTOR3 };

enum class SystemState { IDLE, RUNNING };

SystemState state          = SystemState::IDLE;
uint8_t     activeMotor    = 0;          // 0-2
unsigned long motorStartMs = 0;

// Button debounce state
bool          lastButtonRaw    = HIGH;
bool          lastButtonStable = HIGH;
unsigned long lastDebounceMs   = 0;

// ─── Helpers ──────────────────────────────────────────────────────────────────

void allMotorsOff() {
  for (uint8_t i = 0; i < 3; i++) digitalWrite(MOTOR_PINS[i], LOW);
}

void activateMotor(uint8_t index) {
  allMotorsOff();
  digitalWrite(MOTOR_PINS[index], HIGH);
  motorStartMs = millis();
  Serial.print(F("[STATE] Motor "));
  Serial.print(index + 1);
  Serial.println(F(" ON"));
}

void startCycle() {
  state       = SystemState::RUNNING;
  activeMotor = 0;
  activateMotor(activeMotor);
  Serial.println(F("[STATE] Cycle STARTED"));
}

void stopCycle() {
  state = SystemState::IDLE;
  allMotorsOff();
  Serial.println(F("[STATE] Cycle STOPPED"));
}

// Returns true on a clean LOW→HIGH button release (debounced)
bool buttonPressed() {
  bool raw = digitalRead(PIN_BUTTON);
  if (raw != lastButtonRaw) lastDebounceMs = millis();
  lastButtonRaw = raw;

  if ((millis() - lastDebounceMs) >= DEBOUNCE_MS) {
    if (lastButtonStable == HIGH && raw == LOW) {   // pressed (active-low)
      lastButtonStable = raw;
      return true;
    }
    lastButtonStable = raw;
  }
  return false;
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Serial.println(F("[BOOT] Motor Sequential Cycler v1.0"));

  pinMode(PIN_BUTTON, INPUT_PULLUP);   // Active-low with internal pull-up
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(MOTOR_PINS[i], OUTPUT);
    digitalWrite(MOTOR_PINS[i], LOW);
  }

  Serial.println(F("[INIT] All motors OFF. Waiting for button press."));
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  // Toggle ON/OFF on button press
  if (buttonPressed()) {
    if (state == SystemState::IDLE) startCycle();
    else                            stopCycle();
  }

  // Advance motor cycle every 2 minutes
  if (state == SystemState::RUNNING) {
    if ((millis() - motorStartMs) >= MOTOR_RUN_MS) {
      activeMotor = (activeMotor + 1) % 3;
      activateMotor(activeMotor);
    }
  }
}