/* =======================================================================
   Alpha Electronz | alphaelectronz.tech
   Sketch: MQ-2 Gas Detector (Digital D0)          Version: v1.0
   Board:  Arduino Nano/Uno                        Date: 2025-09-05
   Purpose: Use MQ-2 module’s digital output to drive RED/GREEN LEDs + buzzer.
   ======================================================================= */

const int MQ_DO  = 9;   // MQ-2 module D0 pin (digital comparator output)
const int BUZZ   = 11;
const int LED_R  = 13;
const int LED_G  = 12;

// Set this depending on your module's logic:
// Many LM393 boards pull DO LOW when GAS is detected (active-low).
const bool DO_ACTIVE_LOW = true;

void setup() {
  pinMode(MQ_DO, INPUT);        // module usually has its own pull-up/down
  pinMode(BUZZ, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, LOW);
  Serial.begin(9600);
  Serial.println("MQ-2 (digital) ready. Preheat sensor ~1–2 minutes for stability.");
}

void loop() {
  int doVal = digitalRead(MQ_DO);
  bool gas = (doVal == (DO_ACTIVE_LOW ? LOW : HIGH));

  digitalWrite(LED_G, !gas);
  digitalWrite(LED_R, gas);

  // Beep pattern when gas detected: 200 ms ON every 600 ms
  if (gas) {
    digitalWrite(BUZZ, HIGH);
  } else {
    digitalWrite(BUZZ, LOW);
  }

  Serial.print("DO="); Serial.print(doVal);
  Serial.print("  GAS="); Serial.println(gas ? "YES" : "NO");
  delay(50);
}
