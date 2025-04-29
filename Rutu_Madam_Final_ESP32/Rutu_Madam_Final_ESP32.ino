/**********************************************************************
 *   Water-Quality Logger – ESP32  (ModbusMaster version)
 *   ---------------------------------------------------------------
 *   RS-485 (UART2) :  BGA probe  ID-1  +  Chlorophyll probe  ID-6
 *   Analogue      :  pH, DO, Turbidity, TDS, ORP
 *   1-Wire (GPIO15) : DS18B20
 *   GPS   (UART1) :  NEO-6M  with TinyGPS++
 *********************************************************************/

#include <ModbusMaster.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TinyGPSPlus.h>

/* ---------- pin map ---------- */
#define RS485_RX      16
#define RS485_TX      17
#define RS485_DE_RE    4      // HIGH = TX, LOW = RX
#define GPS_RX        33
#define GPS_TX        32
#define ONE_WIRE_PIN  15

/* ---------- analogue channels ---------- */
#define PIN_PH        36   // ADC1_CH0
#define PIN_DO        39   // ADC1_CH3
#define PIN_TURBID    34   // ADC1_CH6
#define PIN_TDS       35   // ADC1_CH7
#define PIN_ORP       25   // ADC2_CH8

/* ---------- constants / calibration ---------- */
constexpr uint8_t  ID_BGA   = 1;
constexpr uint8_t  ID_CHLO  = 6;
constexpr uint32_t MODBUS_BAUD = 9600;
constexpr float    VREF     = 3.30f;

constexpr float PH_SLOPE    = 3.5f;
constexpr float PH_OFFSET   = 0.0f;

constexpr float DO_SLOPE    = 7.43f;
constexpr float DO_OFFSET   = 0.0f;

constexpr float TU_A = -1120.4f, TU_B = 5742.3f, TU_C = -4352.9f;
constexpr float TDS_K = 133.42f,  TDS_B = -255.86f,  TDS_C = 857.39f;
constexpr float ORP_OFFSET = 1500.0f;

/* ---------- global objects ---------- */
HardwareSerial RS485Serial(2);
ModbusMaster   bga;      // node for ID-1
ModbusMaster   chlo;     // node for ID-6
TinyGPSPlus    gps;
OneWire        oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);

float algaeCells = NAN, chlUgL = NAN, chlTemp = NAN, waterTempDS = NAN;
uint32_t lineCounter = 0;

/* ---------- RS-485 direction control ---------- */
void preTransmission()  { digitalWrite(RS485_DE_RE, LOW); }
void postTransmission() { digitalWrite(RS485_DE_RE, HIGH);  }

/* ---------- helper ---------- */
static float beFloat(uint16_t h, uint16_t l)   // big-endian words → float
{
  uint32_t raw = (uint32_t)h << 16 | l;
  float f;  memcpy(&f, &raw, 4);
  return f;
}

static float readVoltage(uint8_t pin)
{
  uint16_t raw = analogRead(pin);             // 0-4095
  return (raw / 4095.0f) * VREF;
}

/* ---------- setup ---------- */
void setup()
{
  
  Serial.begin(115200);
  delay(400);

  /* Analogue + DS18B20 */
  analogReadResolution(12);
  ds18b20.begin();

  /* GPS */
  Serial1.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  /* RS-485 */
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);             // start in RX mode
  RS485Serial.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  /* Modbus nodes */
  bga.begin(ID_BGA,  RS485Serial);
  chlo.begin(ID_CHLO, RS485Serial);

  bga.preTransmission(preTransmission);
  bga.postTransmission(postTransmission);
  chlo.preTransmission(preTransmission);
  chlo.postTransmission(postTransmission);

  Serial.println(F("\nTime,Lat,Lon,BGA(cells/mL),Chl(ug/L),"
                   "WaterT(°C),pH,DO(mg/L),Turb(NTU),TDS(ppm),ORP(mV),DS18B20(°C)"));

}

/* ---------- Modbus polling ---------- */
void pollModbus()
{
  uint8_t  res;     // result code
  uint16_t r[4];    // local copy for debug
 // simulate a Modbus transmit burst
  digitalWrite(RS485_DE_RE, HIGH);
  delay(3);                         // 3 ms "transmit"
  digitalWrite(RS485_DE_RE, LOW);         // back to receive
  delay(1000);
  /* ---- BGA (3 words) ---- */
  res = bga.readHoldingRegisters(0x0000, 3);
  if (res == bga.ku8MBSuccess) {
      r[0] = bga.getResponseBuffer(0);
      r[1] = bga.getResponseBuffer(1);
      r[2] = bga.getResponseBuffer(2);
      algaeCells = beFloat(r[0], r[1]);
      chlTemp    = int16_t(r[2]) / 10.0f;
  } else {
      Serial.printf("BGA  Modbus error 0x%02X\n", res);
  }

  /* ---- Chlorophyll (4 words) ---- */
  res = chlo.readHoldingRegisters(0x0000, 4);
  if (res == chlo.ku8MBSuccess) {
      for (int i=0;i<4;i++) r[i] = chlo.getResponseBuffer(i);
      chlUgL = r[0] / powf(10, r[1]);
      float t = r[2] / powf(10, r[3]);
      if (!isnan(t)) chlTemp = t;
  } else {
      Serial.printf("CHLO Modbus error 0x%02X\n", res);
  }
}


/* ---------- main loop ---------- */
void loop()
{
  /* ----- regular 2-second cycle ----- */
  static uint32_t next = 0;
  if (millis() < next) {
    /* keep GPS parsing while we wait */
    while (Serial1.available()) gps.encode(Serial1.read());
    return;
  }
  next = millis() + 2000;

  /* poll Modbus sensors */
  pollModbus();

  /* read GPS stream */
  while (Serial1.available()) gps.encode(Serial1.read());

  /* read analogue sensors */
  float vph   = readVoltage(PIN_PH);
  float pH    = PH_SLOPE * vph + PH_OFFSET;

  float vdo   = readVoltage(PIN_DO);
  float DOmgL = DO_SLOPE * vdo + DO_OFFSET;

  float vturb = readVoltage(PIN_TURBID);
  float ntu   = TU_A * vturb * vturb + TU_B * vturb + TU_C;
  ntu = max(ntu, 0.0f);

  ds18b20.requestTemperatures();
  waterTempDS = ds18b20.getTempCByIndex(0);

  float vtds  = readVoltage(PIN_TDS);
  float tCoeff = 1.0 + 0.02f * (waterTempDS - 25.0f);
  float tds   = (TDS_K * vtds * vtds * vtds +
                 TDS_B * vtds * vtds +
                 TDS_C * vtds) / tCoeff;

  float vorp  = readVoltage(PIN_ORP);
  float orp   = (vorp * 1000.0f * 2.0f) - ORP_OFFSET;

  /* print CSV every cycle, header every 30 lines */
  if (++lineCounter % 30 == 0)
    Serial.println(F("Time,Lat,Lon,BGA(cells/mL),Chl(ug/L),"
                     "WaterT(°C),pH,DO(mg/L),Turb(NTU),TDS(ppm),ORP(mV),DS18B20(°C)"));

  Serial.printf("%lu,", millis());

  if (gps.location.isValid())
    Serial.printf("%.6f,%.6f,", gps.location.lat(), gps.location.lng());
  else
    Serial.print("nan,nan,");

  Serial.printf("%.1f,%.1f,%.1f,", algaeCells, chlUgL, chlTemp);
  Serial.printf("%.2f,%.2f,%.1f,%.0f,%.0f,%.1f\n",
                pH, DOmgL, ntu, tds, orp, waterTempDS);
}
