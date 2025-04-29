#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// ——— Pin definitions ———
#define ONE_WIRE_BUS 2     // DS18B20 data pin
#define DHTPIN       4     // DHT11 data pin
#define DHTTYPE    DHT11    // DHT sensor type

// ——— OneWire & DallasTemperature ———
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ——— DHT11 ———
DHT dht(DHTPIN, DHTTYPE);

// Storage for the two sensor addresses
DeviceAddress tempDeviceAddress[2];

// Helper to print a 64-bit ROM address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print('0');
    Serial.print(deviceAddress[i], HEX);
  }
}

void setup() {
  Serial.begin(9600);
  sensors.begin();
  dht.begin();

  // 1) Print CSV header
  Serial.println("timestamp_ms,ds1_temp_C,ds2_temp_C,dht_temp_C,dht_hum_%");
  // 2) Print handshake
  Serial.println("ARDUINO_READY");

  // Discover and print the addresses of the two DS18B20 sensors
  int count = sensors.getDeviceCount();
  Serial.print("Found ");
  Serial.print(count);
  Serial.println(" device(s) on OneWire bus.");
  for (int i = 0; i < count && i < 2; i++) {
    if (sensors.getAddress(tempDeviceAddress[i], i)) {
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.print(" address: ");
      printAddress(tempDeviceAddress[i]);
      Serial.println();
    } else {
      Serial.print("ERROR: Could not read address for sensor ");
      Serial.println(i);
    }
  }
}

void loop() {
  // Request all DS18B20 temperatures
  sensors.requestTemperatures();

  float ds1 = sensors.getTempC(tempDeviceAddress[0]);
  float ds2 = sensors.getTempC(tempDeviceAddress[1]);

  // Read DHT11
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Check for errors
  if (isnan(ds1) || isnan(ds2) || isnan(h) || isnan(t)) {
    Serial.println("ERROR_READING_SENSORS");
  } else {
    unsigned long ts = millis();
    Serial.print(ts); Serial.print(',');
    Serial.print(ds1, 2); Serial.print(',');
    Serial.print(ds2, 2); Serial.print(',');
    Serial.println(h,   2);
  }

  delay(2000);
}
