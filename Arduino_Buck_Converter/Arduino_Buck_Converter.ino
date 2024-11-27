// Pin Definitions
int potentiometer = A0; // Pin for potentiometer input
int feedback = A1;      // Pin for feedback input
int PWM = 3;            // Pin for PWM output
int pwm = 0;            // Variable to store mapped PWM value

void setup() {
  // Initialize Serial Communication
  Serial.begin(9600);
  Serial.println("System Initializing...");
  
  // Set pin modes
  pinMode(potentiometer, INPUT); // Set potentiometer pin as input
  pinMode(feedback, INPUT);      // Set feedback pin as input
  pinMode(PWM, OUTPUT);          // Set PWM pin as output
  
  // Adjust PWM frequency for pin 3 and 11
  // Setting prescaler to 1 for a frequency of 31372.55 Hz
  TCCR2B = TCCR2B & B11111000 | B00000001; 
  Serial.println("PWM Frequency Adjusted to 31372.55 Hz");
  
  // Confirmation of setup
  Serial.println("Setup Complete. Starting loop...");
}

void loop() {
  // Read values from potentiometer and feedback
  float voltage = analogRead(potentiometer); // Read raw potentiometer value
  float output = analogRead(feedback);       // Read raw feedback value
  
  // Logging the raw values
  Serial.print("Raw Potentiometer Value: ");
  Serial.print(voltage);
  Serial.print(" | Raw Feedback Value: ");
  Serial.println(output);
  
  // Map the potentiometer value to PWM range (0-255)
  pwm = map(voltage, 0, 1023, 0, 255);
  
  // Log the mapped PWM value
  Serial.print("Mapped PWM Value: ");
  Serial.println(pwm);
  
  // Write the mapped PWM value to the PWM pin
  analogWrite(PWM, pwm);
  
  // Provide additional debug information
  Serial.print("PWM Signal Output on Pin ");
  Serial.print(PWM);
  Serial.print(": ");
  Serial.println(pwm);
  
  // Add a delay for better serial monitor readability
  delay(500); // Delay of 500ms between each loop iteration
}
