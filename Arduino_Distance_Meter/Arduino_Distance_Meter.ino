  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>

  // Define LCD address, size, and initialize the LCD
  LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x27 is a common I2C address; yours might be different

  // Define the ultrasonic sensor pins
  const int trigPin = 9;
  const int echoPin = 10;

  void setup() {
    // Initialize the LCD
    lcd.begin();
    lcd.backlight();
    //lcd.setBacklight(100);
    // Set the ultrasonic sensor pins
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // Display a startup message
    lcd.setCursor(0, 0);
    lcd.print("Distance Meter");
    lcd.setCursor(0, 1);
    lcd.print("Initializing...");
    delay(2000); // Display message for 2 seconds
    lcd.clear();
  }

  void loop() {
    long duration;
    float distance;

    // Send a pulse from the Trig pin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Read the duration of the pulse from the Echo pin
    duration = pulseIn(echoPin, HIGH);

    // Calculate the distance (in centimeters)
    distance = (duration * 0.034) / 2; // Speed of sound in cm/us divided by 2

    // Display the distance on the LCD
    lcd.setCursor(0, 0);
    lcd.print("Distance:");
    lcd.setCursor(0, 1);
    lcd.print(distance);
    lcd.print(" cm   ");

    // Small delay before the next reading
    delay(500);
  }
