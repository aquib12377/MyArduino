// Buzzer on D8, Water Sensor on D2 (digital)

const int waterSensorPin = A0;
const int buzzerPin = A1;

void setup() {
  Serial.begin(9600);

  pinMode(waterSensorPin, INPUT); // water sensor digital pin
  pinMode(buzzerPin, OUTPUT);     // buzzer pin

  // Initially turn buzzer off
  digitalWrite(buzzerPin, LOW);
}

void loop() {
  int waterState = analogRead(waterSensorPin);
Serial.println(waterState);
  // If your sensor is active LOW, then waterState == LOW means water detected
  if (waterState > 620) {
    // Turn the buzzer ON
    digitalWrite(buzzerPin, HIGH);
    Serial.println("Water Detected! Buzzer ON");
  } else {
    // Turn the buzzer OFF
    digitalWrite(buzzerPin, LOW);
    Serial.println("No Water Detected. Buzzer OFF");
  }

  delay(500); 
}
