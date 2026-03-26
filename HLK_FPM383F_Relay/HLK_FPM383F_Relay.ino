void setup() {
  Serial.begin(115200);
  pinMode(3, INPUT_PULLUP);
  Serial.println("Monitoring TOUCH pin...");
}

void loop() {
  Serial.println(digitalRead(3));
  delay(200);
}