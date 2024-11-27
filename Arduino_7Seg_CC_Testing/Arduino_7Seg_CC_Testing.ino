// Define segment pins for the 7-segment display
const int segmentA = 2;
const int segmentB = 3;
const int segmentC = 4;
const int segmentD = 5;
const int segmentE = 6;
const int segmentF = 7;
const int segmentG = 8;
const int segmentDip = 9;

// Digits from 0 to 9 (Common Cathode, so HIGH means ON)
byte digits[10][7] = {
  {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, LOW},  // 0
  {LOW, HIGH, HIGH, LOW, LOW, LOW, LOW},      // 1
  {HIGH, HIGH, LOW, HIGH, HIGH, LOW, HIGH},   // 2
  {HIGH, HIGH, HIGH, HIGH, LOW, LOW, HIGH},   // 3
  {LOW, HIGH, HIGH, LOW, LOW, HIGH, HIGH},    // 4
  {HIGH, LOW, HIGH, HIGH, LOW, HIGH, HIGH},   // 5
  {HIGH, LOW, HIGH, HIGH, HIGH, HIGH, HIGH},  // 6
  {HIGH, HIGH, HIGH, LOW, LOW, LOW, LOW},     // 7
  {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH}, // 8
  {HIGH, HIGH, HIGH, HIGH, LOW, HIGH, HIGH}   // 9
};

// Function to display a digit on the 7-segment display
void displayDigit(int digit) {
  // Ensure the digit is within bounds (0-9)
  if (digit < 0 || digit > 9) return;

  // Set each segment according to the digit array
  digitalWrite(segmentA, digits[digit][0]);
  digitalWrite(segmentB, digits[digit][1]);
  digitalWrite(segmentC, digits[digit][2]);
  digitalWrite(segmentD, digits[digit][3]);
  digitalWrite(segmentE, digits[digit][4]);
  digitalWrite(segmentF, digits[digit][5]);
  digitalWrite(segmentG, digits[digit][6]);
}

void setup() {
  // Set the segment pins as OUTPUT
  pinMode(segmentA, OUTPUT);
  pinMode(segmentB, OUTPUT);
  pinMode(segmentC, OUTPUT);
  pinMode(segmentD, OUTPUT);
  pinMode(segmentE, OUTPUT);
  pinMode(segmentF, OUTPUT);
  pinMode(segmentG, OUTPUT);
  pinMode(segmentDip, OUTPUT);

  // Turn off all segments initially (LOW for common cathode)
  for (int i = 2; i <= 8; i++) {
    digitalWrite(i, LOW);
  }
}

void loop() {
  // Loop through digits 0 to 9 and display each for 1 second
  for (int i = 0; i < 10; i++) {
    displayDigit(i);
    delay(1000);  // Display for 1 second
  }
  digitalWrite(segmentDip, HIGH);
  delay(1000);
  digitalWrite(segmentDip, LOW);
}
