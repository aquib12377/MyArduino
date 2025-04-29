#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display settings
#define SCREEN_WIDTH 128       // OLED display width, in pixels
#define SCREEN_HEIGHT  64      // OLED display height, in pixels
#define OLED_RESET     -1      // Reset pin # (or -1 if sharing Arduino reset)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MLX90614 IR temperature sensor
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// MQ‑135 gas sensor pin
const int MQ135_PIN = 34;      // ADC1 channel 2

void setup() {
  Serial.begin(115200);
  Wire.begin();                // Join I2C bus

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: SSD1306 allocation failed");
    while (true) delay(10);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Initialize MLX90614
  if (!mlx.begin()) {
    Serial.println("ERROR: Could not find MLX90614 sensor!");
    while (true) delay(10);
  }

  // Give sensors a moment
  delay(100);
}

void loop() {
  // 1) Read temperatures from MLX90614
  float ambientTemp = mlx.readAmbientTempC();
  float objectTemp  = mlx.readObjectTempC();

  // 2) Read MQ‑135 raw value and convert to voltage
  int rawValue = analogRead(MQ135_PIN);                    // 0–4095 on ESP32
  float voltage = (rawValue / 4095.0f) * 3.3f;             // assuming 3.3 V reference

  // 3) Print to Serial for debugging
  Serial.print("Ambient Temp: "); Serial.print(ambientTemp, 1); Serial.println(" °C");
  Serial.print("Object  Temp: "); Serial.print(objectTemp,  1); Serial.println(" °C");
  Serial.print("MQ‑135 Raw: ");    Serial.print(rawValue);        Serial.print("  Voltage: ");
  Serial.print(voltage, 2);         Serial.println(" V");
  Serial.println();

  // 4) Display on OLED
  display.clearDisplay();
  display.setCursor(0, 0);

  display.print("Amb Temp: ");
  display.print(ambientTemp, 1);
  display.println(" C");

  display.print("Obj Temp: ");
  display.print(objectTemp, 1);
  display.println(" C");

  display.print("Gas Raw: ");
  display.println(rawValue);

  display.print("Gas V:   ");
  display.print(voltage, 2);
  display.println(" V");

  display.display();

  delay(1000);  // update once per second
}
