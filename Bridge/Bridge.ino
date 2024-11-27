// Smart Bridge Alert System
// Components:
// - IR Module (Active Low) connected to A0 for Tsunami detection
// - 2 Servo Motors for Barricades connected to pins 5 and 6
// - 2 Servo Motors for Bridge movement connected to pins 9 and 10
// - 4 LEDs (2 Green, 2 Red) connected to pins A2, A3, A4, and A5
// - Buzzer connected to A1 for Tsunami alert

#include <Servo.h>

// Define pins
const int irPin = A0;      // IR Module pin
const int buzzerPin = A1;  // Buzzer pin

// Servo pins
const int barricadeServo1Pin = 5;
const int barricadeServo2Pin = 6;
const int bridgeServo1Pin = 9;
const int bridgeServo2Pin = 10;

// LED pins
const int redLED1Pin = A2;
const int greenLED1Pin = A3;
const int redLED2Pin = A4;
const int greenLED2Pin = A5;

// Create servo objects
Servo barricadeServo1;
Servo barricadeServo2;
Servo bridgeServo1;
Servo bridgeServo2;

// Variable to store previous sensor state
int previousIrValue = LOW; // Assuming no Tsunami at startup

void setup() {
  Serial.begin(9600); // Initialize serial communication

  // Attach servos to their respective pins
  barricadeServo1.attach(barricadeServo1Pin);
  barricadeServo2.attach(barricadeServo2Pin);
  bridgeServo1.attach(bridgeServo1Pin);
  bridgeServo2.attach(bridgeServo2Pin);

  // Initialize LED pins as OUTPUT
  pinMode(redLED1Pin, OUTPUT);
  pinMode(greenLED1Pin, OUTPUT);
  pinMode(redLED2Pin, OUTPUT);
  pinMode(greenLED2Pin, OUTPUT);

  // Initialize buzzer pin as OUTPUT
  pinMode(buzzerPin, OUTPUT);

  // Initialize IR sensor pin as INPUT with pull-up resistor
  pinMode(irPin, INPUT_PULLUP);

  // Default state (No Tsunami detected)
  barricadeServo1.write(0);  // Barricades raised
  barricadeServo2.write(0);
  bridgeServo1.write(0);     // Bridge lowered
  bridgeServo2.write(0);

  digitalWrite(greenLED1Pin, HIGH);  // Green LEDs ON
  digitalWrite(greenLED2Pin, HIGH);
  digitalWrite(redLED1Pin, LOW);     // Red LEDs OFF
  digitalWrite(redLED2Pin, LOW);

  noTone(buzzerPin);  // Buzzer OFF

  Serial.println("System initialized. Waiting for Tsunami detection...");
}

void loop() {
  int irValue = digitalRead(irPin);  // Read IR sensor value
  Serial.print("IR sensor value: ");
  Serial.println(irValue);

  // Check if the sensor value has changed
  if (irValue != previousIrValue) {
    // Sensor value has changed
    if (irValue == LOW) {  // Tsunami detected
      Serial.println("Tsunami detected!");

      // Turn on red LEDs and turn off green LEDs
      digitalWrite(redLED1Pin, HIGH);
      digitalWrite(redLED2Pin, HIGH);
      digitalWrite(greenLED1Pin, LOW);
      digitalWrite(greenLED2Pin, LOW);
      Serial.println("Red LEDs ON, Green LEDs OFF");

      // Generate different tones with the buzzer
      Serial.println("Activating buzzer with varying tones...");
      for (int freq = 1000; freq <= 2000; freq += 250) {
        tone(buzzerPin, freq); // Play tone at frequency 'freq' Hz
        Serial.print("Playing tone at ");
        Serial.print(freq);
        Serial.println(" Hz");
        delay(500);            // Play each tone for 500ms
      }
      noTone(buzzerPin);       // Stop the tone
      Serial.println("Buzzer deactivated");

      // Move barricades and bridge servos together
      Serial.println("Moving barricades and bridge...");
      int startAngleBarricade = barricadeServo1.read();
      int endAngleBarricade = 90;
      int startAngleBridge = bridgeServo1.read();
      int endAngleBridge = 60;
      int stepsBarricade = abs(endAngleBarricade - startAngleBarricade) / 2;
      int stepsBridge = abs(endAngleBridge - startAngleBridge) / 2;
      int maxSteps = max(stepsBarricade, stepsBridge);
      int directionBarricade = (endAngleBarricade > startAngleBarricade) ? 2 : -2;
      int directionBridge = (endAngleBridge > startAngleBridge) ? 2 : -2;
      for (int i = 0; i <= maxSteps; i++) {
        int angleBarricade = startAngleBarricade + i * directionBarricade;
        int angleBridge = startAngleBridge + i * directionBridge;
        angleBarricade = constrain(angleBarricade, min(startAngleBarricade, endAngleBarricade), max(startAngleBarricade, endAngleBarricade));
        angleBridge = constrain(angleBridge, min(startAngleBridge, endAngleBridge), max(startAngleBridge, endAngleBridge));
        barricadeServo1.write(angleBarricade);
        barricadeServo2.write(angleBarricade);
        bridgeServo1.write(angleBridge);
        bridgeServo2.write(angleBridge);
        Serial.print("Moving Barricade Servos to angle: ");
        Serial.println(angleBarricade);
        Serial.print("Moving Bridge Servos to angle: ");
        Serial.println(angleBridge);
        delay(5);
      }
      Serial.println("Barricades lowered and bridge raised.");

    } else {  // No Tsunami detected
      Serial.println("Tsunami cleared!");

      // Turn on green LEDs and turn off red LEDs
      digitalWrite(greenLED1Pin, HIGH);
      digitalWrite(greenLED2Pin, HIGH);
      digitalWrite(redLED1Pin, LOW);
      digitalWrite(redLED2Pin, LOW);
      Serial.println("Green LEDs ON, Red LEDs OFF");

      // Deactivate buzzer
      noTone(buzzerPin);
      Serial.println("Buzzer deactivated");

      // Move barricades and bridge servos together
      Serial.println("Moving barricades and bridge...");
      int startAngleBarricade = barricadeServo1.read();
      int endAngleBarricade = 0;
      int startAngleBridge = bridgeServo1.read();
      int endAngleBridge = 0;
      int stepsBarricade = abs(endAngleBarricade - startAngleBarricade) / 2;
      int stepsBridge = abs(endAngleBridge - startAngleBridge) / 2;
      int maxSteps = max(stepsBarricade, stepsBridge);
      int directionBarricade = (endAngleBarricade > startAngleBarricade) ? 2 : -2;
      int directionBridge = (endAngleBridge > startAngleBridge) ? 2 : -2;
      for (int i = 0; i <= maxSteps; i++) {
        int angleBarricade = startAngleBarricade + i * directionBarricade;
        int angleBridge = startAngleBridge + i * directionBridge;
        angleBarricade = constrain(angleBarricade, min(startAngleBarricade, endAngleBarricade), max(startAngleBarricade, endAngleBarricade));
        angleBridge = constrain(angleBridge, min(startAngleBridge, endAngleBridge), max(startAngleBridge, endAngleBridge));
        barricadeServo1.write(angleBarricade);
        barricadeServo2.write(angleBarricade);
        bridgeServo1.write(angleBridge);
        bridgeServo2.write(angleBridge);
        Serial.print("Moving Barricade Servos to angle: ");
        Serial.println(angleBarricade);
        Serial.print("Moving Bridge Servos to angle: ");
        Serial.println(angleBridge);
        delay(5);
      }
      Serial.println("Barricades raised and bridge lowered.");
    }
  } else {
    // Sensor value hasn't changed, no action needed
    Serial.println("No change in sensor value, no action taken.");
  }

  // Update the previous sensor value
  previousIrValue = irValue;

  delay(100);  // Small delay for debounce
}
