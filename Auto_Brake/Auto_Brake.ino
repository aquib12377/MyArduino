// ======================
// Arduino Auto-Brake Bot
// ======================

// ----------------------
// Pin Definitions
// ----------------------
const int motor1Pin1 = 4;   // IN1
const int motor1Pin2 = 5;   // IN2
const int motor2Pin1 = 6;   // IN3
const int motor2Pin2 = 7;   // IN4

const int trigPin = A1;      // Ultrasonic Trigger
const int echoPin = A0;      // Ultrasonic Echo

// ----------------------
// Ultrasonic Variables
// ----------------------
long duration;
int distance;

// ----------------------
// Auto-Brake Variables
// ----------------------
const int threshold = 20;          // Distance threshold in centimeters
const int brakeSteps = 50;         // Number of steps to decelerate
const int brakeDelay = 20;         // Delay between steps in milliseconds

// ----------------------
// Software PWM Variables
// ----------------------
const int pwmFrequency = 100;      // PWM frequency in Hz
const int pwmResolution = 1000;    // PWM resolution (steps per second)

unsigned long pwmInterval = 1000 / pwmFrequency; // Interval between PWM cycles
unsigned long lastPwmTime = 0;         // Last PWM toggle time

int pwmStep = 0;                        // Current PWM step
bool pwmState = LOW;                    // Current state of PWM

// ----------------------
// Setup Function
// ----------------------
void setup() {
  // Initialize Motor Pins
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);
  
  // Initialize Ultrasonic Sensor Pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Initialize Serial Monitor for debugging (optional)
  Serial.begin(9600);
  
  // Start moving forward
  moveForward();
}

// ----------------------
// Loop Function
// ----------------------
void loop() {
  // Read distance from ultrasonic sensor
  distance = getDistance();
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  if (distance > 0 && distance < threshold) {
    Serial.println("Obstacle detected! Initiating auto-brake.");
    autoBrake();
  }
  moveForward();
  // Small delay to prevent excessive serial output
  delay(100);
}

// ----------------------
// Function Definitions
// ----------------------

// Function to move the robot forward
void moveForward() {
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, HIGH);
  digitalWrite(motor2Pin2, LOW);
}

// Function to stop the robot immediately
void stopBot() {
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(motor2Pin1, LOW);
  digitalWrite(motor2Pin2, LOW);
}

// Function to perform auto-brake (gradual stop)
void autoBrake() {
  // Gradually reduce the motor "on" time using software PWM
  for(int step = brakeSteps; step >= 0; step--){
    int pwmValue = map(step, 0, brakeSteps, 0, 255);
    setMotorPwm(pwmValue);
    delay(brakeDelay);
  }
  
  // Stop the motors completely
  stopBot();
}

// Function to set motor speed using software PWM
void setMotorPwm(int pwmValue) {
  // Calculate duty cycle (0-100)
  int dutyCycle = map(pwmValue, 0, 255, 0, 100);
  
  // Simulate PWM by toggling the motor pins
  for(int i = 0; i < dutyCycle; i++) {
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor2Pin1, HIGH);
    delayMicroseconds(1000 / pwmFrequency); // Adjust based on pwmFrequency
  }
  
  for(int i = dutyCycle; i < 100; i++) {
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor2Pin1, LOW);
    delayMicroseconds(1000 / pwmFrequency); // Adjust based on pwmFrequency
  }
}

// Function to get distance from ultrasonic sensor in centimeters
int getDistance() {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echo pin
  duration = pulseIn(echoPin, HIGH, 30000); // Timeout after 30ms (~5 meters)
  
  // Calculate distance in cm
  if (duration > 0) {
    distance = duration * 0.034 / 2;
    return distance;
  } else {
    // If no echo received, return a large distance
    return 999;
  }
}
