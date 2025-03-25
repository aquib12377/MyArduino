#include <DHT.h>
#include <Servo.h>

// Pin definitions
#define DHTPIN 2         // Pin connected to the DHT11 data pin
#define DHTTYPE DHT11    // Define sensor type: DHT11
#define SERVO_PIN 3      // Pin connected to the servo

// Create DHT sensor instance
DHT dht(DHTPIN, DHTTYPE);

// Create Servo instance
Servo myservo;

// Temperature threshold (in °C)
// If the measured temperature is above this value, the servo opens.
const float thresholdTemp = 30.0;

void setup() {
  Serial.begin(9600);
  dht.begin();
  
  // Attach servo and initialize to closed position (0°)
  myservo.attach(SERVO_PIN);
  myservo.write(0);
  
  Serial.println("Temperature based servo control started.");
}

void loop() {
  // Read temperature as Celsius
  float temperature = dht.readTemperature();

  // Check if reading failed
  if (isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(2000);
    return;
  }
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  
  // Open servo if temperature is above threshold, else close it
  if (temperature >= thresholdTemp) {
    Serial.println("Temperature above threshold. Opening servo.");
    myservo.write(90);  // Open position (90°)
  } else {
    Serial.println("Temperature below threshold. Closing servo.");
    myservo.write(0);   // Closed position (0°)
  }
  
  delay(2000);  // Update every 2 seconds
}
