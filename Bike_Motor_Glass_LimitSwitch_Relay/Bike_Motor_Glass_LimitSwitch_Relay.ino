/* 
  Control a single DC motor in both directions using 2 relays (active low),
  2 limit switches (stop motor immediately when triggered),
  and a DPDT toggle switch feeding 2 digital pins for direction.

  Relay logic:
    - Relay coil energizes (activates) on LOW signal
    - Relay coil de-energizes (deactivates) on HIGH signal

  Motor direction:
    - Forward:  Relay1 = HIGH, Relay2 = LOW
    - Reverse:  Relay1 = LOW,  Relay2 = HIGH
    - Stop:     Relay1 = HIGH, Relay2 = HIGH
*/

///////////////////////////////////////
// Pin Assignments
///////////////////////////////////////
const int relay1Pin = 2;       // Drives Relay 1
const int relay2Pin = 3;       // Drives Relay 2

const int limitFwdPin = 4;     // Forward limit switch
const int limitRevPin = 5;     // Reverse limit switch

const int dirPin1    = 6;      // Direction input #1 from DPDT toggle
const int dirPin2    = 7;      // Direction input #2 from DPDT toggle

///////////////////////////////////////
// Setup
///////////////////////////////////////
void setup() {
  // Relay pins as outputs, initialize them to HIGH so relays are not energized
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay1Pin, HIGH);
  digitalWrite(relay2Pin, HIGH);

  // Limit switches as inputs with internal pullup
  // Assuming switch closes to GND when pressed
  pinMode(limitFwdPin, INPUT_PULLUP);
  pinMode(limitRevPin, INPUT_PULLUP);

  // Direction pins as inputs (can use INPUT_PULLUP depending on your wiring)
  pinMode(dirPin1, INPUT_PULLUP);
  pinMode(dirPin2, INPUT_PULLUP);

  // Optional: Start serial for debugging
  Serial.begin(9600);
  Serial.println("Motor Control Initialized");
}

///////////////////////////////////////
// Main Loop
///////////////////////////////////////
void loop() {
  // Read DPDT switch states
  int dir1State = digitalRead(dirPin1);
  int dir2State = digitalRead(dirPin2);

  // Read limit switch states
  // LOW = pressed, HIGH = not pressed
  int limitFwdState = digitalRead(limitFwdPin);
  int limitRevState = digitalRead(limitRevPin);

  // Decide what the desired motor direction is based on DPDT switch
  // dirPin1 = HIGH & dirPin2 = LOW   => Forward
  // dirPin1 = LOW  & dirPin2 = HIGH => Reverse
  // otherwise => Stop
  bool wantForward = (dir1State == HIGH && dir2State == LOW);
  bool wantReverse = (dir1State == LOW  && dir2State == HIGH);

  // Now check limit switches:
  // If we want forward but forward limit switch is pressed => STOP
  // If we want reverse but reverse limit switch is pressed => STOP
  if (wantForward && (limitFwdState == LOW)) {
    // Forward limit reached, stop immediately
    wantForward = false;
  }

  if (wantReverse && (limitRevState == LOW)) {
    // Reverse limit reached, stop immediately
    wantReverse = false;
  }

  // Based on final decision, energize or deenergize relays
  if (wantForward) {
    // Forward
    digitalWrite(relay1Pin, HIGH); // Not energized => +V to one side
    digitalWrite(relay2Pin, LOW);  // Energized => GND to other side
    Serial.println("Motor Forward");
  }
  else if (wantReverse) {
    // Reverse
    digitalWrite(relay1Pin, LOW);  // Energized => GND
    digitalWrite(relay2Pin, HIGH); // Not energized => +V
    Serial.println("Motor Reverse");
  }
  else {
    // Stop
    digitalWrite(relay1Pin, HIGH);
    digitalWrite(relay2Pin, HIGH);
    Serial.println("Motor Stopped");
  }

  delay(10); // short delay to avoid spamming messages (adjust as desired)
}
