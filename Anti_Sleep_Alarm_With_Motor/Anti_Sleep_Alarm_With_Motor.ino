/*
  Anti-Sleep Driver Project

  Components:
    - IR Sensor (Active LOW): outputs LOW when eyes are closed.
    - Buzzer: activated when the driver is drowsy.
    - Relay controlling motor (Active LOW): motor is ON when relay output is LOW.

  Behavior:
    - If the IR sensor indicates eyes closed for more than 3 seconds,
      the buzzer is turned on continuously.
    - If eyes remain closed for more than 8 seconds,
      the buzzer remains on and the relay is switched off (motor off).
    - When eyes are open (IR sensor HIGH), everything resets.
*/

const int IR_PIN = 2;       // IR sensor digital pin
const int BUZZER_PIN = 4;   // Buzzer digital pin
const int RELAY_PIN = 3;    // Relay digital pin controlling motor

unsigned long closedStartTime = 0;  // Time when eyes were first detected as closed

void setup() {
  // IR sensor is active LOW so using the internal pull-up resistor is useful.
  pinMode(IR_PIN, INPUT_PULLUP);
  
  // Set the buzzer and relay pins as outputs.
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Ensure motor is running initially (relay active LOW means motor ON).
  digitalWrite(RELAY_PIN, LOW);
  
  // Ensure buzzer is off at startup.
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.begin(9600);
}

void loop() {
  // Read the IR sensor. LOW means eyes closed, HIGH means eyes open.
  int sensorValue = digitalRead(IR_PIN);

  if (sensorValue == LOW) {
    // Eyes are open: reset everything.
    if (closedStartTime != 0) {
      Serial.println("Eyes opened. Resetting timer and restoring motor.");
    }
    closedStartTime = 0;
    digitalWrite(BUZZER_PIN, LOW);    // Turn off buzzer.
    digitalWrite(RELAY_PIN, LOW);       // Ensure motor is on.
  } 
  else { // sensorValue == LOW => Eyes closed.
    // If this is the first time reading closed eyes, record the time.
    if (closedStartTime == 0) {
      closedStartTime = millis();
      Serial.println("Eyes closed detected. Starting timer.");
    }
    
    // Calculate how long the eyes have been closed.
    unsigned long elapsed = millis() - closedStartTime;
    
    // After 3 seconds of closed eyes, activate the buzzer.
    if (elapsed >= 3000) {
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.print("Eyes closed for ");
      Serial.print(elapsed);
      Serial.println(" ms: buzzer ON.");
    }
    
    // After 8 seconds, turn OFF the motor by switching the relay.
    if (elapsed >= 8000) {
      digitalWrite(RELAY_PIN, HIGH);  // Motor off (since relay is active LOW).
      Serial.print("Eyes closed for ");
      Serial.print(elapsed);
      Serial.println(" ms: motor OFF.");
    }
  }
  
  delay(50); // Short delay to stabilize the loop.
}
