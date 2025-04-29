#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <LiquidCrystal_I2C.h>

// Create an MLX90614 object
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Create an LCD object (adjust the I2C address if needed, common addresses are 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buzzer pin
const int buzzerPin = 3;

// Temperature threshold in Fahrenheit (for object temperature)
const float tempThresholdF = 100.0;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }
  
  // Initialize MLX90614 sensor
  if (!mlx.begin()) {
    Serial.println("Error initializing MLX90614!");
    while (1) {
      delay(10);
    }
  }
  
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp Monitor");
  
  // Initialize buzzer pin
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  
  delay(1000);
}

void loop() {
  // Read object and ambient temperatures in Celsius
  float objTempC = mlx.readObjectTempC();
  float ambTempC = mlx.readAmbientTempC();
  
  // Convert to Fahrenheit
  float objTempF = objTempC * 9.0 / 5.0 + 32.0;
  float ambTempF = ambTempC * 9.0 / 5.0 + 32.0;
  
  // Print temperatures to Serial Monitor
  Serial.print("Object Temp: ");
  Serial.print(objTempC, 1);
  Serial.print(" C / ");
  Serial.print(objTempF, 1);
  Serial.println(" F");
  
  Serial.print("Ambient Temp: ");
  Serial.print(ambTempC, 1);
  Serial.print(" C / ");
  Serial.print(ambTempF, 1);
  Serial.println(" F");
  
  // Display the temperatures on the LCD
  lcd.clear();
  
  // First row: Object Temperature
  lcd.setCursor(0, 0);
  lcd.print("O:");
  lcd.print(objTempC, 1);
  lcd.print("C ");
  lcd.print(objTempF, 1);
  lcd.print("F");
  
  // Second row: Ambient Temperature
  lcd.setCursor(0, 1);
  lcd.print("A:");
  lcd.print(ambTempC, 1);
  lcd.print("C ");
  lcd.print(ambTempF, 1);
  lcd.print("F");
  
  // If the object temperature exceeds the threshold, beep the buzzer
  if (objTempF > tempThresholdF) {
    digitalWrite(buzzerPin, HIGH);
    delay(2000); // Beep at 1000 Hz
  } else {
    noTone(buzzerPin);
  }
  
  delay(1000); // Wait 1 second before next reading
}
