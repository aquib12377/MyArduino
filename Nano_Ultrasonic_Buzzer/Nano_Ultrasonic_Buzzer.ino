// Define pin numbers
const int trigPin = 9;    // Trigger pin for the ultrasonic sensor
const int echoPin = 10;   // Echo pin for the ultrasonic sensor
const int buzzerPin = 8;  // Pin connected to the buzzer

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  
  // Set pin modes
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  long duration;
  int distance;
  
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Send a 10 microsecond pulse to trigger the sensor
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echo pin: pulseIn returns the time (in microseconds) of the high pulse
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance in centimeters
  // Speed of sound is ~0.034 cm/microsecond. Divide by 2 since the pulse travels to the object and back.
  distance = duration * 0.034 / 2;
  
  // Debug print the distance
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  
  // If an object is detected within 20 cm, beep the buzzer
  if (distance > 0 && distance <= 20) {
    digitalWrite(buzzerPin, HIGH); // Turn on buzzer
    delay(100);                    // Beep duration
    digitalWrite(buzzerPin, LOW);  // Turn off buzzer
    delay(100);                    // Short pause between beeps
  } else {
    // Ensure buzzer is off when no object is detected
    digitalWrite(buzzerPin, LOW);
  }
  
  delay(100); // Delay before the next reading
}
