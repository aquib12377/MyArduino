#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT Setup
#define DHTPIN 4
#define DHTTYPE DHT11   // Change to DHT22 if using DHT22
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
}

void loop() {

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 25);
    display.println("Sensor Error!");
    display.display();
    delay(2000);
    return;
  }

  display.clearDisplay();

  // Title
  display.setTextSize(1);
  display.setCursor(20, 0);
  display.println("Temperature & Humidity");

  // Temperature
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("T:");
  display.print(temp);
  display.println("C");

  // Humidity
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print("H:");
  display.print(hum);
  display.println("%");

  display.display();

  delay(2000);  // Update every 2 seconds
}
