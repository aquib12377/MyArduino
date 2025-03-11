// Define joystick pins
#define JOYSTICK_X_PIN  34  // Analog pin for X-axis
#define JOYSTICK_Y_PIN  35  // Analog pin for Y-axis
#define JOYSTICK_SW_PIN 32  // Digital pin for the button (if needed)

// Define relay pins
#define RELAY1  25
#define RELAY2  26
#define RELAY3  27

// Joystick threshold values
#define DEADZONE 200  // Deadzone to avoid unwanted triggering
#define CENTER 2048    // ADC center value for 12-bit resolution (0-4095)

void setup() {
    Serial.begin(115200);
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(RELAY3, OUTPUT);
    pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP); // Joystick button (if used)
    
    // Initialize relays in off state (ACTIVE LOW means HIGH is OFF)
    digitalWrite(RELAY1, HIGH);
    digitalWrite(RELAY2, HIGH);
    digitalWrite(RELAY3, HIGH);
      analogSetAttenuation(ADC_11db);
}

void loop() {
    int xValue = analogRead(JOYSTICK_X_PIN);
    int yValue = analogRead(JOYSTICK_Y_PIN);
    int swState = digitalRead(JOYSTICK_SW_PIN);
    Serial.print("Y Value: "); Serial.println(yValue);
    Serial.print("X Value: "); Serial.println(xValue);
    // Default to turning off all relays (ACTIVE LOW means HIGH is OFF)
    digitalWrite(RELAY1, HIGH);
    digitalWrite(RELAY2, HIGH);
    digitalWrite(RELAY3, HIGH);

    // Forward movement (Y-axis forward)
    if (yValue > CENTER + DEADZONE) {
        Serial.println("Moving Forward - Relay1 ON");
        digitalWrite(RELAY1, LOW);
    }
    // Backward movement (Y-axis backward)
    else if (yValue < CENTER - DEADZONE) {
        Serial.println("Moving Backward - Relay3 ON");
        digitalWrite(RELAY3, LOW);
    }
    // Left movement (X-axis left)
    else if (xValue < CENTER - DEADZONE) {
        Serial.println("Moving Left - Relay2 ON");
        digitalWrite(RELAY2, LOW);
    }
    // Right movement (X-axis right)
    else if (xValue > CENTER + DEADZONE) {
        Serial.println("Moving Right - All Relays ON");
        digitalWrite(RELAY1, LOW);
        digitalWrite(RELAY2, LOW);
        digitalWrite(RELAY3, LOW);
    }
    
    delay(100); // Small delay to stabilize readings
}
