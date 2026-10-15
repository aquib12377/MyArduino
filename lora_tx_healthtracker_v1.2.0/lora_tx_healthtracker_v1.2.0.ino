/* ============================================================
 *  Project : LoRa Health / Asset Tracker — TX node  (v1.2.0)
 *  Changes : v1.2.0 — MLX90614 moved to Wire1 (separate I2C bus)
 *                     to isolate it from MAX30100 on Wire.
 *                     Wire1 SDA=25, SCL=26 (change to suit your wiring).
 *                     LoRa TX now async (non-blocking) so pox.update()
 *                     is never starved during transmit.
 *  Previous: auto-detect MAX30100 / MAX30102
 *            I2C @ 100 kHz to support MLX90614 SMBus
 *            MLX warm-up + NaN retry
 *            I2C bus scanner at boot
 * ============================================================ */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>

// --- MAX30100 path (oxullo lib) ---
#include <MAX30100_PulseOximeter.h>
// --- MAX30102 path (SparkFun lib) ---
#include "MAX30105.h"
#include "spo2_algorithm.h"

#include <Adafruit_MLX90614.h>
#include <TinyGPSPlus.h>

// -------------------- Build info --------------------
#define FW_NAME      "LoRa-TX-HealthTracker"
#define FW_VERSION   "1.2.0"

// Master switch — set 1 when MLX90614 is wired up on Wire1.
#define MLX_ENABLED  1

// -------------------- LoRa (SX1278) -----------------
#define LORA_SCK       18
#define LORA_MISO      19
#define LORA_MOSI      23
#define LORA_SS         5
#define LORA_RST       14
#define LORA_DIO0       2
#define LORA_FREQ      433E6
#define LORA_TX_PWR    17
#define LORA_SF         9
#define LORA_BW        125E3
#define LORA_CR         5
#define LORA_SYNCWORD  0xA5

// -------------------- I2C bus 0 (Wire) — MAX30100 / MAX30102 ---
// *** WIRE1 *** : Wire keeps the original pins for MAX30100/MAX30102.
#define I2C_SDA        21
#define I2C_SCL        22
#define I2C_CLOCK_HZ   100000UL

// -------------------- I2C bus 1 (Wire1) — MLX90614 only ---------
// *** WIRE1 *** : Pick any two free GPIO pins for the second bus.
//                Avoid pins used by SPI/UART/ADC above.
//                Change these to match your actual wiring.
#define I2C1_SDA       25
#define I2C1_SCL       26
#define I2C1_CLOCK_HZ  100000UL    // MLX90614 SMBus spec: max 100 kHz

// -------------------- HR sensor (on Wire) -----------
#define HR_I2C_ADDR    0x57
#define PART_ID_REG    0xFF
#define PID_MAX30100   0x11
#define PID_MAX30102   0x15

// -------------------- GPS NEO-6M --------------------
#define GPS_RX_PIN     16
#define GPS_TX_PIN     17
#define GPS_BAUD       9600

// -------------------- Battery (3S Li-ion) -----------
#define BAT_ADC_PIN      34
#define BAT_DIV_RATIO     5.0f
#define BAT_VMIN          9.00f
#define BAT_VMAX         12.60f
#define ADC_REF_V         3.30f
#define ADC_MAX_COUNT  4095.0f
#define ADC_SAMPLES       16

// -------------------- Timing ------------------------
#define TX_INTERVAL_MS      3000UL
#define LORA_RETRY_MS       5000UL
#define STATUS_INTERVAL_MS  10000UL
#define SPO2_BUFFER_LEN     100
#define SPO2_SHIFT           25
#define MLX_WARMUP_MS       300UL
#define MLX_READ_RETRIES      3

// -------------------- Sensor type -------------------
enum HRType { HR_NONE = 0, HR_MAX30100 = 100, HR_MAX30102 = 102 };

// -------------------- Globals -----------------------
HardwareSerial      GPSSerial(2);
TinyGPSPlus         gps;

PulseOximeter       pox;                // MAX30100 — uses Wire
MAX30105            particleSensor;     // MAX30102 — uses Wire

// *** WIRE1 *** : mlx now driven by Wire1 via the TwoWire* pointer below.
Adafruit_MLX90614   mlx;

HRType   hrType       = HR_NONE;
bool     mlx_ok       = false;
bool     lora_ok      = false;

// MAX30102 buffers
uint32_t irBuffer [SPO2_BUFFER_LEN];
uint32_t redBuffer[SPO2_BUFFER_LEN];
int      bufferIndex   = 0;
int32_t  heartRate_102 = 0;
int8_t   validHR_102   = 0;
int32_t  spo2_102      = 0;
int8_t   validSpO2_102 = 0;

// Latest validated outputs
float    lastHR        = 0;
int      lastSpO2      = 0;
bool     haveHRReading = false;

// Scheduling
uint32_t lastTxMs      = 0;
uint32_t lastRetryMs   = 0;
uint32_t lastStatusMs  = 0;
uint32_t packetCounter = 0;

// *** WIRE1 *** : second bus instance — ESP32 Arduino core exposes Wire1.
//  Wire  → GPIO 21/22 → MAX30100 or MAX30102
//  Wire1 → GPIO 25/26 → MLX90614
TwoWire *mlxBus = &Wire1;

// -------------------- Logging -----------------------
#define LOG(tag, ...) do { \
        Serial.print("[" tag "] "); \
        Serial.printf(__VA_ARGS__);  \
        Serial.println();            \
    } while (0)

// -------------------- CRC8 (poly 0x07) --------------
uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
    }
    return crc;
}

// -------------------- I2C helpers -------------------
// *** WIRE1 *** : accepts a TwoWire reference so it works on either bus.
bool i2cReadReg(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t &out) {
    bus.beginTransmission(addr);
    bus.write(reg);
    if (bus.endTransmission(false) != 0) return false;
    if (bus.requestFrom((int)addr, 1) != 1) return false;
    out = bus.read();
    return true;
}

// *** WIRE1 *** : scan a specific bus and label it in the log.
void i2cScan(TwoWire &bus, const char *busName, uint32_t clockHz) {
    LOG("INFO", "I2C scan [%s] @ %lu Hz ...", busName, (unsigned long)clockHz);
    uint8_t found = 0;
    for (uint8_t a = 0x03; a < 0x78; a++) {
        bus.beginTransmission(a);
        if (bus.endTransmission() == 0) {
            Serial.printf("       device @ 0x%02X\n", a);
            found++;
        }
    }
    if (!found) LOG("WARN", "[%s] No I2C devices found — check SDA/SCL/power", busName);
    else        LOG("INFO", "[%s] %u device(s) on bus", busName, found);
}

// -------------------- HR-sensor detect (Wire only) --
HRType detectHRSensor() {
    uint8_t pid = 0;
    // *** WIRE1 *** : HR sensor is always on Wire (the primary bus).
    if (!i2cReadReg(Wire, HR_I2C_ADDR, PART_ID_REG, pid)) {
        LOG("WARN", "No I2C device at 0x%02X on Wire — HR sensor missing?", HR_I2C_ADDR);
        return HR_NONE;
    }
    LOG("INFO", "HR sensor PART_ID @ 0xFF = 0x%02X", pid);
    if (pid == PID_MAX30100) return HR_MAX30100;
    if (pid == PID_MAX30102) return HR_MAX30102;
    LOG("WARN", "Unknown PART_ID 0x%02X — treating as MAX30102 (best guess)", pid);
    return HR_MAX30102;
}

// -------------------- Battery -----------------------
float readBatteryVoltage() {
    uint32_t acc = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        acc += analogRead(BAT_ADC_PIN);
        delayMicroseconds(200);
    }
    float raw  = (float)acc / ADC_SAMPLES;
    float vAdc = (raw / ADC_MAX_COUNT) * ADC_REF_V;
    return vAdc * BAT_DIV_RATIO;
}

uint8_t batteryPercent(float vbat) {
    if (vbat <= BAT_VMIN) return 0;
    if (vbat >= BAT_VMAX) return 100;
    return (uint8_t)((vbat - BAT_VMIN) * 100.0f / (BAT_VMAX - BAT_VMIN));
}

// -------------------- Init helpers ------------------
bool initLoRa() {
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(LORA_FREQ)) return false;
    LoRa.setTxPower(LORA_TX_PWR, PA_OUTPUT_PA_BOOST_PIN);
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setSyncWord(LORA_SYNCWORD);
    LoRa.enableCrc();
    return true;
}

bool initMAX30100() {
    if (!pox.begin()) return false;
    pox.setIRLedCurrent(MAX30100_LED_CURR_27_1MA);
    return true;
}

bool initMAX30102() {
    if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) return false;
    particleSensor.setup(60, 4, 2, 100, 411, 4096);
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeIR (0x1F);
    return true;
}

// *** WIRE1 *** : initMLX now starts Wire1 and passes it to mlx.begin().
bool initMLX() {
    // Initialise the second I2C bus on its dedicated pins.
    mlxBus->begin(I2C1_SDA, I2C1_SCL, I2C1_CLOCK_HZ);
    delay(50);   // let bus settle

    // Adafruit_MLX90614::begin() accepts an optional TwoWire* in recent
    // library versions (>= 2.x).  Pass mlxBus so it never touches Wire.
    if (!mlx.begin(0x5A, mlxBus)) return false;

    delay(MLX_WARMUP_MS);
    // Discard first reading — MLX90614 sometimes returns NaN on initial read.
    (void)mlx.readObjectTempC();
    delay(20);
    return true;
}

void initGPS() {
    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

// -------------------- MLX read with retry -----------
bool readMLX(float &tObj, float &tAmb) {
    if (!mlx_ok) return false;
    for (uint8_t i = 0; i < MLX_READ_RETRIES; i++) {
        float o = mlx.readObjectTempC();
        float a = mlx.readAmbientTempC();
        if (!isnan(o) && !isnan(a)
            && o > -40.0f && o < 125.0f
            && a > -40.0f && a < 125.0f) {
            tObj = o; tAmb = a;
            return true;
        }
        delay(15);
    }
    LOG("WARN", "MLX90614 read failed after %u retries", MLX_READ_RETRIES);
    return false;
}

// -------------------- HR-sensor update --------------
void updateHRSensor() {
    if (hrType == HR_MAX30100) {
        pox.update();
        float   hr = pox.getHeartRate();
        uint8_t s  = pox.getSpO2();
        if (hr > 30.0f && hr < 220.0f && s >= 70 && s <= 100) {
            lastHR        = hr;
            lastSpO2      = s;
            haveHRReading = true;
        }
    } else if (hrType == HR_MAX30102) {
        particleSensor.check();
        while (particleSensor.available()) {
            redBuffer[bufferIndex] = particleSensor.getRed();
            irBuffer [bufferIndex] = particleSensor.getIR();
            particleSensor.nextSample();
            bufferIndex++;
            if (bufferIndex >= SPO2_BUFFER_LEN) {
                maxim_heart_rate_and_oxygen_saturation(
                    irBuffer, SPO2_BUFFER_LEN, redBuffer,
                    &spo2_102, &validSpO2_102,
                    &heartRate_102, &validHR_102);
                if (validHR_102 && validSpO2_102
                    && heartRate_102 > 30 && heartRate_102 < 220
                    && spo2_102 >= 70 && spo2_102 <= 100) {
                    lastHR        = (float)heartRate_102;
                    lastSpO2      = (int)spo2_102;
                    haveHRReading = true;
                }
                for (int i = SPO2_SHIFT; i < SPO2_BUFFER_LEN; i++) {
                    redBuffer[i - SPO2_SHIFT] = redBuffer[i];
                    irBuffer [i - SPO2_SHIFT] = irBuffer [i];
                }
                bufferIndex = SPO2_BUFFER_LEN - SPO2_SHIFT;
            }
        }
    }
}

// -------------------- GPS feed ----------------------
void feedGPS() {
    while (GPSSerial.available()) gps.encode(GPSSerial.read());
}

// -------------------- Build & TX --------------------
void buildAndSend() {
    packetCounter++;

    float tObj = NAN, tAmb = NAN;
#if MLX_ENABLED
    readMLX(tObj, tAmb);
#endif

    float   vBat = readBatteryVoltage();
    uint8_t pct  = batteryPercent(vBat);

    bool    fix  = gps.location.isValid();
    double  lat  = fix ? gps.location.lat() : 0.0;
    double  lng  = fix ? gps.location.lng() : 0.0;
    float   alt  = gps.altitude.isValid()   ? gps.altitude.meters() : 0.0f;
    uint8_t sats = gps.satellites.isValid() ? (uint8_t)gps.satellites.value() : 0;

    int hrOut   = haveHRReading ? (int)lastHR : 0;
    int spo2Out = haveHRReading ? lastSpO2    : 0;

    char payload[160];
    int n = snprintf(payload, sizeof(payload),
        "P,%lu,%.6f,%.6f,%.1f,%u,%d,%d,%.2f,%.2f,%.2f,%u,%u",
        (unsigned long)packetCounter,
        lat, lng, alt, sats,
        hrOut, spo2Out,
        isnan(tObj) ? -127.0f : tObj,
        isnan(tAmb) ? -127.0f : tAmb,
        vBat, pct,
        fix ? 1u : 0u);
    if (n <= 0 || n >= (int)sizeof(payload)) {
        LOG("ERROR", "payload buffer overflow"); return;
    }

    uint8_t crc = crc8((const uint8_t *)payload, (size_t)n);
    char frame[180];
    int m = snprintf(frame, sizeof(frame), "%s*%02X", payload, crc);
    if (m <= 0 || m >= (int)sizeof(frame)) {
        LOG("ERROR", "frame buffer overflow"); return;
    }

    LoRa.beginPacket();
    LoRa.write((const uint8_t *)frame, (size_t)m);

    // *** FIX *** : async=true — non-blocking transmit so pox.update()
    //              is never starved while waiting for the radio to finish.
    int ok = LoRa.endPacket(true);

    if (ok) LOG("INFO",  "TX#%lu len=%d  %s",
                          (unsigned long)packetCounter, m, frame);
    else    LOG("ERROR", "LoRa.endPacket() failed for pkt#%lu",
                          (unsigned long)packetCounter);
}

// -------------------- Periodic status ---------------
const char *hrTypeName() {
    switch (hrType) {
        case HR_MAX30100: return "MAX30100";
        case HR_MAX30102: return "MAX30102";
        default:          return "NONE";
    }
}

void printStatus() {
    LOG("INFO", "Sensors  HR=%s  MLX90614=%s  LoRa=%s",
        hrTypeName(),
        mlx_ok  ? "OK" : "FAIL",
        lora_ok ? "OK" : "FAIL");
    LOG("INFO", "GPS  sats=%lu  fix=%d  chars=%lu",
        (unsigned long)gps.satellites.value(),
        gps.location.isValid() ? 1 : 0,
        (unsigned long)gps.charsProcessed());
    if (gps.charsProcessed() < 10)
        LOG("WARN", "No GPS data — check wiring/baud (NEO-6M @ 9600)");
}

// -------------------- Setup -------------------------
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    LOG("BOOT", "%s v%s @ 115200", FW_NAME, FW_VERSION);
    LOG("BOOT", "LoRa %.0f MHz SF%u BW%.0fk CR4/%u SW0x%02X %ddBm",
        (float)(LORA_FREQ / 1E6), (unsigned)LORA_SF,
        (float)(LORA_BW / 1E3),   (unsigned)LORA_CR,
        LORA_SYNCWORD, LORA_TX_PWR);
    LOG("BOOT", "Battery: 3S Li-ion %.2fV–%.2fV via 25V sensor (/%.1f)",
        BAT_VMIN, BAT_VMAX, BAT_DIV_RATIO);
    LOG("BOOT", "TX interval %lu ms  |  Wire I2C %lu Hz  |  Wire1 I2C %lu Hz",
        (unsigned long)TX_INTERVAL_MS,
        (unsigned long)I2C_CLOCK_HZ,
        (unsigned long)I2C1_CLOCK_HZ);

    // *** WIRE1 *** : show which pins each bus uses at boot.
    LOG("BOOT", "Wire  SDA=%d SCL=%d  (MAX30100/MAX30102)",
        I2C_SDA, I2C_SCL);
    LOG("BOOT", "Wire1 SDA=%d SCL=%d  (MLX90614)",
        I2C1_SDA, I2C1_SCL);

    analogReadResolution(12);
    analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);

    // *** WIRE1 *** : only Wire is initialised here.
    //                Wire1 is started inside initMLX() after the scan.
    Wire.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_HZ);
    Wire.setClock(I2C_CLOCK_HZ);
    delay(50);

    // Scan both buses independently so each shows its own devices.
    i2cScan(Wire,  "Wire ",  I2C_CLOCK_HZ);

    // --- HR sensor detect & init (Wire) ---
    hrType = detectHRSensor();
    bool hrOk = false;
    if      (hrType == HR_MAX30100) hrOk = initMAX30100();
    else if (hrType == HR_MAX30102) hrOk = initMAX30102();
    LOG("INIT", "HR sensor (%s) : %s", hrTypeName(), hrOk ? "OK" : "FAIL");
    if (!hrOk) hrType = HR_NONE;

    // --- MLX90614 (Wire1) ---
#if MLX_ENABLED
    // *** WIRE1 *** : initMLX() calls Wire1.begin() internally,
    //                scans Wire1, then calls mlx.begin(0x5A, &Wire1).
    //                A fault on Wire1 cannot affect Wire.
    LOG("INFO", "I2C scan [Wire1] @ %lu Hz ...", (unsigned long)I2C1_CLOCK_HZ);
    mlxBus->begin(I2C1_SDA, I2C1_SCL, I2C1_CLOCK_HZ);
    delay(50);
    i2cScan(*mlxBus, "Wire1", I2C1_CLOCK_HZ);

    mlx_ok = initMLX();
    LOG("INIT", "MLX90614 (Wire1 SDA=%d SCL=%d) : %s",
        I2C1_SDA, I2C1_SCL,
        mlx_ok ? "OK" : "FAIL (check wiring on Wire1)");
#else
    mlx_ok = false;
    LOG("INIT", "MLX90614 : DISABLED in firmware (MLX_ENABLED=0)");
#endif

    // --- GPS ---
    initGPS();
    LOG("INIT", "GPS UART2 @ %d  (RX=%d TX=%d)", GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN);

    // --- LoRa ---
    lora_ok = initLoRa();
    LOG("INIT", "LoRa SX1278: %s", lora_ok ? "OK" : "FAIL");
    if (!lora_ok)
        LOG("WARN", "Will retry LoRa every %lu ms", (unsigned long)LORA_RETRY_MS);
}

// -------------------- Loop --------------------------
void loop() {
    // pox.update() must run as often as possible — no blocking calls in loop.
    if (hrType != HR_NONE) updateHRSensor();
    feedGPS();

    if (!lora_ok && millis() - lastRetryMs >= LORA_RETRY_MS) {
        lastRetryMs = millis();
        lora_ok = initLoRa();
        if (lora_ok) LOG("RECOVERY", "LoRa init succeeded");
    }

    if (lora_ok && millis() - lastTxMs >= TX_INTERVAL_MS) {
        lastTxMs = millis();
        buildAndSend();
    }

    if (millis() - lastStatusMs >= STATUS_INTERVAL_MS) {
        lastStatusMs = millis();
        printStatus();
    }
}
