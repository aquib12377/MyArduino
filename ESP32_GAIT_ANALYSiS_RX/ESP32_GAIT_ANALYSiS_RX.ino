n mvl]\26262 y
/*
    ESP-NOW Broadcast FSR Logger (Slave)
    - Uses ESP32_NOW wrapper
    - Receives FsrPacket via broadcast on channel 6
    - Logs each packet as CSV line to /fsr_log.csv on SD card

    SD wiring (default SPI on ESP32):
      CS   -> GPIO 5
      MOSI -> GPIO 23
      MISO -> GPIO 19
      SCK  -> GPIO 18
*/

#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>    // MAC2STR / MACSTR

#include <vector>

// ----- SD & FS -----
#include "FS.h"
#include "SD.h"
#include "SPI.h" uz

/* Definitions */

#define ESPNOW_WIFI_CHANNEL 6
#define SD_CS_PIN 5
static const char *LOG_FILE_PATH = "/fsr_log.csv";

/* FSR Packet (MUST MATCH SENDER) */

#pragma pack(push, 1)
struct FsrPacket {
  uint8_t  magic[4];     // 'F','S','R','1'
  uint32_t seq;
  uint32_t msec;
  uint16_t raw[5];
  float    volt[5];
};
#pragma pack(pop)

/* Globals */

// Simple single-slot buffer for last received packet
 bool      g_hasPacket = false;
 FsrPacket g_lastPkt;
 uint8_t   g_lastMac[6];

bool g_sdOk = false;

/* SD Helpers */

bool fileExists(const char *path) {
  return SD.exists(path);
}

bool appendLineToFile(const char *path, const String &line) {
  if (!g_sdOk) return false;

  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    Serial.println(F("[SD] Failed to open file for appending"));
    return false;
  }
  bool ok = file.print(line);
  file.close();

  if (!ok) {
    Serial.println(F("[SD] Append failed"));
  }
  return ok;
}

bool setupSdCard() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("[SD] Card Mount Failed"));
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println(F("[SD] No SD card attached"));
    return false;
  }

  Serial.print(F("[SD] Card Type: "));
  if      (cardType == CARD_MMC)  Serial.println(F("MMC"));
  else if (cardType == CARD_SD)   Serial.println(F("SDSC"));
  else if (cardType == CARD_SDHC) Serial.println(F("SDHC"));
  else                            Serial.println(F("UNKNOWN"));

  uint64_t cardSize = SD.cardSize() / (1024ULL * 1024ULL);
  Serial.printf("[SD] Card Size: %llu MB\n", cardSize);

  // If log file doesn't exist, create it and write CSV header
  if (!fileExists(LOG_FILE_PATH)) {
    Serial.println(F("[SD] Creating log file with header"));
    File file = SD.open(LOG_FILE_PATH, FILE_WRITE);
    if (!file) {
      Serial.println(F("[SD] Failed to create log file"));
      return false;
    }

    file.println(
      "seq,msec,mac,"
      "raw0,raw1,raw2,raw3,raw4,"
      "volt0,volt1,volt2,volt3,volt4"
    );
    file.close();
  }

  return true;
}

/* ESP-NOW Peer Class */

class ESP_NOW_Peer_Class : public ESP_NOW_Peer {
public:
  // Constructor of the class
  ESP_NOW_Peer_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk)
    : ESP_NOW_Peer(mac_addr, channel, iface, lmk) {}

  ~ESP_NOW_Peer_Class() {}

  bool add_peer() {
    if (!add()) {
      log_e("Failed to register the broadcast/master peer");
      return false;
    }
    return true;
  }

  // Called when this peer receives a message
  void onReceive(const uint8_t *data, size_t len, bool broadcast) override {
    Serial.printf("Received %u bytes from " MACSTR " (%s)\n",
                  (unsigned)len, MAC2STR(addr()), broadcast ? "broadcast" : "unicast");

    if (len != sizeof(FsrPacket)) {
      Serial.printf("[RECV] Unexpected size %u, expected %u\n",
                    (unsigned)len, (unsigned)sizeof(FsrPacket));
      return;
    }

    FsrPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));

    if (!(pkt.magic[0]=='F' && pkt.magic[1]=='S' && pkt.magic[2]=='R' && pkt.magic[3]=='1')) {
      Serial.println(F("[RECV] Magic mismatch; ignoring"));
      return;
    }

    // Copy to global buffer for processing in loop()
    noInterrupts();
    memcpy((void*)g_lastMac, addr(), 6);
    memcpy((void*)&g_lastPkt, &pkt, sizeof(FsrPacket));
    g_hasPacket = true;
    interrupts();

    Serial.printf("[RECV] seq=%lu ms=%lu\n",
                  (unsigned long)pkt.seq,
                  (unsigned long)pkt.msec);
  }
};

/* Global list of masters */

std::vector<ESP_NOW_Peer_Class> masters;

/* Callback: when unknown peer sends a message */

void register_new_master(const esp_now_recv_info_t *info, const uint8_t *data, int len, void *arg) {
  if (memcmp(info->des_addr, ESP_NOW.BROADCAST_ADDR, 6) == 0) {
    Serial.printf("Unknown peer " MACSTR " sent a broadcast message\n", MAC2STR(info->src_addr));
    Serial.println("Registering the peer as a master");

    ESP_NOW_Peer_Class new_master(info->src_addr, ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);

    masters.push_back(new_master);
    if (!masters.back().add_peer()) {
      Serial.println("Failed to register the new master");
      return;
    }
  } else {
    // Only interested in broadcast messages here
    log_v("Received a unicast message from " MACSTR, MAC2STR(info->src_addr));
    log_v("Ignoring the message");
  }
}

/* Main */

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("ESP-NOW FSR Logger - Broadcast Slave");
  Serial.println("Wi-Fi parameters:");

  // Wi-Fi init
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // SD init
  g_sdOk = setupSdCard();
  if (!g_sdOk) {
    Serial.println(F("[SD] WARNING: SD not OK, logging disabled!"));
  }

  // ESP-NOW init
  if (!ESP_NOW.begin()) {
    Serial.println("Failed to initialize ESP-NOW");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // Register callback for unknown peers (masters)
  ESP_NOW.onNewPeer(register_new_master, NULL);

  Serial.println("Setup complete. Waiting for broadcast FSR packets...");
}

void loop() {
  if (g_hasPacket) {
    // Take local copy
    noInterrupts();
    FsrPacket pkt = g_lastPkt;
    uint8_t mac[6];
    memcpy(mac, (const void*)g_lastMac, 6);
    g_hasPacket = false;
    interrupts();

    // Build MAC string
    char macStrBuf[18];
    snprintf(macStrBuf, sizeof(macStrBuf),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    String macStr = String(macStrBuf);

    // Debug print
    Serial.print("[LOG] from ");
    Serial.print(macStr);
    Serial.print(" seq=");  Serial.print(pkt.seq);
    Serial.print(" ms=");   Serial.print(pkt.msec);
    Serial.print(" RAW=");
    for (int i = 0; i < 5; ++i) {
      Serial.print(pkt.raw[i]);
      if (i < 4) Serial.print(',');
    }
    Serial.print("  V=");
    for (int i = 0; i < 5; ++i) {
      Serial.print(pkt.volt[i], 3);
      if (i < 4) Serial.print(',');
    }
    Serial.println();

    // Build CSV line
    String line;
    line.reserve(128);
    line += String(pkt.seq);
    line += ',';
    line += String(pkt.msec);
    line += ',';
    line += macStr;
    line += ',';

    // RAW values
    for (int i = 0; i < 5; ++i) {
      line += String(pkt.raw[i]);
      if (i < 4) line += ',';
    }
    line += ',';

    // Volt values
    for (int i = 0; i < 5; ++i) {
      line += String(pkt.volt[i], 4);
      if (i < 4) line += ',';
    }
    line += "\n";

    // Append to CSV
    if (g_sdOk) {
      if (!appendLineToFile(LOG_FILE_PATH, line)) {
        Serial.println(F("[SD] Error writing log line"));
      } else {
        Serial.println(F("[SD] Logged to CSV"));
      }
    } else {
      Serial.println(F("[SD] Skipped logging (SD not OK)"));
    }
  }

  delay(5);   // Small delay so loop isn't too tight
}
