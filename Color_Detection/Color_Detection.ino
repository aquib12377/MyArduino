#define S0_PIN 4  // Module pins wiring
#define S1_PIN 5
#define S2_PIN 6
#define S3_PIN 7
#define OUT_PIN 8

int redValue = 0, blueValue = 0, greenValue = 0;  // RGB values 

void setup() {
  pinMode(S0_PIN, OUTPUT);  // Pin modes
  pinMode(S1_PIN, OUTPUT);
  pinMode(S2_PIN, OUTPUT);
  pinMode(S3_PIN, OUTPUT);
  pinMode(OUT_PIN, INPUT);

  Serial.begin(9600);  // Initialize the serial monitor baud rate
   
  digitalWrite(S0_PIN, HIGH);
  digitalWrite(S1_PIN, HIGH);
}

void loop() {
  getColors();  // Execute the getColors function to get the value of each RGB color

  if (redValue <= 15 && greenValue <= 15 && blueValue <= 15) {  // If the values are low, it's likely white color (all colors are present)
    Serial.println("White");                    
  } else if (redValue < blueValue && redValue <= greenValue && redValue < 23) {  // If red value is the lowest one and smaller than 23, it's likely red
    Serial.println("Red");
  } else if (blueValue < greenValue && blueValue < redValue && blueValue < 20) {  // Same thing for blue
    Serial.println("Blue");
  } else if (greenValue < redValue && greenValue - blueValue <= 8) {  // Green was a little tricky, so check the difference between green and blue to see if it's acceptable
    Serial.println("Green");                    
  } else {
    Serial.println("Unknown");  // If the color is not recognized, you can add as many as you want
  }

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
