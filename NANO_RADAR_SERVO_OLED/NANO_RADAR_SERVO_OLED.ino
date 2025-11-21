#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

// -------------------- OLED CONFIG --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);   // -1 = no reset pin

// Radar origin (bottom center of screen)
const int ORIGIN_X = SCREEN_WIDTH / 2;
const int ORIGIN_Y = SCREEN_HEIGHT - 1;
const int RADAR_RADIUS = 60;      // max radius in pixels (should fit inside 64px height)

// -------------------- PINS --------------------
const int SERVO_PIN = 3;
const int TRIG_PIN  = 12;
const int ECHO_PIN  = 11;

// -------------------- SERVO CONFIG --------------------
const int MIN_ANGLE = 15;
const int MAX_ANGLE = 165;

Servo radarServo;
int servoAngle = MIN_ANGLE;   // starting angle
int servoStep  = 1;           // step per frame
bool sweepingForward = true;  // true: MIN_ANGLE -> MAX_ANGLE, false: back

// -------------------- ULTRASONIC CONFIG --------------------
const float MAX_DISTANCE_CM = 30.0; // anything farther is "no object"

// -------------------- FUNCTIONS --------------------

// Simple typing animation for a given text and total duration
void showTypingAnimation(const char* msg, uint16_t totalDurationMs)
{
  uint8_t len = strlen(msg);
  if (len == 0) return;

  // Calculate delay per character so full word takes ~totalDurationMs
  uint16_t perCharDelay = totalDurationMs / len;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Vertically center the text (roughly)
  uint8_t y = (SCREEN_HEIGHT - 8) / 2; // 8px font height
  display.setCursor((SCREEN_WIDTH - (len * 6)) / 2, y);
  // 6px per char (5px width + 1px space) for default font

  Serial.println(F("[SPLASH] Showing 'Alpha Electronz' typing animation"));

  for (uint8_t i = 0; i < len; i++) {
    display.print(msg[i]);
    display.display();
    delay(perCharDelay);
  }

  // small extra hold (optional)
  delay(300);
}

float readDistanceCm()
{
  // Trigger ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo time (timeout ~25ms => ~4m)
  long duration = pulseIn(ECHO_PIN, HIGH);

  if (duration == 0) {
    // No echo received in time
    Serial.println(F("[US] Timeout: no echo"));
    return -1;
  }

  // Convert time to distance (cm)
  float distance = duration / 58.0;  // HC-SR04 constant

  Serial.print(F("[US] duration="));
  Serial.print(duration);
  Serial.print(F(" us, distance="));
  Serial.print(distance);
  Serial.println(F(" cm"));

  return distance;
}

// Draw static radar circles and baseline
void drawRadarGrid()
{
  // Concentric circles
  display.drawCircle(ORIGIN_X, ORIGIN_Y, RADAR_RADIUS, WHITE);
  display.drawCircle(ORIGIN_X, ORIGIN_Y, RADAR_RADIUS * 2 / 3, WHITE);
  display.drawCircle(ORIGIN_X, ORIGIN_Y, RADAR_RADIUS / 3, WHITE);

  // Baseline (left-right)
  display.drawLine(ORIGIN_X - RADAR_RADIUS, ORIGIN_Y,
                   ORIGIN_X + RADAR_RADIUS, ORIGIN_Y, WHITE);
}

// Map servo angle to radar sweep angle (0..180° left->right)
float servoAngleToRad(int angle)
{
  // Clamp to valid range
  if (angle < MIN_ANGLE) angle = MIN_ANGLE;
  if (angle > MAX_ANGLE) angle = MAX_ANGLE;

  // Map servo angle (MIN_ANGLE..MAX_ANGLE) to display angle (0..180)
  // 0° -> right, 90° -> up, 180° -> left
  long displayDeg = map(angle, MIN_ANGLE, MAX_ANGLE, 0, 180);

  // NO extra inversion here – so sweep direction matches servo movement
  float rad = displayDeg * PI / 180.0;
  return rad;
}

void setup()
{
  Serial.begin(9600);
  Serial.println(F("\n=== Radar Setup Start ==="));

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  radarServo.attach(SERVO_PIN);
  radarServo.write(servoAngle);
  Serial.print(F("[SERVO] Initial angle: "));
  Serial.println(servoAngle);
  delay(500);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[OLED] SSD1306 allocation failed!"));
    // If OLED init fails, stay here
    while (true) {
      pinMode(LED_BUILTIN, OUTPUT);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
    }
  }

  Serial.println(F("[OLED] SSD1306 initialized OK."));
  Serial.println(F("=== Radar Setup Done ==="));

  // --- Typing splash for "Alpha Electronz" ---
  showTypingAnimation("Alpha Electronz", 5000); // ~5 seconds total

  // Then show radar initializing and start normal project
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("Radar Initializing..."));
  display.display();
  delay(1000);
}

void loop()
{
  // Move servo to current angle
  radarServo.write(servoAngle);
  Serial.print(F("[SERVO] Angle: "));
  Serial.println(servoAngle);
  delay(15);  // small delay for smooth motion

  // Read distance
  float distance = readDistanceCm();
  bool objectDetected = false;

  if (distance > 0 && distance <= MAX_DISTANCE_CM) {
    objectDetected = true;
    Serial.print(F("[OBJ] Detected at "));
    Serial.print(distance);
    Serial.println(F(" cm"));
  } else {
    Serial.println(F("[OBJ] No valid object in range"));
    distance = MAX_DISTANCE_CM; // clamp for drawing
  }

  // Prepare drawing
  display.clearDisplay();

  // Draw grid
  drawRadarGrid();

  // Calculate radar line angle
  float rad = servoAngleToRad(servoAngle);

  // End point of radar sweep line (max radius)
  int endX = ORIGIN_X + (int)(RADAR_RADIUS * cos(rad));
  int endY = ORIGIN_Y - (int)(RADAR_RADIUS * sin(rad)); // minus because screen Y grows downward

  // Draw sweeping line
  display.drawLine(ORIGIN_X, ORIGIN_Y, endX, endY, WHITE);

  // If object detected, draw a blip on the line
  if (objectDetected) {
    float normalized = distance / MAX_DISTANCE_CM;
    if (normalized > 1.0) normalized = 1.0;
    int rTarget = (int)(RADAR_RADIUS * normalized);

    int objX = ORIGIN_X + (int)(rTarget * cos(rad));
    int objY = ORIGIN_Y - (int)(rTarget * sin(rad));

    display.fillCircle(objX, objY, 2, WHITE);

    // Show text info
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("OBJ: "));
    display.print((int)distance);
    display.println(F("cm"));

    display.setCursor(0, 10);
    display.print(F("Angle: "));
    display.print(servoAngle);
    display.println(F(" deg"));
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("No object"));
    display.setCursor(0, 10);
    display.print(F("Angle: "));
    display.print(servoAngle);
    display.println(F(" deg"));
  }

  display.display();

  // Update servo angle for next frame (back-and-forth sweep)
  if (sweepingForward) {
    servoAngle += servoStep;
    if (servoAngle >= MAX_ANGLE) {
      servoAngle = MAX_ANGLE;
      sweepingForward = false;
      Serial.println(F("[SERVO] Reached max, reversing sweep"));
    }
  } else {
    servoAngle -= servoStep;
    if (servoAngle <= MIN_ANGLE) {
      servoAngle = MIN_ANGLE;
      sweepingForward = true;
      Serial.println(F("[SERVO] Reached min, reversing sweep"));
    }
  }

  // Small delay to control sweep speed (tune as you like)
  delay(10);
}
