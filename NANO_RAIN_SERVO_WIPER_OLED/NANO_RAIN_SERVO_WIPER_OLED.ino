#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

// -------------------- OLED CONFIG --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); // -1 = no reset pin

// -------------------- PINS --------------------
const int RAIN_PIN  = 12;   // Rain sensor DIGITAL output
const int SERVO_PIN = 3;    // MG90 signal

// Digital rain sensor is usually ACTIVE LOW (0 = rain, 1 = dry)
const int RAIN_ACTIVE_LEVEL = LOW;

// -------------------- RAIN STATE LOGIC --------------------
enum RainState : uint8_t { RAIN_DRY, RAIN_WET };
RainState rainState = RAIN_DRY;

// -------------------- TIMING --------------------
unsigned long lastSensorReadMs = 0;
const unsigned long SENSOR_INTERVAL_MS = 200;

unsigned long lastAnimMs = 0;
unsigned long animIntervalMs = 60;

// -------------------- RAIN ANIMATION --------------------
struct Drop {
  int x;
  int y;
};

const int MAX_DROPS = 16;
Drop drops[MAX_DROPS];

void initDrops() {
  for (int i = 0; i < MAX_DROPS; i++) {
    drops[i].x = random(0, SCREEN_WIDTH);
    drops[i].y = random(-SCREEN_HEIGHT, 0);
  }
}

void updateDrops(int activeDrops) {
  for (int i = 0; i < activeDrops; i++) {
    int speed = 2; // constant speed for now
    drops[i].y += speed;
    if (drops[i].y > SCREEN_HEIGHT) {
      drops[i].y = random(-20, 0);
      drops[i].x = random(0, SCREEN_WIDTH);
    }
  }
}

// -------------------- WIPER ANIMATION + SERVO --------------------
Servo wiperServo;

const int WIPER_MIN_ANGLE = 40;   // degrees (tune for your linkage)
const int WIPER_MAX_ANGLE = 140;  // degrees
int wiperAngle = WIPER_MIN_ANGLE;
int wiperDir   = 1;               // 1 = increasing angle, -1 = decreasing

// -------------------- SUN ANIMATION --------------------
int sunPhase = 0;   // for rotating rays

// -------------------- RAIN STATE --------------------
void updateRainStateDigital(int level) {
  RainState newState;
  bool isWet = (level == RAIN_ACTIVE_LEVEL); // for active LOW sensor

  if (isWet) {
    newState = RAIN_WET;
  } else {
    newState = RAIN_DRY;
  }

  if (newState != rainState) {
    rainState = newState;
    Serial.print(F("[RAIN] State changed to: "));
    Serial.println(rainState == RAIN_WET ? "WET" : "DRY");

    // When it becomes dry, park the wiper at min angle
    if (rainState == RAIN_DRY) {
      wiperAngle = WIPER_MIN_ANGLE;
      wiperServo.write(wiperAngle);
    }
  }
}

// -------------------- STEP ANIMATIONS --------------------
void stepAnimations() {
  if (rainState == RAIN_WET) {
    // Rain drops
    updateDrops(MAX_DROPS);

    // Wiper sweep (OLED + SERVO in sync)
    wiperAngle += wiperDir * 3;   // 3 degrees per frame
    if (wiperAngle >= WIPER_MAX_ANGLE) {
      wiperAngle = WIPER_MAX_ANGLE;
      wiperDir   = -1;
    } else if (wiperAngle <= WIPER_MIN_ANGLE) {
      wiperAngle = WIPER_MIN_ANGLE;
      wiperDir   = 1;
    }

    // Move real servo exactly to the same angle as drawn wiper
    wiperServo.write(wiperAngle);

  } else {
    // Sun ray rotation
    sunPhase = (sunPhase + 1) % 16;
  }
}

// -------------------- DRAW HELPER: WIPER --------------------
void drawWiper() {
  int pivotX = SCREEN_WIDTH / 2;
  int pivotY = SCREEN_HEIGHT - 4;
  int length = 40;

  float rad = wiperAngle * PI / 180.0;
  int endX = pivotX + (int)(length * cos(rad));
  int endY = pivotY - (int)(length * sin(rad));

  // Thicker wiper line
  display.drawLine(pivotX, pivotY, endX, endY, WHITE);
  display.drawLine(pivotX - 1, pivotY, endX - 1, endY, WHITE);
}

// -------------------- DRAW HELPER: SUN --------------------
void drawSun() {
  int cx = SCREEN_WIDTH - 26;
  int cy = 22;
  int radius = 9;

  // Sun core
  display.fillCircle(cx, cy, radius, WHITE);

  // Rays - rotate slightly using sunPhase
  for (int i = 0; i < 8; i++) {
    float baseAngleDeg = i * 45;            // 0,45,90,...
    float angleDeg = baseAngleDeg + sunPhase * 4; // small rotation
    float rad = angleDeg * PI / 180.0;

    int innerR = radius + 2;
    int outerR = radius + 7;

    int x1 = cx + (int)(innerR * cos(rad));
    int y1 = cy + (int)(innerR * sin(rad));
    int x2 = cx + (int)(outerR * cos(rad));
    int y2 = cy + (int)(outerR * sin(rad));

    display.drawLine(x1, y1, x2, y2, WHITE);
  }
}

// -------------------- DRAW SCENE --------------------
void drawScene() {
  display.clearDisplay();

  // "Windshield" frame
  int wx = 4;
  int wy = 18;
  int ww = SCREEN_WIDTH - 8;
  int wh = SCREEN_HEIGHT - 22;
  display.drawRoundRect(wx, wy, ww, wh, 4, WHITE);

  if (rainState == RAIN_WET) {
    // --- Rain drops inside windshield ---
    for (int i = 0; i < MAX_DROPS; i++) {
      int x = drops[i].x;
      int y = drops[i].y;
      if (x >= wx + 2 && x <= wx + ww - 4 &&
          y >= wy + 2 && y <= wy + wh - 4) {
        display.drawLine(x, y, x, y + 3, WHITE);
      }
    }

    // --- Wiper sweeping at bottom ---
    drawWiper();
  } else {
    // --- Sun animation when dry ---
    drawSun();
  }

  display.display();
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);
  Serial.println(F("\n=== Rain Wiper OLED + Servo ==="));

  pinMode(RAIN_PIN, INPUT);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("[OLED] SSD1306 init failed!"));
    while (true) { } // halt
  }

  // Servo init
  wiperServo.attach(SERVO_PIN);
  wiperServo.write(WIPER_MIN_ANGLE);

  // Seed randomness for raindrop positions
  randomSeed(analogRead(A1));
  initDrops();

  // Splash screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println(F("Alpha"));
  display.setCursor(10, 40);
  display.println(F("Electronz"));
  display.display();
  delay(1500);

  drawScene();
}

// -------------------- LOOP --------------------
void loop() {
  unsigned long now = millis();

  // 1) Read rain sensor periodically (just to switch rain / sun)
  if (now - lastSensorReadMs >= SENSOR_INTERVAL_MS) {
    lastSensorReadMs = now;
    int level = digitalRead(RAIN_PIN);  // HIGH/LOW
    updateRainStateDigital(level);
  }

  // 2) Step animation + redraw
  if (now - lastAnimMs >= animIntervalMs) {
    lastAnimMs = now;
    stepAnimations();
    drawScene();
  }
}
