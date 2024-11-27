// Define the digital pin where the relay is connected
const int relayPin = A0;
const int relayPin1 = A1;

// Variables to store incoming serial data
String inputString = "";         // A string to hold incoming data
bool stringComplete = false;     // Whether the string is complete

void setup() {
  // Initialize the relay pin as an output
  pinMode(relayPin, OUTPUT);
  
  // Ensure the relay is off initially
  digitalWrite(relayPin, LOW);

  pinMode(relayPin1, OUTPUT);
  
  // Ensure the relay is off initially
  digitalWrite(relayPin1, LOW);
  delay(1000);
  // Initialize serial communication at 9600 bits per second
  Serial.begin(9600);
  
  // Reserve 200 bytes for the inputString
  inputString.reserve(200);
  
  Serial.println("Relay Control Ready.");
  Serial.println("Send 'ON' or '1' to turn the relay ON.");
  Serial.println("Send 'OFF' or '0' to turn the relay OFF.");
}

void loop() {
  // Print the string when a newline arrives
  if (stringComplete) {
    // Convert the input string to uppercase to make the command case-insensitive
    inputString.toUpperCase();
    
    if (inputString.equals("1") || inputString.equals("2") || inputString.equals("3")
    || inputString.equals("4")|| inputString.equals("5")|| inputString.equals("6")
    || inputString.equals("7")|| inputString.equals("8")|| inputString.equals("9")) {
      digitalWrite(relayPin, HIGH);   // Turn relay ON
      digitalWrite(relayPin1, HIGH);   // Turn relay ON
      Serial.println("Relay is ON");
      delay(30000);  // Wait for 2 seconds

      digitalWrite(relayPin, LOW);   // Turn relay OFF
      digitalWrite(relayPin1, LOW);   // Turn relay OFF
      Serial.println("Relay is OFF after 2 seconds");
      delay(1000);
    }
    else {
      digitalWrite(relayPin, LOW);    // Turn relay OFF
      digitalWrite(relayPin1, LOW);    // Turn relay OFF
      delay(1000);
      Serial.print("Unknown command: ");
      Serial.println(inputString);
      Serial.println("Please send 'ON', 'OFF', '1', or '0'.");
    }
    
    // Clear the input string and reset the flag
    inputString = "";
    stringComplete = false;
  }
  serialEvent();
}

// This function is called whenever new serial data arrives
void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read(); // Read the incoming byte
    
    // If the incoming character is a newline, set the flag
    if (inChar == '\n') {
      stringComplete = true;
    }
    else if (inChar != '\r') { // Ignore carriage returns
      inputString += inChar;     // Add to the input string
    }
  }
}
