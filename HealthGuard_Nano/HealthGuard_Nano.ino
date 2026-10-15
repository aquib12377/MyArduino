/*
  HealthGuard Nano - classic ATmega328P Nano, 5 V / 16 MHz
  Educational prototype, not a medical or emergency-response device.
  A tilt switch detects orientation, NOT a confirmed fall. A DS18B20
  measures probe temperature, NOT automatically core body temperature.

  LIBRARIES (Arduino Library Manager):
    PulseSensor Playground (World Famous Electronics)
    AltSoftSerial (Paul Stoffregen)
    OneWire (Paul Stoffregen)
    DallasTemperature (Miles Burton and contributors)
    LiquidCrystal I2C (Frank de Brabander; init()/backlight() API)

  WIRING - all grounds common:
    Analog Pulse Sensor: signal A0, VCC 5V, GND GND.
    Digital tilt module: DO D2, VCC 5V, GND GND.
      For a bare two-pin tilt switch: D2 -> switch -> GND.
      Set TILT_NORMAL_STATE to the level read in the mounted upright position.
    DS18B20: DQ D4, VDD 5V, GND GND; 4.7k between DQ and 5V.
      Use three-wire powered mode, not parasite power. Verify probe wire colors.
    16x2 LCD backpack: SDA A4, SCL A5, VCC 5V, GND GND.
    HC-05 TXD -> D8 (Nano RX).
    Nano D9 (TX) -> 1k resistor -> HC-05 RXD.
    HC-05 RXD -> 2k resistor -> GND. This is a 5V-to-3.3V divider.
    HC-05 breakout VCC -> 5V ONLY if its regulator/input rating permits it.
      Bare HC-05 modules require a regulated 3.3V supply.
    Optional recalibration button: D3 -> momentary button -> GND.
    Panic button: D5 -> normally-open momentary button -> GND.
      Uses INPUT_PULLUP; no external resistor is needed.
    Supply: USB or suitable regulated 5V. Do not apply 5V to VIN.
      Avoid simultaneous external 5V and USB power without power isolation.

  NORMAL USE:
    HC05_AT_MODE = 0. Serial Monitor: 115200 baud.
    Keep a finger still on the Pulse Sensor during 8-second startup calibration.
    Calibration sets a signal-detection threshold; it does not calibrate medical
    accuracy. Motion/noise can pass the basic signal checks.
    Send C over USB/Bluetooth or press D3 to recalibrate.
    Press D5 panic button to send ALERT,PANIC,Help_requested once per press
    (40 ms debounce). Release and press again to send another alert.
    LCD shows PANIC ALERT while held and for at least 5 s from the press.
    PANIC in DATA lines reports this temporary indication, not acknowledgment.
    DATA lines every 2 s; alerts go to both USB and Bluetooth.
    Pair AND connect an Android/PC Bluetooth Classic SPP terminal to receive.
    Bluetooth alone does not send SMS/WhatsApp, call anyone, or guarantee delivery.
    No HC-05 STATE/acknowledgment is used; alerts are not stored while disconnected.
    Sustained tilt gives POSSIBLE_FALL after 2 s; repeats every 15 s until upright
    for 3 s. Mount and test the switch; it cannot sense all fall directions.
    HR/temperature limit alerts are OPTIONAL and disabled by default. Set the
    ENABLE_LIMIT_ALERTS flag after choosing application-appropriate limits.

  HC-05 ONE-TIME NAME SETUP (common HC-05 firmware; clones can differ):
    Set HC05_AT_MODE = 1, upload, then enter full AT mode by holding the module
    button during power-up / holding KEY at 3.3V as specified by its vendor.
    Never drive an unprotected KEY input with 5V.
    Open Serial Monitor at 115200, select Both NL & CR. Send separately:
      AT                         (expect OK)
      AT+NAME=HealthGuard_Nano    (expect OK)
      AT+ROLE=0                  (expect OK)
      AT+UART=9600,0,0            (expect OK)
      AT+NAME?                   (verify stored name)
      AT+UART?                   (verify 9600,0,0)
    Set HC05_AT_MODE back to 0 and upload again. Release KEY/button and power
    cycle HC-05 into data mode. Forget/re-pair if the phone cached its old name.
    AT mode uses 38400 on the HC-05 side; normal data mode uses 9600.
    AT mode intentionally runs no sensor monitoring.

  Timer use: AltSoftSerial Timer1 (fixed D8/D9); PulseSensor Timer2.
  Do not add Servo, tone(), or other conflicting timer users.
  This sketch has not been compiled or tested on physical hardware here.
*/

#define HC05_AT_MODE 0

#include <NeoSWSerial.h>
NeoSWSerial bluetooth(8, 9); // RX, TXano

#if HC05_AT_MODE

void setup() {
  Serial.begin(115200);
  bluetooth.begin(38400);
  Serial.println(F("HC-05 AT bridge. Select Both NL & CR; send AT."));
}

void loop() {
  if (Serial.available()) bluetooth.write(Serial.read());
  if (bluetooth.available()) Serial.write(bluetooth.read());
}

#else

#define USE_ARDUINO_INTERRUPTS true
#include <PulseSensorPlayground.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

const byte PULSE_PIN = A0;
const byte TILT_PIN = 2;
const byte CAL_BUTTON_PIN = 3;
const byte TEMP_PIN = 4;
const byte PANIC_BUTTON_PIN = 5;
const byte TILT_NORMAL_STATE = LOW; // change to HIGH if upright reads HIGH
const byte LCD_ADDRESS = 0x27;      // some backpacks use 0x3F

const unsigned long CAL_TIME_MS = 8000;
const unsigned long NO_BEAT_MS = 3000;
const unsigned long FALL_CONFIRM_MS = 2000;
const unsigned long FALL_RECOVER_MS = 3000;
const unsigned long FALL_REPEAT_MS = 15000;
const unsigned long PANIC_DISPLAY_MS = 5000;
const int MIN_SIGNAL_SPAN = 20; // ADC counts, tune for your particular sensor
const int MAX_SIGNAL_SPAN = 700;

// EXAMPLE application limits, not medical diagnostic thresholds.
const bool ENABLE_LIMIT_ALERTS = false;
const int LOW_BPM = 50;
const int HIGH_BPM = 120;
const float HIGH_TEMP_C = 38.0;
const unsigned long LIMIT_HOLD_MS = 10000;

PulseSensorPlayground pulse;
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
OneWire oneWire(TEMP_PIN);
DallasTemperature temperature(&oneWire);

bool pulseStarted = false, calibrating = false, calibrated = false;
bool signalOK = false, bpmValid = false;
int bpm = 0, threshold = 550;
byte goodBeats = 0;
unsigned long calStart = 0, lastSample = 0, lastBeat = 0;
unsigned long signalWindow = 0, pulseWatchStart = 0;
int calMin = 1023, calMax = 0, windowMin = 1023, windowMax = 0;
unsigned long calSum = 0, calCount = 0;
bool pulseMissingAlert = false;

float tempC = 0;
bool tempValid = false, tempFault = false;
unsigned long tempRequested = 0;

byte tiltRaw, tiltStable;
unsigned long tiltChanged = 0, postureSince = 0, lastFallAlert = 0;
bool fallActive = false;

unsigned long lastData = 0, lastLCD = 0;
bool buttonRaw = HIGH, buttonStable = HIGH;
unsigned long buttonChanged = 0;
bool panicRaw = HIGH, panicStable = HIGH, panicActive = false;
unsigned long panicChanged = 0, lastPanicPress = 0;
bool hrPending = false, hrAlarm = false, hotPending = false, hotAlarm = false;
unsigned long hrSince = 0, hotSince = 0;

void message(const __FlashStringHelper *s) {
  Serial.println(s);
  bluetooth.println(s);
}

void updatePanic(unsigned long now) {
  bool raw = digitalRead(PANIC_BUTTON_PIN);
  if (raw != panicRaw) { panicRaw = raw; panicChanged = now; }
  if (raw != panicStable && now - panicChanged >= 40) {
    panicStable = raw;
    if (panicStable == LOW) {
      panicActive = true;
      lastPanicPress = now;
      message(F("ALERT,PANIC,Help_requested"));
    }
  }
  if (panicActive && panicStable == HIGH &&
      now - lastPanicPress >= PANIC_DISPLAY_MS) {
    panicActive = false;
  }
}

void startCalibration() {
  if (!pulseStarted) return;
  calibrating = true;
  calibrated = signalOK = bpmValid = false;
  goodBeats = 0;
  calMin = windowMin = 1023;
  calMax = windowMax = 0;
  calSum = calCount = 0;
  calStart = signalWindow = pulseWatchStart = millis();
  pulseMissingAlert = false;
  message(F("INFO,PULSE_CALIBRATING,Keep_finger_still_8s"));
}

void updatePulse(unsigned long now) {
  if (!pulseStarted) return;
  if (now - lastSample >= 4) {
    lastSample = now;
    int raw = pulse.getLatestSample();
    if (raw < windowMin) windowMin = raw;
    if (raw > windowMax) windowMax = raw;
    // Ignore the first 2 seconds while the finger settles.
    if (calibrating && now - calStart >= 2000) {
      if (raw < calMin) calMin = raw;
      if (raw > calMax) calMax = raw;
      calSum += raw;
      ++calCount;
    }
  }
  if (now - signalWindow >= 2000) {
    int span = windowMax - windowMin;
    signalOK = span >= MIN_SIGNAL_SPAN && span <= MAX_SIGNAL_SPAN &&
               windowMin > 5 && windowMax < 1018;
    windowMin = 1023;
    windowMax = 0;
    signalWindow = now;
    if (!signalOK) { bpmValid = false; goodBeats = 0; }
  }
  if (calibrating && now - calStart >= CAL_TIME_MS) {
    calibrating = false;
    int span = calMax - calMin;
    calibrated = calCount > 100 && span >= MIN_SIGNAL_SPAN &&
                 span <= MAX_SIGNAL_SPAN && calMin > 5 && calMax < 1018;
    if (calibrated) {
      threshold = constrain((int)(calSum / calCount) + span / 4,
                            calMin + 1, calMax - 1);
      pulse.setThreshold(threshold);
      goodBeats = 0;
      bpmValid = false;
      lastBeat = pulseWatchStart = now;
      message(F("INFO,PULSE_CALIBRATED,Wait_for_stable_beats"));
      Serial.print(F("Pulse starting threshold: "));
      Serial.println(threshold);
    } else {
      message(F("ALERT,PULSE_CAL_FAILED,Adjust_finger_then_send_C"));
    }
  }
  if (pulse.sawStartOfBeat() && calibrated && !calibrating && signalOK) {
    int measured = pulse.getBeatsPerMinute();
    // Plausibility screen only; this does NOT prove a real heartbeat.
    if (measured >= 35 && measured <= 220) {
      lastBeat = now;
      bpm = measured;
      if (goodBeats < 4) ++goodBeats;
      bpmValid = goodBeats >= 4;
    } else { bpmValid = false; goodBeats = 0; }
  }
  if (now - lastBeat > NO_BEAT_MS) { bpmValid = false; goodBeats = 0; }
  if (calibrated && !calibrating) {
    if (bpmValid) {
      pulseWatchStart = now;
      if (pulseMissingAlert) message(F("INFO,PULSE_SIGNAL_RESTORED"));
      pulseMissingAlert = false;
    } else if (!pulseMissingAlert && now - pulseWatchStart >= 10000) {
      pulseMissingAlert = true;
      message(F("ALERT,PULSE_UNAVAILABLE,Check_finger_and_sensor"));
    }
  }
}

void updateTemperature(unsigned long now) {
  // 12-bit DS18B20 conversion: allow at least 750 ms. Read every 1 second.
  if (now - tempRequested < 1000) return;
  float value = temperature.getTempCByIndex(0);
  // 85 C is the power-on scratchpad value: conservatively reject for this project.
  tempValid = value != DEVICE_DISCONNECTED_C && value >= -55.0 &&
              value <= 125.0 && value != 85.0;
  if (tempValid) {
    tempC = value;
    if (tempFault) message(F("INFO,TEMPERATURE_SENSOR_RESTORED"));
    tempFault = false;
  } else if (!tempFault) {
    tempFault = true;
    message(F("ALERT,TEMPERATURE_UNAVAILABLE,Check_DS18B20"));
  }
  temperature.requestTemperatures();
  tempRequested = millis();
}

void updateTilt(unsigned long now) {
  byte raw = digitalRead(TILT_PIN);
  if (raw != tiltRaw) { tiltRaw = raw; tiltChanged = now; }
  if (raw != tiltStable && now - tiltChanged >= 100) {
    tiltStable = raw;
    postureSince = now;
  }
  bool tilted = tiltStable != TILT_NORMAL_STATE;
  if (!fallActive && tilted && now - postureSince >= FALL_CONFIRM_MS) {
    fallActive = true;
    lastFallAlert = now;
    message(F("ALERT,POSSIBLE_FALL,Sustained_tilt_detected"));
  }
  if (fallActive && !tilted && now - postureSince >= FALL_RECOVER_MS) {
    fallActive = false;
    message(F("INFO,UPRIGHT_RESTORED"));
  }
  if (fallActive && now - lastFallAlert >= FALL_REPEAT_MS) {
    lastFallAlert = now;
    message(F("ALERT,POSSIBLE_FALL,Please_check_person"));
  }
}

void updateLimits(unsigned long now) {
  if (!ENABLE_LIMIT_ALERTS) return;
  bool outside = bpmValid && (bpm < LOW_BPM || bpm > HIGH_BPM);
  if (outside) {
    if (!hrPending) { hrPending = true; hrSince = now; }
    if (!hrAlarm && now - hrSince >= LIMIT_HOLD_MS) {
      hrAlarm = true;
      message(F("ALERT,HEART_RATE_OUTSIDE_CONFIGURED_LIMITS"));
    }
  } else {
    hrPending = false;
    if (hrAlarm && bpmValid) message(F("INFO,HEART_RATE_WITHIN_LIMITS"));
    hrAlarm = false;
  }
  bool hot = tempValid && tempC >= HIGH_TEMP_C;
  if (hot) {
    if (!hotPending) { hotPending = true; hotSince = now; }
    if (!hotAlarm && now - hotSince >= LIMIT_HOLD_MS) {
      hotAlarm = true;
      message(F("ALERT,TEMPERATURE_ABOVE_CONFIGURED_LIMIT"));
    }
  } else {
    hotPending = false;
    if (hotAlarm && tempValid) message(F("INFO,TEMPERATURE_BELOW_LIMIT"));
    hotAlarm = false;
  }
}

void sendData(Print &out) {
  out.print(F("DATA,BPM="));
  if (bpmValid) out.print(bpm); else out.print(F("NA"));
  out.print(F(",TEMP_C="));
  if (tempValid) out.print(tempC, 1); else out.print(F("NA"));
  out.print(F(",TILT_RAW=")); out.print(tiltStable);
  out.print(F(",TILTED=")); out.print(tiltStable != TILT_NORMAL_STATE);
  out.print(F(",POSSIBLE_FALL=")); out.print(fallActive);
  out.print(F(",PANIC=")); out.print(panicActive);
  out.print(F(",CAL="));
  if (calibrating) out.println(F("RUNNING"));
  else if (calibrated) out.println(F("OK"));
  else out.println(F("NEEDED"));
}

void updateLCD(unsigned long now) {
  if (now - lastLCD < 500) return;
  lastLCD = now;
  char line[17], tempText[9];
  if (tempValid) dtostrf(tempC, 5, 1, tempText);
  else strcpy(tempText, " --.-");
  if (bpmValid) snprintf(line, sizeof(line), "HR:%3d T:%sC ", bpm, tempText);
  else snprintf(line, sizeof(line), "HR:--- T:%sC ", tempText);
  lcd.setCursor(0, 0); lcd.print(line);
  lcd.setCursor(0, 1);
  if (panicActive) lcd.print(F("PANIC ALERT     "));
  else if (fallActive) lcd.print(F("POSSIBLE FALL!  "));
  else if (calibrating) lcd.print(F("Cal: hold finger"));
  else if (!pulseStarted) lcd.print(F("Pulse init error"));
  else if (!calibrated) lcd.print(F("Send C to calib "));
  else if (tiltStable != TILT_NORMAL_STATE) lcd.print(F("Tilt:CHECK      "));
  else if (hrAlarm) lcd.print(F("HR LIMIT ALERT  "));
  else if (hotAlarm) lcd.print(F("TEMP LIMIT ALERT"));
  else if (!tempValid) lcd.print(F("Temperature ERR "));
  else if (!bpmValid) lcd.print(F("Tilt:OK HR:WAIT "));
  else lcd.print(F("Tilt:OK         "));
}

void updateControls(unsigned long now) {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'C' || c == 'c') startCalibration();
  }
  if (bluetooth.available()) {
    char c = bluetooth.read();
    if (c == 'C' || c == 'c') startCalibration();
  }
  bool raw = digitalRead(CAL_BUTTON_PIN);
  if (raw != buttonRaw) { buttonRaw = raw; buttonChanged = now; }
  if (raw != buttonStable && now - buttonChanged >= 40) {
    buttonStable = raw;
    if (raw == LOW) startCalibration();
  }
}

void setup() {
  Serial.begin(115200);
  bluetooth.begin(9600);
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(CAL_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
  tiltRaw = tiltStable = digitalRead(TILT_PIN);
  tiltChanged = postureSince = millis();
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print(F("HealthGuard Nano"));
  lcd.setCursor(0, 1); lcd.print(F("Starting...     "));
  temperature.begin();
  temperature.setResolution(12);
  temperature.setWaitForConversion(false);
  temperature.requestTemperatures();
  tempRequested = millis();
  pulse.analogInput(PULSE_PIN);
  pulse.blinkOnPulse(LED_BUILTIN);
  pulse.setThreshold(threshold);
  pulseStarted = pulse.begin();
  message(F("INFO,HealthGuard_Nano,Send_C_to_calibrate"));
  if (pulseStarted) startCalibration();
  else message(F("ALERT,PULSE_INITIALIZATION_FAILED"));
}

void loop() {
  unsigned long now = millis();
  updatePanic(now);
  now = millis(); // panic alert output may take a few milliseconds
  updateControls(now);
  now = millis(); // calibration may have emitted serial output
  updateTilt(now);
  updatePulse(now);
  updateTemperature(now);
  updateLimits(now);
  updateLCD(now);
  if (now - lastData >= 2000) {
    lastData = now;
    sendData(Serial);
    sendData(bluetooth);
  }
}

#endif
