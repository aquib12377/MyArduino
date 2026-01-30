#include <BluetoothSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- OLED ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// If you have TWO OLEDs on I2C, they MUST have different addresses (commonly 0x3C and 0x3D)
#define OLED1_ADDR 0x3C
#define OLED2_ADDR 0x3D

Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Eye animation timing
unsigned long lastBlinkTime = 0;
bool eyesOpen = true;

// Bigger centered eye
const int EYE_CX = SCREEN_WIDTH / 2;   // 64
const int EYE_CY = SCREEN_HEIGHT / 2;  // 32
const int EYE_R  = 18;                 // bigger radius

// ---------------- BLUETOOTH ----------------
BluetoothSerial SerialBT;

// ---------------- L298N PINS ----------------
const int IN1 = 26;
const int IN2 = 27;
const int IN3 = 14;
const int IN4 = 12;

// ---------------- DFPLAYER ----------------
HardwareSerial dfSerial(2);
DFRobotDFPlayerMini mp3;

String cmdBuffer;

// ---------------- MOTOR HELPERS ----------------
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void left() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void right() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

// ---------------- OLED EYES (PER OLED, CENTERED) ----------------
void drawEyeOpen(Adafruit_SSD1306 &d) {
  d.clearDisplay();
  d.fillCircle(EYE_CX, EYE_CY, EYE_R, SSD1306_WHITE);
  d.display();
}

void drawEyeClosed(Adafruit_SSD1306 &d) {
  d.clearDisplay();
  // closed line centered, scaled for bigger eye
  int w = EYE_R * 2;     // ~28
  int h = 4;
  int x = EYE_CX - (w / 2);
  int y = EYE_CY;
  d.fillRect(x, y, w, h, SSD1306_WHITE);
  d.display();
}

void drawBothEyesOpen() {
  drawEyeOpen(display1);
  drawEyeOpen(display2);
}

void drawBothEyesClosed() {
  drawEyeClosed(display1);
  drawEyeClosed(display2);
}

void updateEyeBlink() {
  unsigned long now = millis();

  if (eyesOpen && (now - lastBlinkTime > 3000)) {
    eyesOpen = false;
    lastBlinkTime = now;
    drawBothEyesClosed();
  }
  else if (!eyesOpen && (now - lastBlinkTime > 200)) {
    eyesOpen = true;
    lastBlinkTime = now;
    drawBothEyesOpen();
  }
}

// ---------------- COMMAND PARSING ----------------
bool startsWithIgnoreCase(const String &s, const String &prefix) {
  if (s.length() < prefix.length()) return false;
  for (int i = 0; i < prefix.length(); i++) {
    if (tolower(s[i]) != tolower(prefix[i])) return false;
  }
  return true;
}

void handleCommand(String c) {
  c.trim();
  if (c.length() == 0) return;

  if (c.equalsIgnoreCase("F")) { forward(); return; }
  if (c.equalsIgnoreCase("B")) { backward(); return; }
  if (c.equalsIgnoreCase("L")) { left(); return; }
  if (c.equalsIgnoreCase("R")) { right(); return; }
  if (c.equalsIgnoreCase("S")) { stopMotors(); return; }

  if (c[0] == 'P' || c[0] == 'p') {
    int t = c.substring(1).toInt();
    if (t >= 1 && t <= 15) mp3.play(t);
    return;
  }

  if (startsWithIgnoreCase(c, "STOPA")) {
    mp3.stop();
    return;
  }

  if (startsWithIgnoreCase(c, "VOL")) {
    int v = c.substring(3).toInt();
    mp3.volume(constrain(v, 0, 30));
    return;
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  SerialBT.begin("ESP32_ROBOT");

  Wire.begin(); // ESP32 default I2C pins: SDA=21, SCL=22

  // OLED1 init
  if (!display1.begin(SSD1306_SWITCHCAPVCC, OLED1_ADDR)) {
    Serial.println("OLED1 not found at 0x3C");
    while (true);
  }

  // OLED2 init
  if (!display2.begin(SSD1306_SWITCHCAPVCC, OLED2_ADDR)) {
    Serial.println("OLED2 not found at 0x3D");
    while (true);
  }

  drawBothEyesOpen();

  // DFPlayer
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);
  if (mp3.begin(dfSerial)) {
    mp3.volume(20);
    mp3.EQ(DFPLAYER_EQ_NORMAL);
  }

  Serial.println("System Ready");
}

// ---------------- LOOP ----------------
void loop() {
  // Bluetooth handling
  while (SerialBT.available()) {
    char ch = (char)SerialBT.read();

    if ((ch == 'F' || ch == 'B' || ch == 'L' || ch == 'R' || ch == 'S') && cmdBuffer.length() == 0) {
      String one; one += ch;
      handleCommand(one);
      continue;
    }

    if (ch == '\n' || ch == '\r') {
      handleCommand(cmdBuffer);
      cmdBuffer = "";
    } else {
      cmdBuffer += ch;
    }
  }

  // Eye blinking animation (both OLEDs, centered bigger eye)
  updateEyeBlink();
}
