// Define the GPIO pins connected to the relays
#define RELAY_1 5   // Adjust these pins according to your wiring
#define RELAY_2 6
#define RELAY_3 7
#define RELAY_4 8
#define RELAY_5 9
#define RELAY_6 10

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Set relay pins as outputs
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  pinMode(RELAY_4, OUTPUT);
  pinMode(RELAY_5, OUTPUT);
  pinMode(RELAY_6, OUTPUT);

  // Start with all relays OFF
  digitalWrite(RELAY_1, HIGH);
  digitalWrite(RELAY_2, HIGH);
  digitalWrite(RELAY_3, HIGH);
  digitalWrite(RELAY_4, HIGH);
  digitalWrite(RELAY_5, HIGH);
  digitalWrite(RELAY_6, HIGH);
}

void loop() {
  // Array of relay pins
  int relays[] = {RELAY_1, RELAY_2, RELAY_3, RELAY_4, RELAY_5, RELAY_6};
  int numRelays = sizeof(relays) / sizeof(relays[0]);

  // Turn each relay ON one by one, with a delay in between
  for (int i = 0; i < numRelays; i++) {
    Serial.print("Turning ON Relay ");
    Serial.println(i + 1);
    digitalWrite(relays[i], LOW); // Turn ON relay (active LOW)
    delay(1000);                  // Keep relay ON for 1 second
        Serial.print("Turning OFF Relay ");
    Serial.println(i + 1);
    digitalWrite(relays[i], HIGH); // Turn ON relay (active LOW)
    delay(1000);                  // Keep relay ON for 1 second
  }

}
