#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SD.h>

// Create object to represent PCA9685 at default I2C address
Adafruit_PWMServoDriver pca9685 = Adafruit_PWMServoDriver(0x40);

// Define maximum and minimum number of "ticks" for the servo motors
#define SERVOMIN 80   // Minimum value
#define SERVOMAX 600  // Maximum value
#define NUM_MOTORS 16 // Total number of motors connected to PCA9685

// SD Card module
#define SD_CS_PIN 5 // Chip select pin for SD Card module
#define SPK 27
const int dacPin = 25; // GPIO25 (DAC1)
const int sampleRate = 44100; // 44.1 kHz sample rate
const int frequency = 440; // Tone frequency in Hz (e.g., 440 Hz for A4 note)
void setup() {
  // Serial monitor setup
  Serial.begin(115200);
  pinMode(SPK,OUTPUT);
  // Initialize PCA9685
  Serial.println("Initializing PCA9685...");
  pca9685.begin();
  pca9685.setPWMFreq(50);

  // Initialize SD Card
  Serial.println("Initializing SD Card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card initialization failed!");
    //while (true); // Halt if SD card initialization fails
  }
  Serial.println("SD Card initialized successfully.");

  // Create a test file on the SD card
  File file = SD.open("/test.txt", FILE_WRITE);
  if (file) {
    file.println("PCA9685 and SD Card Test");
    file.close();
    Serial.println("Test file written to SD Card.");
  } else {
    Serial.println("Failed to create test file.");
  }
}

void loop() {
 
  // Move all motors forward (70 to 110 degrees)
  for (int posDegrees = 0; posDegrees <= 110; posDegrees++) {
    for (int motor = 0; motor < NUM_MOTORS; motor++) {
      int pwmValue = map(posDegrees, 0, 180, SERVOMIN, SERVOMAX);
      pca9685.setPWM(motor, 0, pwmValue);
      Serial.print("Motor ");
      Serial.print(motor);
      Serial.print(" = ");
      Serial.println(posDegrees);
    }
    delay(30); // Delay between position updates
  }

  // Move all motors backward (110 to 70 degrees)
  for (int posDegrees = 110; posDegrees >= 0; posDegrees--) {
    for (int motor = 0; motor < NUM_MOTORS; motor++) {
      int pwmValue = map(posDegrees, 0, 180, SERVOMIN, SERVOMAX);
      pca9685.setPWM(motor, 0, pwmValue);
      Serial.print("Motor ");
      Serial.print(motor);
      Serial.print(" = ");
      Serial.println(posDegrees);
    }
    delay(30); // Delay between position updates
  }
}
