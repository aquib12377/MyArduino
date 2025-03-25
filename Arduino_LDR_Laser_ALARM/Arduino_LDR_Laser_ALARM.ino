/************************************************
 * Detect if a laser beam is interrupted using
 * a digital-output LDR module and beep a buzzer.
 ************************************************/

// --- Pin Assignments ---
const int LDR_PIN    = 2;  // Digital input pin to read from LDR module's DO
const int BUZZER_PIN = 3;  // Digital output pin connected to active buzzer

void setup() {
  Serial.begin(9600);

  pinMode(LDR_PIN, INPUT_PULLUP);   // or INPUT_PULLUP if your module is open-collector
  pinMode(BUZZER_PIN, OUTPUT);

  // Optionally beep at startup
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("LDR Module Laser Interrupt Test");
  Serial.println("Waiting for beam interruptions...");
}

void loop() {
  // Read digital output from LDR module
  int ldrState = digitalRead(LDR_PIN);

  // Typically, the module outputs:
  //   HIGH when there's enough light (laser not interrupted)
  //   LOW  when the light is blocked (laser interrupted)
  // Some modules might invert this logic, so you may need to flip if conditions.

  if (ldrState == HIGH) {
    // Laser beam is likely blocked -> beep the buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("Beam interrupted!");
  } else {
    // Beam intact -> no beep
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(100);  // Poll every 100ms (adjust as needed)
}
