/* --------------------------------------------------------
   Simple Traffic‑Light Demo
   Red  – STOP      (5 s)
   Green – GO       (5 s)
   Yellow – READY   (2 s)
   --------------------------------------------------------*/

const byte RED_LED_PIN    = 10;   // connect the red LED (through a resistor) to D10
const byte YELLOW_LED_PIN =  9;   // yellow LED → D9
const byte GREEN_LED_PIN  =  8;   // green LED  → D8

const unsigned long RED_TIME     = 5000;  // ms the red LED stays on
const unsigned long GREEN_TIME   = 5000;  // ms the green LED stays on
const unsigned long YELLOW_TIME  = 2000;  // ms the yellow LED stays on

void setup() {
  pinMode(RED_LED_PIN,    OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN,  OUTPUT);

  // start with all LEDs off
  digitalWrite(RED_LED_PIN,    LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN,  LOW);
}

void loop() {
  // ----- Red phase ---------------------------------------------------------
  digitalWrite(RED_LED_PIN, HIGH);
  delay(RED_TIME);
  digitalWrite(RED_LED_PIN, LOW);

  // ----- Green phase -------------------------------------------------------
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(GREEN_TIME);
  digitalWrite(GREEN_LED_PIN, LOW);

  // ----- Yellow phase ------------------------------------------------------
  digitalWrite(YELLOW_LED_PIN, HIGH);
  delay(YELLOW_TIME);
  digitalWrite(YELLOW_LED_PIN, LOW);
}
