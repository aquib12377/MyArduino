// ═══════════════════════════════════════════════════════════════════════
//  ESP32 – 3-Motor Sequencer using 2× L9110S Drivers
//  Compatible with ESP32 Arduino Core v3.x (ledcAttach new API)
// ───────────────────────────────────────────────────────────────────────
//  Wiring summary
//  ┌──────────────┬───────────────┬──────────┬──────────┐
//  │  Motor       │  Driver       │  IA Pin  │  IB Pin  │
//  ├──────────────┼───────────────┼──────────┼──────────┤
//  │  Motor 1     │  Driver 1 (A) │  GPIO 25 │  GPIO 26 │
//  │  Motor 2     │  Driver 1 (B) │  GPIO 27 │  GPIO 14 │
//  │  Motor 3     │  Driver 2 (A) │  GPIO 12 │  GPIO 13 │
//  └──────────────┴───────────────┴──────────┴──────────┘
//  Button : GPIO 21 (active LOW – one leg to GPIO 21, other to GND)
//           Uses internal pull-up; no external resistor needed.
// ═══════════════════════════════════════════════════════════════════════

#include <Arduino.h>

// ── Pin Definitions ──────────────────────────────────────────────────
#define MOTOR1_IA   25
#define MOTOR1_IB   26

#define MOTOR2_IA   27
#define MOTOR2_IB   14

#define MOTOR3_IA   32
#define MOTOR3_IB   33

#define BUTTON_PIN  21

// ── PWM / Speed Configuration ────────────────────────────────────────
#define PWM_FREQ       5000
#define PWM_RESOLUTION    8
#define MOTOR_SPEED     180   // 128/255 ~= 50%

// ── Timing ───────────────────────────────────────────────────────────
#define MOTOR_RUN_MS  (2UL * 60UL * 1000UL)   // 2 minutes

// ── State ────────────────────────────────────────────────────────────
enum SystemState { STOPPED, RUNNING };
SystemState systemState = STOPPED;

uint8_t  currentMotor   = 1;
uint32_t motorStartTime = 0;

// ── Button Debounce ──────────────────────────────────────────────────
// FIX: Three separate variables needed for correct debounce:
//   lastRawButton  – raw reading from previous loop (detects any change)
//   stableButton   – last CONFIRMED stable state (used for edge detection)
//   lastDebounceMs – timestamp of last raw change
bool     lastRawButton  = HIGH;   // previous raw reading
bool     stableButton   = HIGH;   // last debounce-confirmed state
uint32_t lastDebounceMs = 0;
#define  DEBOUNCE_MS     50

// ── Press counter ────────────────────────────────────────────────────
uint32_t pressCount = 0;

// ════════════════════════════════════════════════════════════════════
//  Forward declarations
// ════════════════════════════════════════════════════════════════════
void setupPWM();
void runMotor(uint8_t motor);
void stopMotor(uint8_t motor);
void stopAllMotors();
void startSequence();

// ════════════════════════════════════════════════════════════════════
//  setup()
// ════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("=========================================");
  Serial.println("  ESP32 Motor Sequencer  |  Core v3.x   ");
  Serial.println("=========================================");
  Serial.println("  Motor 1 : GPIO 25 (IA)  GPIO 26 (IB)");
  Serial.println("  Motor 2 : GPIO 27 (IA)  GPIO 14 (IB)");
  Serial.println("  Motor 3 : GPIO 12 (IA)  GPIO 13 (IB)");
  Serial.println("  Button  : GPIO 21");
  Serial.println("-----------------------------------------");
  Serial.printf ("  PWM Freq      : %d Hz\n", PWM_FREQ);
  Serial.printf ("  PWM Resolution: %d-bit (0-255)\n", PWM_RESOLUTION);
  Serial.printf ("  Motor Speed   : %d / 255  (~50%%)\n", MOTOR_SPEED);
  Serial.printf ("  Run Time/Motor: %lu ms  (2 min)\n", MOTOR_RUN_MS);
  Serial.println("-----------------------------------------");

  Serial.println("[INIT] Setting up PWM on all motor pins...");
  setupPWM();
  Serial.println("[INIT] PWM setup complete.");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.printf("[INIT] Button configured on GPIO %d (INPUT_PULLUP)\n", BUTTON_PIN);

  stopAllMotors();
  Serial.println("[INIT] All motors stopped.");
  Serial.println("=========================================");
  Serial.println("[READY] Press button to start sequence.");
  Serial.println("=========================================\n");
}

// ════════════════════════════════════════════════════════════════════
//  loop()
// ════════════════════════════════════════════════════════════════════
void loop() {

  // ── 1. Corrected debounce + edge detection ────────────────────────
  bool rawReading = digitalRead(BUTTON_PIN);

  // Step A: If the raw signal changed AT ALL, reset the debounce timer
  if (rawReading != lastRawButton) {
    lastDebounceMs = millis();
    lastRawButton  = rawReading;   // track raw changes immediately
    Serial.printf("[DBG ] Raw pin change detected -> %s\n",
                  rawReading == LOW ? "LOW" : "HIGH");
  }

  // Step B: Only act if the signal has been STABLE for DEBOUNCE_MS
  if ((millis() - lastDebounceMs) >= DEBOUNCE_MS) {

    // Step C: Compare stable state – fire only on a TRUE state change
    if (rawReading != stableButton) {
      stableButton = rawReading;   // update confirmed stable state

      // Falling edge = button pressed (HIGH -> LOW)
      if (stableButton == LOW) {
        pressCount++;
        Serial.println("-----------------------------------------");
        Serial.printf("[BTN] Button PRESSED! (Total presses: %lu)\n", pressCount);

        if (systemState == STOPPED) {
          Serial.println("[BTN] Action -> START sequence");
          startSequence();
        } else {
          Serial.println("[BTN] Action -> STOP sequence");
          systemState = STOPPED;
          stopAllMotors();
          Serial.println("[STOP] All motors turned OFF.");
          Serial.println("[STOP] Sequence reset to Motor 1.");
          Serial.println("[READY] Press button to restart.");
          Serial.println("-----------------------------------------\n");
        }
      }

      // Rising edge = button released (LOW -> HIGH)
      if (stableButton == HIGH) {
        Serial.println("[BTN] Button RELEASED.");
      }
    }
  }

  // ── 2. Motor sequencer timing ─────────────────────────────────────
  if (systemState == RUNNING) {
    uint32_t elapsed   = millis() - motorStartTime;
    uint32_t remaining = MOTOR_RUN_MS - elapsed;

    // Countdown print every 30 seconds
    static uint32_t lastPrintMs = 0;
    if (millis() - lastPrintMs >= 30000) {
      lastPrintMs = millis();
      Serial.printf("[TIME] Motor %d running... %.0f sec remaining\n",
                    currentMotor, remaining / 1000.0f);
    }

    if (elapsed >= MOTOR_RUN_MS) {
      Serial.println("-----------------------------------------");
      Serial.printf("[DONE] Motor %d completed its 2-minute run.\n", currentMotor);
      stopMotor(currentMotor);
      Serial.printf("[STOP] Motor %d stopped.\n", currentMotor);

      currentMotor   = (currentMotor % 3) + 1;   // 1->2->3->1->...
      motorStartTime = millis();

      Serial.printf("[NEXT] Advancing to Motor %d\n", currentMotor);
      runMotor(currentMotor);
      Serial.printf("[RUN ] Motor %d started at %d%% speed (%d/255 PWM)\n",
                    currentMotor, (MOTOR_SPEED * 100) / 255, MOTOR_SPEED);
      Serial.printf("[INFO] Will run for %lu seconds (2 min)\n", MOTOR_RUN_MS / 1000);
      Serial.println("-----------------------------------------\n");
    }
  }
}

// ════════════════════════════════════════════════════════════════════
//  startSequence()
// ════════════════════════════════════════════════════════════════════
void startSequence() {
  stopAllMotors();
  systemState    = RUNNING;
  currentMotor   = 1;
  motorStartTime = millis();

  Serial.println("-----------------------------------------");
  Serial.println("[START] Sequence initiated!");
  Serial.println("[START] Order: Motor1 -> Motor2 -> Motor3 -> loop");
  Serial.printf ("[RUN ] Motor %d started at %d%% speed (%d/255 PWM)\n",
                  currentMotor, (MOTOR_SPEED * 100) / 255, MOTOR_SPEED);
  Serial.printf ("[INFO] Will run for %lu seconds (2 min)\n", MOTOR_RUN_MS / 1000);
  Serial.println("-----------------------------------------\n");

  runMotor(currentMotor);
}

// ════════════════════════════════════════════════════════════════════
//  setupPWM() – ESP32 Core v3.x pin-based API
// ════════════════════════════════════════════════════════════════════
void setupPWM() {
  Serial.printf("[PWM ] Attaching Motor 1 IA -> GPIO %d\n", MOTOR1_IA);
  ledcAttach(MOTOR1_IA, PWM_FREQ, PWM_RESOLUTION);
  Serial.printf("[PWM ] Attaching Motor 1 IB -> GPIO %d\n", MOTOR1_IB);
  ledcAttach(MOTOR1_IB, PWM_FREQ, PWM_RESOLUTION);

  Serial.printf("[PWM ] Attaching Motor 2 IA -> GPIO %d\n", MOTOR2_IA);
  ledcAttach(MOTOR2_IA, PWM_FREQ, PWM_RESOLUTION);
  Serial.printf("[PWM ] Attaching Motor 2 IB -> GPIO %d\n", MOTOR2_IB);
  ledcAttach(MOTOR2_IB, PWM_FREQ, PWM_RESOLUTION);

  Serial.printf("[PWM ] Attaching Motor 3 IA -> GPIO %d\n", MOTOR3_IA);
  ledcAttach(MOTOR3_IA, PWM_FREQ, PWM_RESOLUTION);
  Serial.printf("[PWM ] Attaching Motor 3 IB -> GPIO %d\n", MOTOR3_IB);
  ledcAttach(MOTOR3_IB, PWM_FREQ, PWM_RESOLUTION);
}

// ════════════════════════════════════════════════════════════════════
//  runMotor()
// ════════════════════════════════════════════════════════════════════
void runMotor(uint8_t motor) {
  switch (motor) {
    case 1:
      ledcWrite(MOTOR1_IA, MOTOR_SPEED);
      ledcWrite(MOTOR1_IB, 0);
      break;
    case 2:
      ledcWrite(MOTOR2_IA, MOTOR_SPEED);
      ledcWrite(MOTOR2_IB, 0);
      break;
    case 3:
      ledcWrite(MOTOR3_IA, MOTOR_SPEED);
      ledcWrite(MOTOR3_IB, 0);
      break;
    default:
      Serial.printf("[ERROR] runMotor: invalid motor number -> %d\n", motor);
  }
}

// ════════════════════════════════════════════════════════════════════
//  stopMotor()
// ════════════════════════════════════════════════════════════════════
void stopMotor(uint8_t motor) {
  switch (motor) {
    case 1:
      ledcWrite(MOTOR1_IA, 0);
      ledcWrite(MOTOR1_IB, 0);
      break;
    case 2:
      ledcWrite(MOTOR2_IA, 0);
      ledcWrite(MOTOR2_IB, 0);
      break;
    case 3:
      ledcWrite(MOTOR3_IA, 0);
      ledcWrite(MOTOR3_IB, 0);
      break;
    default:
      Serial.printf("[ERROR] stopMotor: invalid motor number -> %d\n", motor);
  }
}

// ════════════════════════════════════════════════════════════════════
//  stopAllMotors()
// ════════════════════════════════════════════════════════════════════
void stopAllMotors() {
  for (uint8_t i = 1; i <= 3; i++) {
    stopMotor(i);
  }
  Serial.println("[STOP] All motors coasted to stop.");
}