#include <Wire.h>  // Include Wire library for I2C communication
#include <LiquidCrystal_I2C.h>  // Include LiquidCrystal_I2C library for LCD
#include "Servo.h"

#define S0_PIN 4  // Module pins wiring
#define S1_PIN 5
#define S2_PIN 6
#define S3_PIN 7
#define OUT_PIN 8
#define SERVO_PIN 9  // Servo pin

int redValue = 0, blueValue = 0, greenValue = 0;  // RGB values 
Servo myservo;  // Create a servo object
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Address 0x27 for a 16x2 LCD

void setup() {
  pinMode(S0_PIN, OUTPUT);  // Pin modes
  pinMode(S1_PIN, OUTPUT);
  pinMode(S2_PIN, OUTPUT);
  pinMode(S3_PIN, OUTPUT);
  pinMode(OUT_PIN, INPUT);

  Serial.begin(9600);  // Initialize the serial monitor baud rate
   
  digitalWrite(S0_PIN, HIGH);
  digitalWrite(S1_PIN, HIGH);

  myservo.attach(SERVO_PIN);  // Attach the servo to the specified pin
  myservo.write(90);  // Initialize the servo to the neutral position

  lcd.begin();  // Initialize the LCD
  lcd.backlight();  // Turn on the backlight
  lcd.clear();  // Clear the LCD screen
  lcd.print(" Color Sorting");
  delay(3000);
  lcd.clear();
  lcd.print("Color: ");  // Print constant label for color
  lcd.setCursor(0, 1);  // Move to the second line
  lcd.print("Angle: ");  // Print constant label for angle
}

void loop() {
  getColors();  // Execute the getColors function to get the value of each RGB color

  lcd.setCursor(7, 0);  // Move cursor to the position after "Color: "
  if (redValue < blueValue && redValue <= greenValue && redValue < 23) {  // If red value is the lowest one and smaller than 23, it's likely red
    lcd.print("Red    ");
    myservo.write(45);  // Move servo to 0 degrees
  } else if (blueValue < greenValue && blueValue < redValue && blueValue < 20) {  // Same thing for blue
    lcd.print("Blue   ");
    myservo.write(90);  // Move servo to 180 degrees
  } else if (greenValue < redValue && greenValue - blueValue <= 8) {  // Green was a little tricky, so check the difference between green and blue to see if it's acceptable
    lcd.print("Green  ");
    myservo.write(135);  // Move servo to 90 degrees
  } else {
    lcd.print("Unknown");  // If the color is not recognized, you can add as many as you want
    myservo.write(180);  // Move servo to the default position
  }

  lcd.setCursor(7, 1);  // Move cursor to the position after "Angle: "
  lcd.print(String(myservo.read())+"  ");  // Print servo angle

  delay(500);
}

void getColors() { 
  digitalWrite(S2_PIN, LOW);                                           
  digitalWrite(S3_PIN, LOW);                                           
  redValue = pulseIn(OUT_PIN, digitalRead(OUT_PIN) == HIGH ? LOW : HIGH);  // Measure red value
  delay(20);  

  digitalWrite(S3_PIN, HIGH);  // Select the blue set of photodiodes and measure the value
  blueValue = pulseIn(OUT_PIN, digitalRead(OUT_PIN) == HIGH ? LOW : HIGH);
  delay(20);  

  digitalWrite(S2_PIN, HIGH);  // Select the green set of photodiodes and measure the value
  greenValue = pulseIn(OUT_PIN, digitalRead(OUT_PIN) == HIGH ? LOW : HIGH);
  delay(20);  
}
