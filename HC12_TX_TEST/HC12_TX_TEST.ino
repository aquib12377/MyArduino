// Transmitter (Remote) Sketch with Extra DP Toggle Button

// Button pin assignments
const int btnForward = 6;    // Button1: Forward
const int btnLeft    = 5;    // Button2: Left
const int btnRight   = 2;    // Button3: Right
const int btnServo   = 12;   // Button4: Servo sweep
const int btnDP      = A0;   // Extra button: Toggle command (D/P)

// Variables for servo sweep command
int servoAngle = 0;
int servoStep  = 5;  // Angle increment for servo sweep

// Variables for DP button toggle
bool sendD = true;          // Toggle flag: true = send 'D', false = send 'P'
bool lastBtnDPState = HIGH; // Last read state (using INPUT_PULLUP, HIGH means not pressed)

void setup() {
  // Initialize hardware serial (HC‑12 is on pins 0/1)
  Serial.begin(9600);
  
  // Configure button pins as inputs with internal pull-ups
  pinMode(btnForward, INPUT_PULLUP);
  pinMode(btnLeft,    INPUT_PULLUP);
  pinMode(btnRight,   INPUT_PULLUP);
  pinMode(btnServo,   INPUT_PULLUP);
  pinMode(btnDP,      INPUT_PULLUP);
}

void loop() {
  // Read the state of the motor and servo buttons (LOW means pressed)
  bool forwardPressed = (digitalRead(btnForward) == LOW);
  bool leftPressed    = (digitalRead(btnLeft)    == LOW);
  bool rightPressed   = (digitalRead(btnRight)   == LOW);
  bool servoPressed   = (digitalRead(btnServo)   == LOW);

  // --- Send motor commands ---
  if (forwardPressed) {
    Serial.println("F");  // Forward command
  } 
  else if (leftPressed) {
    Serial.println("L");  // Left command
  } 
  else if (rightPressed) {
    Serial.println("R");  // Right command
  } 
  else {
    Serial.println("S");  // Stop command if no motor button is pressed
  }

  // --- Send servo commands (while Button4 is held) ---
  if (servoPressed) {
    // Update servo angle for a sweep
    servoAngle += servoStep;
    if (servoAngle >= 180) {
      servoAngle = 180;
      servoStep = -servoStep;  // Reverse at upper limit
    } 
    else if (servoAngle <= 0) {
      servoAngle = 0;
      servoStep = -servoStep;  // Reverse at lower limit
    }
    // Send servo command with prefix 'S' followed by the angle
    Serial.print("A");
    Serial.println(servoAngle);
    delay(50);  // Adjust delay to control sweep speed
  }
  
  // --- Check extra DP button on A0 for toggle command ---
  bool currentDPState = digitalRead(btnDP);
  // Detect a new button press (transition from HIGH to LOW)
  if (lastBtnDPState == HIGH && currentDPState == LOW) {
    if (sendD) {
      Serial.println("D");  // First (or odd) press sends "D"
    } else {
      Serial.println("P");  // Second (or even) press sends "P"
    }
    sendD = !sendD;  // Toggle for next press
    delay(10);       // Short debounce delay
  }
  lastBtnDPState = currentDPState;
  
  delay(1);  // General loop delay (helps with debounce and prevents flooding)
}
