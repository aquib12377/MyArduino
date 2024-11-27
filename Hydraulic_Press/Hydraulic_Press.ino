// Pin Definitions
const int trigPin = 5;
const int echoPin = 4;
const int limitSwitch1 = 2;
const int limitSwitch2 = 3;
const int motorIn1 = 8;
const int motorIn2 = 9;
const int buzzerPin = 6;
const int startButton = A5; // Start button
const int stopButton = A4;  // Stop button

// Distance Threshold
const int distanceThreshold = 10; // in cm

// State Variables
bool running = false; // Tracks if the system is running

// Function to measure distance using the ultrasonic sensor
long measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2; // Convert to centimeters
}

// Function to stop the motor
void stopMotor() {
  digitalWrite(motorIn1, LOW);
  digitalWrite(motorIn2, LOW);
  Serial.println("Motor stopped.");
}

// Function to beep the buzzer
void beep(int frequency, int duration, int count = 1, int delayBetweenBeeps = 300) {
  for (int i = 0; i < count; i++) {
    tone(buzzerPin, frequency, duration);
    delay(duration + delayBetweenBeeps);
  }
}

void setup() {
  // Initialize pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(limitSwitch1, INPUT_PULLUP);
  pinMode(limitSwitch2, INPUT_PULLUP);
  pinMode(motorIn1, OUTPUT);
  pinMode(motorIn2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(startButton, INPUT_PULLUP);
  pinMode(stopButton, INPUT_PULLUP);

  // Initialize Serial Monitor
  Serial.begin(9600);

  // Stop motor initially
  stopMotor();
  Serial.println("System initialized. Waiting for Start button press.");
}

void loop() {
  // Check if Stop button is pressed
  if (digitalRead(stopButton) == LOW) {
    Serial.println("Stop button pressed. Stopping system.");
    running = false; // Set running state to false
    stopMotor();
    beep(1500, 2000, 1); // Long beep for stop
    delay(200); // Debounce delay
    return; // Exit loop to restart process
  }

  // Check if Start button is pressed
  if (digitalRead(startButton) == LOW && !running) {
    Serial.println("Start button pressed. Starting system.");
    running = true; // Set running state to true
    delay(200); // Debounce delay
  }

  if (running) {
    beep(1000, 200, 3);
    // Measure distance
    long distance = measureDistance();
    Serial.print("Distance measured: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance <= distanceThreshold) {
      // Object detected: Beep once, then three short beeps
      beep(1000, 300, 1);  // Single beep
        // Three short beeps

      // Start motor CW until limitSwitch1 is pressed
      Serial.println("Object detected. Starting motor CW...");
      digitalWrite(motorIn1, HIGH);
      digitalWrite(motorIn2, LOW);

      while (digitalRead(limitSwitch1) == HIGH) {
        if (!running || digitalRead(stopButton) == LOW) {
          Serial.println("Stop button pressed during CW motion.");
          running = false;
          stopMotor();
          beep(1500, 2000, 1); // Long beep for stop
          break;
        }
      }

      // Stop motor, beep once, and wait 5 seconds
      stopMotor();
      beep(1000, 300, 1); // Single beep
      Serial.println("Limit Switch 1 pressed. Motor stopped. Waiting 5 seconds...");
      delay(5000);

      // Start motor ACW until limitSwitch2 is pressed
      Serial.println("Starting motor ACW...");
      digitalWrite(motorIn1, LOW);
      digitalWrite(motorIn2, HIGH);

      while (digitalRead(limitSwitch2) == HIGH) {
        if (!running || digitalRead(stopButton) == LOW) {
          Serial.println("Stop button pressed during ACW motion.");
          running = false;
          stopMotor();
          beep(1500, 2000, 1); // Long beep for stop
          break;;
        }
      }

      // Stop motor and beep
      stopMotor();
      beep(1000, 300, 1); // Single beep
      Serial.println("Limit Switch 2 pressed. Motor stopped.");
    } else {
      Serial.println("No object detected within range.");
    }
  }

  // Short delay for stability
  delay(100);
}
