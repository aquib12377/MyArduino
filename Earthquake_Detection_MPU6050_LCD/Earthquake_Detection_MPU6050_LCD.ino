#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <MPU6050.h>

// Initialize MPU6050
MPU6050 mpu;

// Initialize LCD (I2C address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Buzzer pin
const int buzzerPin = A0;

// Thresholds for movement detection (in g)
const float LIGHT_MOVEMENT_THRESHOLD = 1.25;  // Light movement threshold
const float HEAVY_MOVEMENT_THRESHOLD = 2.5;  // Heavy movement threshold

// Variables to store raw acceleration data
int16_t ax, ay, az;

// Variable to store calculated magnitude of movement
float magnitude;

// Enumeration to represent movement states
enum MovementState { 
  NO_MOVEMENT, 
  LIGHT_MOVEMENT, 
  HEAVY_MOVEMENT 
};

// Current and previous movement states
MovementState currentState = NO_MOVEMENT;
MovementState previousState = NO_MOVEMENT;

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  
  // Initialize I2C communication
  Wire.begin();
  
  // Initialize MPU6050 sensor
  mpu.initialize();
  
  // Verify MPU6050 connection
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("MPU6050 Error");
    while (1); // Halt execution
  }
  
  // Initialize LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MPU6050 Ready");
  
  // Initialize buzzer pin
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // Ensure buzzer is off initially
  
  delay(1000); // Wait for a second before starting
}

void loop() {
  // Read raw acceleration data from MPU6050
  mpu.getAcceleration(&ax, &ay, &az);
  //mpu.ma
  // Convert raw data to 'g' units (assuming sensitivity of ±2g, scale factor 16384 LSB/g)
  float ax_g = ax / 16384.0;
  float ay_g = ay / 16384.0;
  float az_g = az / 16384.0;
  

  // Serial.print("X: ");
  // Serial.print(ax);
  // Serial.print(" | Y: ");
  // Serial.print(ay);
  // Serial.print(" | Z: ");
  // Serial.println(az);
  // Calculate the magnitude of acceleration
  magnitude = sqrt(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
  
  // Determine the current movement state based on magnitude
  if (magnitude > HEAVY_MOVEMENT_THRESHOLD) {
    currentState = HEAVY_MOVEMENT;
  }
  else if (magnitude > LIGHT_MOVEMENT_THRESHOLD) {
    currentState = LIGHT_MOVEMENT;
  }
  else {
    currentState = NO_MOVEMENT;
  }
  
  // If the movement state has changed, update LCD and trigger buzzer
  if (currentState != previousState) {
    if (currentState == HEAVY_MOVEMENT) {
      lcd.setCursor(0, 1);
      lcd.print("Heavy Movement  "); // Extra spaces to clear previous text
      beepHeavy();
    }
    else if (currentState == LIGHT_MOVEMENT) {
      lcd.setCursor(0, 1);
      lcd.print("Light Movement  ");
      beepLight();
    }
    else {
      lcd.setCursor(0, 1);
      lcd.print("No Movement     ");
    }
    
    // Update the previous state
    previousState = currentState;
  }
  
  // Display the magnitude on the first line of the LCD
  lcd.setCursor(0, 0);
  lcd.print("Mag: ");
  lcd.print(magnitude, 2); // Display magnitude with 2 decimal places
  lcd.print(" g  "); // Extra spaces to overwrite any residual characters
  
  // Print data to Serial Monitor for debugging
  Serial.print("ax: "); Serial.print(ax_g);
  Serial.print(" ay: "); Serial.print(ay_g);
  Serial.print(" az: "); Serial.print(az_g);
  Serial.print(" | Magnitude: "); Serial.println(magnitude);
  
  delay(500); // Wait for half a second before next reading
}

/**
 * Function to emit two distinct tones for light movement
 */
void beepLight() {
  // Emit first tone (2kHz for 300ms) - Higher frequency for louder sound
  tone(buzzerPin, 2000, 300);
  delay(100); // Short delay to make the sound continuous
  
  // Emit second tone (2.5kHz for 300ms)
  tone(buzzerPin, 2500, 300);
  delay(100); // Short delay before turning off
  
  noTone(buzzerPin); // Ensure buzzer is turned off
}

void beepHeavy() {
  // Emit first tone (3kHz for 400ms) - Even higher frequency for louder sound
  tone(buzzerPin, 3000, 400);
  delay(500); // Short delay to make the sound continuous
  
  // Emit second tone (3.5kHz for 400ms)
  tone(buzzerPin, 3500, 400);
  delay(500); // Short delay before turning off
  
  noTone(buzzerPin); // Ensure buzzer is turned off
}
