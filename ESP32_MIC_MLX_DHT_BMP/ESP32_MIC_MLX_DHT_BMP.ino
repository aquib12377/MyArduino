  #include <Wire.h>
  #include "MAX30105.h"
  #include "heartRate.h"
  #include <Adafruit_BMP085.h>
  #include "DHT.h"

  // --- MAX30105 (Heart Rate) ---
  MAX30105 particleSensor;
  const byte RATE_SIZE = 4;
  byte rates[RATE_SIZE];
  byte rateSpot = 0;
  long lastBeat = 0;
  float beatsPerMinute;
  int beatAvg;

  // --- Mic Sensor ---
  const int micPin = 34;
  int micValue = 0;

  // --- DHT11 Sensor ---
  #define DHTPIN 14
  #define DHTTYPE DHT11
  DHT dht(DHTPIN, DHTTYPE);
  float temperatureDHT;
  float humidityDHT;

  // --- BMP180 Sensor ---
  Adafruit_BMP085 bmp;
  float temperatureBMP;
  float pressureBMP;

  // --- Timing ---
  unsigned long previousMillis = 0;
  const unsigned long interval = 1000; // 1 second

  void setup()
  {
    Serial.begin(115200);
    Serial.println("Initializing...");

    // --- MAX30105 Init ---
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
    {
      Serial.println("MAX30105 was not found. Please check wiring/power.");
      while (1);
    }
    Serial.println("Place your index finger on the sensor with steady pressure.");
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeGreen(0);

    // --- DHT11 Init ---
    dht.begin();

    // --- BMP180 Init ---
    if (!bmp.begin())
    {
      Serial.println("Could not find a valid BMP180 sensor, check wiring!");
      while (1);
    }
  }

  void loop()
  {
    unsigned long currentMillis = millis();

    // --- MAX30105 Heartbeat Reading ---
    long irValue = particleSensor.getIR();

    if (checkForBeat(irValue) == true)
    {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20)
      {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }

    // --- Run other sensor readings and print every 1 second ---
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis = currentMillis;

      // --- Mic Sensor Reading ---
      micValue = analogRead(micPin);

      // --- DHT11 Reading ---
      temperatureDHT = dht.readTemperature();
      humidityDHT = dht.readHumidity();

      // --- BMP180 Reading ---
      temperatureBMP = bmp.readTemperature();
      pressureBMP = bmp.readPressure() / 100.0; // hPa

      // --- Serial Output ---
      Serial.print("IR=");
      Serial.print(irValue);
      Serial.print(", BPM=");
      Serial.print(beatsPerMinute);
      Serial.print(", Avg BPM=");
      Serial.print(beatAvg);
      if (irValue < 50000) Serial.print(" No finger?");
      Serial.println();

      Serial.print("Mic=");
      Serial.print(micValue);
      Serial.print(" | DHT11: Temp=");
      Serial.print(temperatureDHT);
      Serial.print("°C, Humidity=");
      Serial.print(humidityDHT);
      Serial.print("% | BMP180: Temp=");
      Serial.print(temperatureBMP);
      Serial.print("°C, Pressure=");
      Serial.print(pressureBMP);
      Serial.println(" hPa");
    }
  }
