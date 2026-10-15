/* ============================================================
 *  Project : LoRa Health / Asset Tracker — RX node
 *  Hardware: ESP32 DevKit V1 + SX1278 LoRa (433 MHz)
 *  Output  : Serial Monitor @ 115200 baud
 * ============================================================ */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// LoRa settings — MUST match TX
#define LORA_SCK      18
#define LORA_MISO     19
#define LORA_MOSI     23
#define LORA_SS        5
#define LORA_RST      14
#define LORA_DIO0      2
#define LORA_FREQ     433E6
#define LORA_SF        9
#define LORA_BW       125E3
#define LORA_CR        5
#define LORA_SYNCWORD 0xA5

#define STALE_LINK_MS     15000UL
#define LORA_RETRY_MS      3000UL

#define LOG(tag, ...) do { \
        Serial.print("[" tag "] "); \
        Serial.printf(__VA_ARGS__);  \
        Serial.println();            \
    } while (0)

bool     lora_ok       = false;
uint32_t lastRetryMs   = 0;
uint32_t lastPacketMs  = 0;
uint32_t packetsRx     = 0;
uint32_t packetsCrcErr = 0;

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

bool initLoRa() {
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(LORA_FREQ)) return false;
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setSyncWord(LORA_SYNCWORD);
    LoRa.enableCrc();
    LoRa.receive();
    return true;
}

void handleFrame(char *frame, int rssi, float snr) {
    // Frame format: "P,pkt,lat,lng,alt,sats,hr,spo2,tObj,tAmb,vBat,pct,fix*CRC"
    char *star = strrchr(frame, '*');
    if (!star || strlen(star) < 3) {
        LOG("WARN", "Malformed frame: %s", frame);
        return;
    }
    *star = '\0';
    char   *crcHex  = star + 1;
    uint8_t recvCrc = (uint8_t)strtoul(crcHex, nullptr, 16);
    uint8_t calcCrc = crc8((const uint8_t *)frame, strlen(frame));
    if (recvCrc != calcCrc) {
        packetsCrcErr++;
        LOG("WARN", "CRC FAIL  recv=%02X calc=%02X  %s*%s",
                    recvCrc, calcCrc, frame, crcHex);
        return;
    }

    char *tok = strtok(frame, ",");
    if (!tok || strcmp(tok, "P") != 0) { LOG("WARN", "Bad header"); return; }
    char *sPkt  = strtok(nullptr, ",");
    char *sLat  = strtok(nullptr, ",");
    char *sLng  = strtok(nullptr, ",");
    char *sAlt  = strtok(nullptr, ",");
    char *sSat  = strtok(nullptr, ",");
    char *sHr   = strtok(nullptr, ",");
    char *sSpo  = strtok(nullptr, ",");
    char *sTobj = strtok(nullptr, ",");
    char *sTamb = strtok(nullptr, ",");
    char *sVbat = strtok(nullptr, ",");
    char *sPct  = strtok(nullptr, ",");
    char *sFix  = strtok(nullptr, ",");
    if (!sPkt || !sLat || !sLng || !sAlt || !sSat || !sHr || !sSpo
        || !sTobj || !sTamb || !sVbat || !sPct || !sFix) {
        LOG("WARN", "Missing field(s)"); return;
    }

    unsigned long pkt  = strtoul(sPkt, nullptr, 10);
    double        lat  = atof(sLat);
    double        lng  = atof(sLng);
    float         alt  = atof(sAlt);
    int           sats = atoi(sSat);
    int           hr   = atoi(sHr);
    int           spo2 = atoi(sSpo);
    float         tObj = atof(sTobj);
    float         tAmb = atof(sTamb);
    float         vBat = atof(sVbat);
    int           pct  = atoi(sPct);
    int           fix  = atoi(sFix);

    packetsRx++;
    lastPacketMs = millis();

    Serial.println(F("---------------- PACKET ----------------"));
    Serial.printf ("Packet #     : %lu\n", pkt);
    Serial.printf ("Total RX     : %lu  (CRC errors: %lu)\n",
                   (unsigned long)packetsRx, (unsigned long)packetsCrcErr);
    Serial.printf ("Link         : RSSI %d dBm   SNR %.1f dB\n", rssi, snr);
    if (fix && (lat != 0.0 || lng != 0.0))
        Serial.printf("GPS          : %.6f, %.6f  alt %.1f m  sats %d  FIX\n",
                      lat, lng, alt, sats);
    else
        Serial.printf("GPS          : no fix  sats %d\n", sats);
    if (hr   > 0)     Serial.printf("Heart rate   : %d bpm\n", hr);
    else              Serial.println("Heart rate   : --");
    if (spo2 > 0)     Serial.printf("SpO2         : %d %%\n", spo2);
    else              Serial.println("SpO2         : --");
    if (tObj > -100)  Serial.printf("Body temp    : %.2f C\n", tObj);
    else              Serial.println("Body temp    : --");
    if (tAmb > -100)  Serial.printf("Ambient temp : %.2f C\n", tAmb);
    else              Serial.println("Ambient temp : --");
    Serial.printf   ("Battery      : %.2f V  (%d %%)\n", vBat, pct);
    Serial.println  (F("----------------------------------------"));
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    LOG("BOOT", "LoRa-RX v1.0.0 @ 115200");
    LOG("BOOT", "LoRa %.0f MHz SF%u BW%.0fk CR4/%u SW0x%02X",
        (float)(LORA_FREQ / 1E6), (unsigned)LORA_SF,
        (float)(LORA_BW / 1E3), (unsigned)LORA_CR, LORA_SYNCWORD);

    lora_ok = initLoRa();
    LOG("INIT", "LoRa SX1278: %s", lora_ok ? "OK" : "FAIL");
    if (!lora_ok) LOG("WARN", "Will retry every %lu ms",
                              (unsigned long)LORA_RETRY_MS);
}

void loop() {
    if (!lora_ok) {
        if (millis() - lastRetryMs >= LORA_RETRY_MS) {
            lastRetryMs = millis();
            lora_ok = initLoRa();
            if (lora_ok) LOG("RECOVERY", "LoRa came up");
        }
        return;
    }

    int sz = LoRa.parsePacket();
    if (sz > 0) {
        static char buf[200];
        int n = 0;
        while (LoRa.available() && n < (int)sizeof(buf) - 1)
            buf[n++] = (char)LoRa.read();
        buf[n] = '\0';
        int   rssi = LoRa.packetRssi();
        float snr  = LoRa.packetSnr();
        handleFrame(buf, rssi, snr);
    }

    // Stale-link diagnostic
    static uint32_t lastStaleMs = 0;
    if (packetsRx > 0
        && millis() - lastPacketMs > STALE_LINK_MS
        && millis() - lastStaleMs  > STALE_LINK_MS) {
        lastStaleMs = millis();
        LOG("WARN", "No packets for %lu ms",
                    (unsigned long)(millis() - lastPacketMs));
    }
}