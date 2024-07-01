  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>

  // Initialize the LCD
  LiquidCrystal_I2C lcd(0x27, 16, 2);

  // Define pin connections
  const int L1 = 2;
  const int L2 = 4;
  const int L3 = 6;
  const int GND = 8;
  const int flameSensor = 5;

  void setup() {
    // Initialize the LCD
    lcd.begin();
    lcd.backlight();
    
    // Set up pins
    pinMode(L1, INPUT_PULLUP);
    pinMode(L2, INPUT_PULLUP);
    pinMode(L3, INPUT_PULLUP);
    pinMode(GND, INPUT_PULLUP);
    pinMode(flameSensor, INPUT_PULLUP);
    
    // Start serial communication for debugging
    Serial.begin(9600);
  }

  void loop() {
    // Read the state of each switch
    int L1State = digitalRead(L1);
    int L2State = digitalRead(L2);
    int L3State = digitalRead(L3);
    int GNDState = digitalRead(GND);
    int flameState = digitalRead(flameSensor);

    // Determine the type of fault
    String faultMessage = "No Fault";
    if ((L1State == LOW && GNDState == LOW) ||
        (L2State == LOW && GNDState == LOW) ||
        (L3State == LOW && GNDState == LOW)) {
      faultMessage = "Line-Ground Fault";
    }
    if ((L1State == LOW && L2State == LOW) ||
        (L2State == LOW && L3State == LOW) ||
        (L3State == LOW && L1State == LOW)) {
      faultMessage = "Line-Line Fault";
    }
    if ((L1State == LOW && L2State == LOW && GNDState == LOW) ||
        (L2State == LOW && L3State == LOW && GNDState == LOW) ||
        (L3State == LOW && L1State == LOW && GNDState == LOW)) {
      faultMessage = "Line-Line-Ground";
    }

    // Display the fault status on the LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(faultMessage);

    // Display the flame sensor status on the LCD
    lcd.setCursor(0, 1);
    lcd.print("Fire: ");
    lcd.print(flameState ? "NO" : "YES");

    // Print the states to the serial monitor for debugging
    Serial.print("L1: ");
    Serial.print(L1State ? "OK" : "FAULT");
    Serial.print(" | L2: ");
    Serial.print(L2State ? "OK" : "FAULT");
    Serial.print(" | L3: ");
    Serial.print(L3State ? "OK" : "FAULT");
    Serial.print(" | GND: ");
    Serial.print(GNDState ? "OK" : "FAULT");
    Serial.print(" | FLAME: ");
    Serial.println(flameState ? "YES" : "NO");

    // Small delay to avoid rapid looping
    delay(500);
  }
