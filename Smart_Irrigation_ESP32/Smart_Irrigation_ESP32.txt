#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "TMPL39Z2PiPuK"
#define BLYNK_TEMPLATE_NAME "Smart Agriculture"
#define BLYNK_AUTH_TOKEN "C5k7J7LTV6upskBGKzxyYnb9eGgw5MHs"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <RTClib.h>

// WiFi credentials
char ssid[] = "AutoIrrigation";
char pass[] = "2024240241";

// RTC setup
RTC_DS3231 rtc;

// Virtual pins for solenoids, pumps, and timers
#define VP_MANUAL_MODE V0
#define VP_SOLENOID1 V1
#define VP_SOLENOID2 V2
#define VP_SOLENOID3 V3
#define VP_PUMP1 V4
#define VP_EMERGENCY_STOP V5  // Emergency Stop Button
#define VP_TIMER_PLOT1 V6
#define VP_TIMER_PLOT2 V7
#define VP_TIMER_FERT V8

// Relay control pins (active LOW)
#define RELAY_SOLENOID1_PIN 32
#define RELAY_SOLENOID2_PIN 33
#define RELAY_SOLENOID3_PIN 26
#define RELAY_PUMP1_PIN 27
#define RELAY_PUMP2_PIN 4

// State variables for solenoids, pumps, and emergency stop
bool manualMode = false;
bool emergencyStop = false;
bool solenoid1State = false, solenoid2State = false, solenoid3State = false;
bool pump1State = false, pump2State = false;

// Timer variables for solenoids
int startHourPlot1 = -1, startMinutePlot1 = -1, stopHourPlot1 = -1, stopMinutePlot1 = -1, weekdaysPlot1 = 0;
int startHourPlot2 = -1, startMinutePlot2 = -1, stopHourPlot2 = -1, stopMinutePlot2 = -1, weekdaysPlot2 = 0;
int startHourFert = -1, startMinuteFert = -1, stopHourFert = -1, stopMinuteFert = -1, weekdaysFert = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Setup started.");

  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk initialized.");

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  } else {
    Serial.println("RTC initialized.");
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Initialize relay control pins
  pinMode(RELAY_SOLENOID1_PIN, OUTPUT);
  digitalWrite(RELAY_SOLENOID1_PIN, HIGH);
  pinMode(RELAY_SOLENOID2_PIN, OUTPUT);
  digitalWrite(RELAY_SOLENOID2_PIN, HIGH);
  pinMode(RELAY_SOLENOID3_PIN, OUTPUT);
  digitalWrite(RELAY_SOLENOID3_PIN, HIGH);
  pinMode(RELAY_PUMP1_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP1_PIN, HIGH);
  pinMode(RELAY_PUMP2_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP2_PIN, HIGH);
}

void loop() {
  Blynk.run();

  if (emergencyStop) {
    disableAllRelays();
    Serial.println("Emergency stop activated. All relays OFF.");
    delay(500);
    return;
  }

  if (!manualMode) {
    checkTimers();
  } else {
    Serial.println("Manual mode active; timers not checked.");
  }

  delay(200);
}

// Toggle Manual/Auto Mode
BLYNK_WRITE(VP_MANUAL_MODE) {
  manualMode = param.asInt();
  Serial.print("Manual mode changed: ");
  Serial.println(manualMode ? "ON" : "OFF");
}

// Emergency Stop Control on V5
BLYNK_WRITE(VP_EMERGENCY_STOP) {
  emergencyStop = param.asInt();
  Serial.print("Emergency stop changed: ");
  Serial.println(emergencyStop ? "ACTIVATED" : "DEACTIVATED");
  if (emergencyStop) {
    disableAllRelays();
  }
}

// Save Plot1 Timer
BLYNK_WRITE(VP_TIMER_PLOT1) {
  TimeInputParam t(param);
  Serial.println("Received Plot1 Timer update.");
  if (t.hasStartTime() && t.hasStopTime()) {
    startHourPlot1 = t.getStartHour();
    startMinutePlot1 = t.getStartMinute();
    stopHourPlot1 = t.getStopHour();
    stopMinutePlot1 = t.getStopMinute();

    weekdaysPlot1 = 0;
    for (int i = 1; i <= 7; i++) {
      if (t.isWeekdaySelected(i)) {
        weekdaysPlot1 |= (1 << (i - 1));
      }
    }

    Serial.println("Plot1 Timer Updated.");
    printTimerDetails(startHourPlot1, startMinutePlot1, stopHourPlot1, stopMinutePlot1, weekdaysPlot1);
  } else {
    Serial.println("Invalid Plot1 Timer update received.");
  }
}

// Save Plot2 Timer
BLYNK_WRITE(VP_TIMER_PLOT2) {
  TimeInputParam t(param);
  Serial.println("Received Plot2 Timer update.");
  if (t.hasStartTime() && t.hasStopTime()) {
    startHourPlot2 = t.getStartHour();
    startMinutePlot2 = t.getStartMinute();
    stopHourPlot2 = t.getStopHour();
    stopMinutePlot2 = t.getStopMinute();

    weekdaysPlot2 = 0;
    for (int i = 1; i <= 7; i++) {
      if (t.isWeekdaySelected(i)) {
        weekdaysPlot2 |= (1 << (i - 1));
      }
    }

    Serial.println("Plot2 Timer Updated.");
    printTimerDetails(startHourPlot2, startMinutePlot2, stopHourPlot2, stopMinutePlot2, weekdaysPlot2);
  } else {
    Serial.println("Invalid Plot2 Timer update received.");
  }
}

// Save Fertigation Timer
BLYNK_WRITE(VP_TIMER_FERT) {
  TimeInputParam t(param);
  Serial.println("Received Fertigation Timer update.");
  if (t.hasStartTime() && t.hasStopTime()) {
    startHourFert = t.getStartHour();
    startMinuteFert = t.getStartMinute();
    stopHourFert = t.getStopHour();
    stopMinuteFert = t.getStopMinute();

    weekdaysFert = 0;
    for (int i = 1; i <= 7; i++) {
      if (t.isWeekdaySelected(i)) {
        weekdaysFert |= (1 << (i - 1));
      }
    }

    Serial.println("Fertigation Timer Updated.");
    printTimerDetails(startHourFert, startMinuteFert, stopHourFert, stopMinuteFert, weekdaysFert);
  } else {
    Serial.println("Invalid Fertigation Timer update received.");
  }
}

// Control Solenoids and Pumps from Blynk in Manual Mode
BLYNK_WRITE(VP_SOLENOID1) {
  if (emergencyStop) return;
  int value = param.asInt();
  Serial.print("Received Solenoid 1 command: ");
  Serial.println(value);
  if (manualMode) {
    solenoid1State = value;
    updateRelayOutputs();
  }
}
BLYNK_WRITE(VP_SOLENOID2) {
  if (emergencyStop) return;
  int value = param.asInt();
  Serial.print("Received Solenoid 2 command: ");
  Serial.println(value);
  if (manualMode) {
    solenoid2State = value;
    updateRelayOutputs();
  }
}
BLYNK_WRITE(VP_SOLENOID3) {
  if (emergencyStop) return;
  int value = param.asInt();
  Serial.print("Received Solenoid 3 command: ");
  Serial.println(value);
  if (manualMode) {
    solenoid3State = value;
    updateRelayOutputs();
  }
}
BLYNK_WRITE(VP_PUMP1) {
  if (emergencyStop) return;
  int value = param.asInt();
  Serial.print("Received Pump 1 command: ");
  Serial.println(value);
  if (manualMode) {
    pump1State = value;
    updateRelayOutputs();
  }
}
BLYNK_WRITE(VP_PUMP2) {
  if (emergencyStop) return;
  int value = param.asInt();
  Serial.print("Received Pump 2 command: ");
  Serial.println(value);
  if (manualMode) {
    pump2State = value;
    updateRelayOutputs();
  }
}

// Print Timer Details
void printTimerDetails(int startHour, int startMinute, int stopHour, int stopMinute, int weekdays) {
  Serial.print("Start Time: ");
  Serial.print(startHour);
  Serial.print(":");
  Serial.println(startMinute);

  Serial.print("Stop Time: ");
  Serial.print(stopHour);
  Serial.print(":");
  Serial.println(stopMinute);

  Serial.print("Selected Days: ");
  for (int i = 0; i < 7; i++) {
    if (weekdays & (1 << i)) {
      Serial.print(String(i + 1) + " ");
    }
  }
  Serial.println();
}

// Update Relay Outputs
void updateRelayOutputs() {
  digitalWrite(RELAY_SOLENOID1_PIN, solenoid1State ? LOW : HIGH);
  digitalWrite(RELAY_SOLENOID2_PIN, solenoid2State ? LOW : HIGH);
  digitalWrite(RELAY_SOLENOID3_PIN, solenoid3State ? LOW : HIGH);
  digitalWrite(RELAY_PUMP1_PIN, solenoid3State ? LOW : HIGH);
  digitalWrite(RELAY_PUMP2_PIN, pump2State ? LOW : HIGH);
  Blynk.virtualWrite(VP_SOLENOID1,solenoid1State);
  Blynk.virtualWrite(VP_SOLENOID2,solenoid2State);
  Blynk.virtualWrite(VP_SOLENOID3,solenoid3State);
  Serial.println("Relay outputs updated.");
}

// Disable All Relays
void disableAllRelays() {
  digitalWrite(RELAY_SOLENOID1_PIN, HIGH);
  digitalWrite(RELAY_SOLENOID2_PIN, HIGH);
  digitalWrite(RELAY_SOLENOID3_PIN, HIGH);
  digitalWrite(RELAY_PUMP1_PIN, HIGH);
  digitalWrite(RELAY_PUMP2_PIN, HIGH);

  solenoid1State = false;
  solenoid2State = false;
  solenoid3State = false;
  pump1State = false;
  pump2State = false;

  Serial.println("All relays set to OFF.");
}

// Check Timers
void checkTimers() {
  DateTime now = rtc.now();
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentTime = currentHour * 60 + currentMinute;  // Current time in minutes
  int today = now.dayOfTheWeek();

  bool plot1Active = checkSingleTimer(startHourPlot1, startMinutePlot1, stopHourPlot1, stopMinutePlot1, weekdaysPlot1, today, currentTime);
  if (plot1Active != solenoid1State) {
    solenoid1State = plot1Active;
    updateRelayOutputs();
  }

  bool plot2Active = checkSingleTimer(startHourPlot2, startMinutePlot2, stopHourPlot2, stopMinutePlot2, weekdaysPlot2, today, currentTime);
  if (plot2Active != solenoid2State) {
    solenoid2State = plot2Active;
    updateRelayOutputs();
  }

  bool fertActive = checkSingleTimer(startHourFert, startMinuteFert, stopHourFert, stopMinuteFert, weekdaysFert, today, currentTime);
  if (fertActive != solenoid3State) {
    solenoid3State = fertActive;
    updateRelayOutputs();
  }
}

// Check Single Timer
bool checkSingleTimer(int startHour, int startMinute, int stopHour, int stopMinute, int weekdays, int today, int currentTime) {
  if (startHour == -1 || stopHour == -1) {
    return false;
  }

  int startTime = startHour * 60 + startMinute;
  int stopTime = stopHour * 60 + stopMinute;

  bool isActiveDay = weekdays & (1 << today);
  bool isActiveTime = currentTime >= startTime && currentTime <= stopTime;

  return isActiveDay && isActiveTime;
}
