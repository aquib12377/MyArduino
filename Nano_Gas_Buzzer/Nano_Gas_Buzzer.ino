// Define pins
const int gasSensorPin = 3;
const int buzzerPin = 2;

// Set a threshold value for gas detection
const int gasThreshold = 300;

void setup() {
  pinMode(gasSensorPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // Read the gas sensor value
  int gasValue = digitalRead(gasSensorPin);
  Serial.print("Gas value: ");
  Serial.println(gasValue);

  // Check if the gas value exceeds the threshold
  if (gasValue == LOW) {
    digitalWrite(buzzerPin, HIGH); // Turn on the buzzer
  } else {
    digitalWrite(buzzerPin, LOW);  // Turn off the buzzer
  }

  delay(500); // Delay for stability
}
