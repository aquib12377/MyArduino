/*****************************************************
 * Blynk / Hardware Setup
 *****************************************************/

// Enable debug prints
#define BLYNK_PRINT Serial

// Blynk Template Info
#define BLYNK_TEMPLATE_ID   "TMPLXXXXXXX"
#define BLYNK_TEMPLATE_NAME "Smart Agriculture"
#define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN_HERE"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <RTClib.h>

// WiFi credentials
char ssid[] = "YOUR_SSID";
char pass[] = "YOUR_PASSWORD";

// RTC setup
RTC_DS3231 rtc;

/*****************************************************
 * Virtual Pins
 *****************************************************/
// We’ll define them for clarity; you may adjust as needed.
#define VP_MANUAL_MODE        V0  // Switch: Auto(0)/Manual(1)
#define VP_EMERGENCY_STOP     V1  // Switch: OFF(0)/ON(1)

// Timers
#define VP_TIMER_PLOT1        V2
#define VP_TIMER_PLOT2        V3
#define VP_TIMER_FERT         V4

// Manual Buttons
#define VP_MANUAL_PLOT1       V5
#define VP_MANUAL_PLOT2       V6
#define VP_MANUAL_FERT        V7

// (Optional) Show states of S1, S2, S3, etc. in the Blynk app
#define VP_SOLENOID1_STATE    V8
#define VP_SOLENOID2_STATE    V9
#define VP_SOLENOID3_STATE    V10
#define VP_PUMP1_STATE        V11
#define VP_PUMP2_STATE        V12


/*****************************************************
 * Pin Assignments (Active LOW Relays)
 *****************************************************/
//   Pin 14 → Solenoid 1 (S1)
//   Pin 32 → Solenoid 2 (S2)
//   Pin 33 → Solenoid 3 (S3)
//   Pin 26 → Pump 1 (P1)
//   Pin 27 → Pump 2 (P2)

#define RELAY_SOLENOID1_PIN 14
#define RELAY_SOLENOID2_PIN 32
#define RELAY_SOLENOID3_PIN 33
#define RELAY_PUMP1_PIN     26
#define RELAY_PUMP2_PIN     27

/*****************************************************
 * Global State Variables
 *****************************************************/

// MODE
bool manualMode      = false; 
bool emergencyStop   = false;

// AUTO MODE Timer States
// (Used internally to see if each timer is active at this moment)
bool plot1Active     = false;
bool plot2Active     = false;
bool fertActive      = false;

// MANUAL MODE States (the user toggles these in manual mode)
bool manualPlot1Active = false; // Means "Plot1" button is pressed
bool manualPlot2Active = false; // Means "Plot2" button is pressed
bool manualFertActive  = false; // Means "Fertigation" button is pressed

// Relay states that actually drive the pins
bool solenoid1State  = false;  // S1
bool solenoid2State  = false;  // S2
bool solenoid3State  = false;  // S3
bool pump1State      = false;  // P1
bool pump2State      = false;  // P2

// Timer settings
int startHourPlot1   = -1, startMinutePlot1   = -1, stopHourPlot1   = -1, stopMinutePlot1   = -1, weekdaysPlot1   = 0;
int startHourPlot2   = -1, startMinutePlot2   = -1, stopHourPlot2   = -1, stopMinutePlot2   = -1, weekdaysPlot2   = 0;
int startHourFert    = -1, startMinuteFert    = -1, stopHourFert    = -1, stopMinuteFert    = -1, weekdaysFert    = 0;

/*****************************************************
 * Setup
 *****************************************************/
void setup() {
  Serial.begin(115200);
  Serial.println("Setup started.");

  // Initialize Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Blynk initialized.");

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  } 
  Serial.println("RTC initialized.");
  if (rtc.lostPower()) {
    Serial.println("RTC lost power; setting to compile time.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialize relay pins (Active LOW)
  pinMode(RELAY_SOLENOID1_PIN, OUTPUT); digitalWrite(RELAY_SOLENOID1_PIN, HIGH);
  pinMode(RELAY_SOLENOID2_PIN, OUTPUT); digitalWrite(RELAY_SOLENOID2_PIN, HIGH);
  pinMode(RELAY_SOLENOID3_PIN, OUTPUT); digitalWrite(RELAY_SOLENOID3_PIN, HIGH);
  pinMode(RELAY_PUMP1_PIN, OUTPUT);     digitalWrite(RELAY_PUMP1_PIN, HIGH);
  pinMode(RELAY_PUMP2_PIN, OUTPUT);     digitalWrite(RELAY_PUMP2_PIN, HIGH);

  // Print current time for sanity check
  DateTime now = rtc.now();
  Serial.print("Current Date/Time: ");
  Serial.print(now.year()); Serial.print("/");
  Serial.print(now.month()); Serial.print("/");
  Serial.print(now.day());   Serial.print(" ");
  Serial.print(now.hour());  Serial.print(":");
  Serial.print(now.minute());Serial.print(":");
  Serial.println(now.second());

  Serial.println("Setup complete.\n");
}

/*****************************************************
 * Main Loop
 *****************************************************/
void loop() {
  Blynk.run();

  // If emergency stop is active → kill everything & skip timers
  if (emergencyStop) {
    disableAllRelays();
    delay(500);
    return;
  }

  // If in auto mode, we check timers to set states
  if (!manualMode) {
    checkTimers();
  } 
  // If in manual mode, the states are determined by the user’s manual toggles;
  // checkTimers() is ignored in that scenario.

  // Let’s update the final relay states
  updateRelayOutputs();

  delay(200);  // Avoid flooding the serial
}

/*****************************************************
 * BLYNK WRITE HANDLERS
 *****************************************************/

// Manual/Auto Mode Toggle
BLYNK_WRITE(VP_MANUAL_MODE) {
  manualMode = param.asInt();  // 0 or 1
  Serial.print("Manual mode changed: ");
  Serial.println(manualMode ? "ON" : "OFF");

  // Clear everything whenever we switch modes
  plot1Active       = false;
  plot2Active       = false;
  fertActive        = false;
  manualPlot1Active = false;
  manualPlot2Active = false;
  manualFertActive  = false;
  solenoid1State    = false;
  solenoid2State    = false;
  solenoid3State    = false;
  pump1State        = false;
  pump2State        = false;
  updateRelayOutputs();
}

// Emergency Stop
BLYNK_WRITE(VP_EMERGENCY_STOP) {
  emergencyStop = param.asInt();
  Serial.print("Emergency stop changed: ");
  Serial.println(emergencyStop ? "ACTIVATED" : "DEACTIVATED");
  if (emergencyStop) {
    disableAllRelays();
  }
}

// ---------- Timers ---------- //
// Plot1 Timer
BLYNK_WRITE(VP_TIMER_PLOT1) {
  TimeInputParam t(param);
  Serial.println("Received Plot1 Timer update.");
  if (t.hasStartTime() && t.hasStopTime()) {
    startHourPlot1   = t.getStartHour();
    startMinutePlot1 = t.getStartMinute();
    stopHourPlot1    = t.getStopHour();
    stopMinutePlot1  = t.getStopMinute();

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

// Plot2 Timer
BLYNK_WRITE(VP_TIMER_PLOT2) {
  TimeInputParam t(param);
  Serial.println("Received Plot2 Timer update.");
  if (t.hasStartTime() && t.hasStopTime()) {
    startHourPlot2   = t.getStartHour();
    startMinutePlot2 = t.getStartMinute();
    stopHourPlot2    = t.getStopHour();
    stopMinutePlot2  = t.getStopMinute();

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

// Fertigation Timer
BLYNK_WRITE(VP_TIMER_FERT) {
  TimeInputParam t(param);
  Serial.println("Received Fertigation Timer update.");
  if (t.hasStartTime() && t.hasStopTime()) {
    startHourFert   = t.getStartHour();
    startMinuteFert = t.getStartMinute();
    stopHourFert    = t.getStopHour();
    stopMinuteFert  = t.getStopMinute();

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

// ---------- Manual Mode Buttons ---------- //
// Plot1 Button
BLYNK_WRITE(VP_MANUAL_PLOT1) {
  if (emergencyStop || !manualMode) return;  // Only process if Manual Mode & no E-Stop
  int value = param.asInt(); // 0 or 1
  manualPlot1Active = (value == 1);
  Serial.printf("[Manual] Plot1 toggle: %s\n", manualPlot1Active ? "ON" : "OFF");
}

// Plot2 Button
BLYNK_WRITE(VP_MANUAL_PLOT2) {
  if (emergencyStop || !manualMode) return;
  int value = param.asInt();
  manualPlot2Active = (value == 1);
  Serial.printf("[Manual] Plot2 toggle: %s\n", manualPlot2Active ? "ON" : "OFF");
}

// Fertigation Button
BLYNK_WRITE(VP_MANUAL_FERT) {
  if (emergencyStop || !manualMode) return;
  int value = param.asInt();
  manualFertActive = (value == 1);
  Serial.printf("[Manual] Fert toggle: %s\n", manualFertActive ? "ON" : "OFF");
}

/*****************************************************
 * Helper: Print Timer Info
 *****************************************************/
void printTimerDetails(int startHour, int startMinute, int stopHour, int stopMinute, int weekdays) {
  Serial.print("Start Time: "); Serial.print(startHour);   Serial.print(":"); Serial.println(startMinute);
  Serial.print("Stop Time:  "); Serial.print(stopHour);    Serial.print(":"); Serial.println(stopMinute);
  Serial.print("WeekdayMask: "); Serial.println(weekdays, BIN);
  Serial.println("--------------------------");
}

/*****************************************************
 * Update Relay Outputs (Active LOW)
 *****************************************************/
void updateRelayOutputs() {
  /*****************************************************
   * If in AUTO MODE, final states = depends on timers
   * If in MANUAL MODE, final states = depends on manual toggles
   *****************************************************/
  if (manualMode) {
    // If Manual Mode → set states based on the manual toggles
    pump1State     = (manualPlot1Active || manualPlot2Active);
    pump2State     = manualFertActive;
    solenoid1State = manualPlot1Active;
    solenoid2State = manualPlot2Active;
    solenoid3State = manualFertActive;
  } else {
    // If Auto Mode → set states based on the timer checks
    pump1State     = (plot1Active || plot2Active);
    pump2State     = fertActive;
    solenoid1State = plot1Active;
    solenoid2State = plot2Active;
    solenoid3State = fertActive;
  }

  // Write final states to the physical pins (ACTIVE LOW)
  digitalWrite(RELAY_SOLENOID1_PIN, solenoid1State ? LOW : HIGH);
  digitalWrite(RELAY_SOLENOID2_PIN, solenoid2State ? LOW : HIGH);
  digitalWrite(RELAY_SOLENOID3_PIN, solenoid3State ? LOW : HIGH);
  digitalWrite(RELAY_PUMP1_PIN,     pump1State     ? LOW : HIGH);
  digitalWrite(RELAY_PUMP2_PIN,     pump2State     ? LOW : HIGH);

  // For debugging
  Serial.printf("Relays => S1=%d  S2=%d  S3=%d  P1=%d  P2=%d\n",
                solenoid1State, solenoid2State, solenoid3State,
                pump1State, pump2State);

  // Update Blynk widgets so you see actual states
  Blynk.virtualWrite(VP_SOLENOID1_STATE, solenoid1State);
  Blynk.virtualWrite(VP_SOLENOID2_STATE, solenoid2State);
  Blynk.virtualWrite(VP_SOLENOID3_STATE, solenoid3State);
  Blynk.virtualWrite(VP_PUMP1_STATE,     pump1State);
  Blynk.virtualWrite(VP_PUMP2_STATE,     pump2State);
}

/*****************************************************
 * Disable ALL Relays
 *****************************************************/
void disableAllRelays() {
  digitalWrite(RELAY_SOLENOID1_PIN, HIGH);
  digitalWrite(RELAY_SOLENOID2_PIN, HIGH);
  digitalWrite(RELAY_SOLENOID3_PIN, HIGH);
  digitalWrite(RELAY_PUMP1_PIN,     HIGH);
  digitalWrite(RELAY_PUMP2_PIN,     HIGH);

  solenoid1State = false;
  solenoid2State = false;
  solenoid3State = false;
  pump1State     = false;
  pump2State     = false;

  Serial.println("All relays set to OFF (Emergency Stop or forced OFF).");
}

/*****************************************************
 * Timer Checks
 *****************************************************/
void checkTimers() {
  // Called only in Auto Mode
  DateTime now   = rtc.now();
  int today      = now.dayOfTheWeek();  // Sunday=0, Monday=1, ...
  int hourNow    = now.hour();
  int minuteNow  = now.minute();
  int currentMin = hourNow * 60 + minuteNow;

  // Evaluate each timer
  plot1Active = checkSingleTimer(startHourPlot1, startMinutePlot1, 
                                 stopHourPlot1,  stopMinutePlot1, 
                                 weekdaysPlot1,  today, currentMin);

  plot2Active = checkSingleTimer(startHourPlot2, startMinutePlot2, 
                                 stopHourPlot2,  stopMinutePlot2, 
                                 weekdaysPlot2,  today, currentMin);

  fertActive  = checkSingleTimer(startHourFert,  startMinuteFert, 
                                 stopHourFert,   stopMinuteFert, 
                                 weekdaysFert,   today, currentMin);

  // The final pin states will be set in updateRelayOutputs()
}

/*****************************************************
 * Check Single Timer
 *****************************************************/
bool checkSingleTimer(int startHour, int startMinute,
                      int stopHour,  int stopMinute,
                      int weekdays,  int today,
                      int currentMin) 
{
  // If the timer is not set up properly
  if (startHour < 0 || stopHour < 0) {
    return false;
  }

  // Convert times to “minutes from midnight”
  int startTime = startHour * 60 + startMinute;
  int stopTime  = stopHour  * 60 + stopMinute;

  // Does “weekdays” bitmask match today?
  // dayOfTheWeek() = 0..6 => Sunday..Saturday
  // so if bit #0 corresponds to Sunday, bit #1 to Monday, etc.:
  bool isActiveDay = (weekdays & (1 << today));

  bool isActiveTime = false;
  if (stopTime >= startTime) {
    // Normal same-day schedule
    isActiveTime = (currentMin >= startTime && currentMin <= stopTime);
  } else {
    // If stopTime < startTime, it means it crosses midnight
    // e.g. start=22:00, stop=02:00 next day
    // We consider it active if currentTime >= startTime OR <= stopTime
    isActiveTime = (currentMin >= startTime || currentMin <= stopTime);
  }

  return (isActiveDay && isActiveTime);
}
