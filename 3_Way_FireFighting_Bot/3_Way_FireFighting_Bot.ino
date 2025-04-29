// Define motor control pins for a 4-wheel bot
#define MOTOR_LEFT_FORWARD    2
#define MOTOR_LEFT_BACKWARD   3
#define MOTOR_RIGHT_FORWARD   4
#define MOTOR_RIGHT_BACKWARD  5

// Define flame sensor pins (each outputs LOW when flame is detected)
#define SENSOR_LEFT    6
#define SENSOR_CENTER  7
#define SENSOR_RIGHT   8

// Define relay pin (Active Low: LOW = relay ON, HIGH = relay OFF)
#define RELAY_PIN      A0

// Setup function runs once when the board is powered on or reset
void setup() {
  // Start serial communication at 9600 baud rate
  Serial.begin(9600);
  
  // Set the motor control pins as outputs
  pinMode(MOTOR_LEFT_FORWARD, OUTPUT);
  pinMode(MOTOR_LEFT_BACKWARD, OUTPUT);
  pinMode(MOTOR_RIGHT_FORWARD, OUTPUT);
  pinMode(MOTOR_RIGHT_BACKWARD, OUTPUT);

  // Set flame sensor pins as inputs
  pinMode(SENSOR_LEFT, INPUT_PULLUP);
  pinMode(SENSOR_CENTER, INPUT_PULLUP);
  pinMode(SENSOR_RIGHT, INPUT_PULLUP);

  // Set the relay pin as output; initialize it to OFF (HIGH because relay is active low)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // Print startup message
  Serial.println("Robot Initialized");
}

// Loop continuously reads sensor inputs and controls the bot
void loop() {
  // Read the flame sensor values (LOW means flame detected)
  bool flameCenter = (digitalRead(SENSOR_CENTER) == LOW);
  bool flameRight  = (digitalRead(SENSOR_RIGHT)  == LOW);
  bool flameLeft  = (digitalRead(SENSOR_LEFT)  == LOW);
  Serial.print("Center: ");
  Serial.print(flameCenter);
  Serial.print(" Right: ");
  Serial.print(flameRight);
  Serial.print(" Left: ");
  Serial.println(flameLeft);
  // Behavior:
  // - If the flame reaches the center sensor, stop and turn on the relay.
  // - Otherwise, if the right sensor sees a flame (and center sensor is still HIGH), turn right.
  // - Else, stop the bot and ensure the relay is off.
  if (flameCenter) {
    Serial.println("Flame detected on center sensor: Stopping bot and activating relay");
    // Flame detected on center: stop and activate the relay.
    // forward();
    // delay(1000);
    stopBot();
    digitalWrite(RELAY_PIN, LOW);  // Activate relay (active low)
    delay(7000);
    digitalWrite(RELAY_PIN, HIGH); // Ensure relay remains off while turning.    
  }
  else if (flameRight) {
    Serial.println("Flame detected on right sensor: Turning right and relay off");
    // Flame detected on right sensor: turn right.
    turnLeft();
    digitalWrite(RELAY_PIN, HIGH); // Ensure relay remains off while turning.
  }
  else if (flameLeft) {
    Serial.println("Flame detected on left sensor: Turning left and relay off");
    // Flame detected on right sensor: turn right.
    turnRight();
    digitalWrite(RELAY_PIN, HIGH); // Ensure relay remains off while turning.
  }
  else {
    Serial.println("No flame detected: Stopping bot and deactivating relay");
    // No flame detected: stop the bot and deactivate relay.
    stopBot();
    digitalWrite(RELAY_PIN, HIGH); // Deactivate relay.
  }

  delay(100); // Delay to stabilize sensor readings
}

// Function to turn right: spins the bot
void turnRight() {
  // To turn right, run the left motor forward and right motor backward.
  digitalWrite(MOTOR_LEFT_FORWARD, HIGH);
  digitalWrite(MOTOR_LEFT_BACKWARD, LOW);
  digitalWrite(MOTOR_RIGHT_FORWARD, LOW);
  digitalWrite(MOTOR_RIGHT_BACKWARD, HIGH);
}

void forward() {
  // To turn right, run the left motor forward and right motor backward.
  digitalWrite(MOTOR_LEFT_FORWARD, HIGH);
  digitalWrite(MOTOR_LEFT_BACKWARD, LOW);
  digitalWrite(MOTOR_RIGHT_FORWARD, HIGH);
  digitalWrite(MOTOR_RIGHT_BACKWARD, LOW);
}

void turnLeft() {
  // To turn right, run the left motor forward and right motor backward.
  digitalWrite(MOTOR_LEFT_FORWARD, LOW);
  digitalWrite(MOTOR_LEFT_BACKWARD, HIGH);
  digitalWrite(MOTOR_RIGHT_FORWARD, HIGH);
  digitalWrite(MOTOR_RIGHT_BACKWARD, LOW);
}

// Function to stop the bot (turns off all motor outputs)
void stopBot() {
  digitalWrite(MOTOR_LEFT_FORWARD, LOW);
  digitalWrite(MOTOR_LEFT_BACKWARD, LOW);
  digitalWrite(MOTOR_RIGHT_FORWARD, LOW);
  digitalWrite(MOTOR_RIGHT_BACKWARD, LOW);
}
