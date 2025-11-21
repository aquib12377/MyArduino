/*
  Servo Key/Lock Tester with One-Button Start/Stop,
  Smooth 0<->90 Sweeps, Speed Control, LCD 16x2 (I2C),
  and SD Card persistence of total cycles.

  Hardware (defaults; change in CONFIG section):
    - Servo signal: D9 (SG90/MG90S etc.)
    - Button: D2 to GND (uses INPUT_PULLUP, active LOW)
    - Potentiometer (optional for speed): A0 (0..1023)
    - LCD 16x2 I2C: address 0x27 (common) or 0x3F
    - SD card module CS: D10 on UNO/Nano, D53 on MEGA

  "Cycle" = one full sweep 0° -> 90° -> 0°.

  Serial speed override: send "S=8" or "S=15" etc. (ms/degree)
  Smaller number = faster movement.

  Requires libraries:
    - Servo (built-in)
    - LiquidCrystal_I2C (by Marco Schwartz or similar)
    - SD (built-in), SPI (built-in)
*/

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  
#include <SPI.h>
#include <SD.h>

// ======== CONFIG ========
const uint8_t SERVO_PIN       = 9;
const uint8_t BUTTON_PIN      = 4;     // to GND, uses INPUT_PULLUP
const uint8_t POT_PIN         = A0;    // optional; if not used, leave floating or tie to GND

// LCD I2C address (0x27 is common; if your LCD shows nothing, try 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// SD chip select pin (UNO/Nano = 10, MEGA2560 = 53)
#if defined(ARDUINO_AVR_MEGA2560)
const uint8_t SD_CS_PIN = 53;
#else
const uint8_t SD_CS_PIN = 10;
#endif

// Motion limits
const int MIN_DEG = 0;
const int MAX_DEG = 110;
const int STEP_DEG = 2;  // degrees per step (keep at 1 for smoother motion)

// Speed defaults (ms per degree). Lower = faster.
int msPerDeg_default = 15;  // used if pot reads invalid

// Debounce
const unsigned long DEBOUNCE_MS = 60;

// SD file name for cumulative cycles
const char* CYCLES_FILE = "cycles.txt";

// ======== STATE ========
Servo servoMotor;

volatile bool isRunning = false;   // toggled by button
unsigned long lastButtonChangeMs = 0;
bool lastButtonState = HIGH;       // because INPUT_PULLUP (HIGH = not pressed)

unsigned long sessionCycles = 0;   // cycles this session (since last power-on)
unsigned long totalCycles   = 0;   // cumulative cycles loaded/saved to SD

// For sweep tracking
bool goingUp = true;   // true: increasing angle; false: decreasing
int currentDeg = MIN_DEG;

// Speed control
int msPerDeg = 15;     // dynamic (from pot or serial)
bool sdOk = false;

// ======== HELPERS ========

void lcdStatus(const char* line1, const char* line2 = nullptr) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (line2) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
}

void lcdShowLive() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (isRunning) {
    lcd.print("RUN ");
  } else {
    lcd.print("STOP");
  }
  lcd.print(" Cyc:");
  lcd.print(sessionCycles);

  lcd.setCursor(0, 1);
  lcd.print("Tot:");
  lcd.print(totalCycles);
  lcd.print(" sp:");
  lcd.print(msPerDeg);
}

void loadTotalCyclesFromSD() {
  if (!sdOk) return;

  if (!SD.exists(CYCLES_FILE)) {
    // Create with zero
    File f = SD.open(CYCLES_FILE, FILE_WRITE);
    if (f) {
      f.println("0");
      f.close();
    }
    totalCycles = 0;
    return;
  }

  File f = SD.open(CYCLES_FILE, FILE_READ);
  if (!f) {
    totalCycles = 0;
    return;
  }

  // Read first line as integer
  String line = f.readStringUntil('\n');
  line.trim();
  unsigned long val = line.toInt();
  totalCycles = val;
  f.close();
}

void saveTotalCyclesToSD() {
  if (!sdOk) return;
  // Overwrite with the latest total
  SD.remove(CYCLES_FILE);
  File f = SD.open(CYCLES_FILE, FILE_WRITE);
  if (f) {
    f.println(totalCycles);
    f.close();
  }
}

void initSD() {
  if (SD.begin(SD_CS_PIN)) {
    sdOk = true;
  } else {
    sdOk = false;
  }
}

void updateSpeedFromPot() {
  // int raw = analogRead(POT_PIN);  // 0..1023
  // // If floating (no pot), raw might read near 1023; still map it.
  // // Map to [4 .. 25] ms/deg (tweak range to taste)
  // int mappedVal = map(raw, 0, 1023, 4, 25);
  // // Just in case analog nonsense, clamp:
  // if (mappedVal < 2) mappedVal = msPerDeg_default;
  // msPerDeg = mappedVal;
}

void applySerialSpeedIfAny() {
  // Accept simple commands like: S=10  or  s=15
  // (ms per degree; integer 2..50)
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd.startsWith("S=")) {
      int v = cmd.substring(2).toInt();
      if (v >= 2 && v <= 50) {
        msPerDeg = v;
        Serial.print(F("[Speed] ms/deg set to "));
        Serial.println(msPerDeg);
      } else {
        Serial.println(F("[Speed] Ignored (out of range 2..50)"));
      }
    }
  }
}

void doSweepStep() {
  // Move one step in the current direction
  if (goingUp) {
    currentDeg += STEP_DEG;
    if (currentDeg >= MAX_DEG) {
      currentDeg = MAX_DEG;
      goingUp = false;
    }
  } else {
    currentDeg -= STEP_DEG;
    if (currentDeg <= MIN_DEG) {
      currentDeg = MIN_DEG;
      goingUp = true;
      // We just finished a 0->90->0 sweep:
      sessionCycles++;
      totalCycles++;
    }
  }

  servoMotor.write(currentDeg);
  delay(msPerDeg * STEP_DEG);
}

bool readButtonPressedDebounced() {
  bool state = digitalRead(BUTTON_PIN); // HIGH = not pressed, LOW = pressed
  unsigned long now = millis();

  if (state != lastButtonState && (now - lastButtonChangeMs) > DEBOUNCE_MS) {
    lastButtonChangeMs = now;
    lastButtonState = state;
    if (state == LOW) {
      // Press detected
      return true;
    }
  }
  return false;
}

void goToZeroSmooth() {
  int d = currentDeg;
  while (d > MIN_DEG) {
    d -= STEP_DEG;
    if (d < MIN_DEG) d = MIN_DEG;
    servoMotor.write(d);
    delay(msPerDeg * STEP_DEG);
  }
  currentDeg = MIN_DEG;
}

// ======== SETUP/LOOP ========

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(200);

  // LCD
  lcd.begin();
  lcd.backlight();
  lcdStatus("Servo Tester", "Init...");

  // SD
  initSD();
  if (sdOk) {
    lcdStatus("SD: OK", "Loading cycles");
  } else {
    lcdStatus("SD: FAILED", "Running anyway");
  }
  delay(800);

  loadTotalCyclesFromSD();

  // Servo
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(MIN_DEG);
  currentDeg = MIN_DEG;
  goingUp = true;

  lcdShowLive();
  Serial.println(F("[Init] Ready. Press button to start/stop. Use pot (A0) or 'S=10' to set speed."));
}

void loop() {
  // Speed sources
  updateSpeedFromPot();
  applySerialSpeedIfAny();

  // Button handling (toggle)
  if (readButtonPressedDebounced()) {
    if (!isRunning) {
      // START
      isRunning = true;
      Serial.println(F("[State] START"));
      lcdShowLive();
    } else {
      // STOP: finish gracefully at 0°, save totals
      Serial.println(F("[State] STOP -> homing to 0"));
      isRunning = false;
      goToZeroSmooth();
      saveTotalCyclesToSD();

      // Show final numbers
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Stopped @0 deg");
      lcd.setCursor(0,1);
      lcd.print("Cyc:");
      lcd.print(sessionCycles);
      lcd.print(" Tot:");
      lcd.print(totalCycles);

      Serial.print(F("[Cycles] Session="));
      Serial.print(sessionCycles);
      Serial.print(F("  Total="));
      Serial.println(totalCycles);
      delay(1200);

      // Return to main live screen
      lcdShowLive();
    }
  }

  // If running, keep sweeping
  if (isRunning) {
    doSweepStep();
    // Update live screen periodically (not every step to avoid flicker)
    static unsigned long lastLcd = 0;
    unsigned long now = millis();
    if (now - lastLcd > 350) {
      lcdShowLive();
      lastLcd = now;
    }
  } else {
    // Idle; update LCD occasionally to reflect pot/serial speed changes
    static unsigned long lastIdleLcd = 0;
    unsigned long now = millis();
    if (now - lastIdleLcd > 800) {
      lcdShowLive();
      lastIdleLcd = now;
    }
    //delay(5);
  }
}
