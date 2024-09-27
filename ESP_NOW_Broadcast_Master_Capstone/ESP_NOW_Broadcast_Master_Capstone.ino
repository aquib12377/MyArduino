#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>  // For the MAC2STR and MACSTR macros

/* Definitions */
#define ESPNOW_WIFI_CHANNEL 6

/* Classes */
class ESP_NOW_Broadcast_Peer : public ESP_NOW_Peer {
public:
  // Constructor of the class using the broadcast address
  ESP_NOW_Broadcast_Peer(uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) : ESP_NOW_Peer(ESP_NOW.BROADCAST_ADDR, channel, iface, lmk) {}

  // Destructor of the class
  ~ESP_NOW_Broadcast_Peer() {
    remove();
  }

  // Function to properly initialize the ESP-NOW and register the broadcast peer
  bool begin() {
    if (!ESP_NOW.begin() || !add()) {
      log_e("Failed to initialize ESP-NOW or register the broadcast peer");
      return false;
    }
    return true;
  }

  // Function to send a message to all devices within the network
  bool send_message(const uint8_t *data, size_t len) {
    if (!send(data, len)) {
      log_e("Failed to broadcast message");
      return false;
    }
    return true;
  }
};

/* Global Variables */
uint32_t msg_count = 0;
ESP_NOW_Broadcast_Peer broadcast_peer(ESPNOW_WIFI_CHANNEL, WIFI_IF_STA, NULL);

/* Main */
void setup() {
  // Start serial communication at the baud rate of 9600
  Serial.begin(9600);
  while (!Serial) {
    delay(10);  // Wait for the serial connection to initialize
  }
  Serial.println("ESP32 ready to communicate!");

  // Initialize the Wi-Fi module for ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("ESP-NOW Example - Broadcast Master");
  Serial.println("Wi-Fi parameters:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  // Register the broadcast peer
  if (!broadcast_peer.begin()) {
    Serial.println("Failed to initialize broadcast peer");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Setup complete. Ready to communicate via Serial and broadcast via ESP-NOW.");
}

void loop() {
  if (Serial.available() > 0) {
    // Read incoming data in chunks
    String receivedData = "";
    while (Serial.available() > 0) {
      receivedData += (char)Serial.read();
      delay(10);  // Short delay to avoid overflowing the buffer
    }

    // Limit size of incoming data
    if (receivedData.length() > 250) {
      Serial.println("Error: Data too large, truncating...");
      receivedData = receivedData.substring(0, 250);
    }

    Serial.println("Received via Serial: " + receivedData);

    // Respond back via Serial
    String responseData = "ESP32 received: " + receivedData;
    Serial.println(responseData);

    // Prepare and send broadcast message via ESP-NOW
    char espnowData[250];
    snprintf(espnowData, sizeof(espnowData), "Serial Data: %s", receivedData.c_str());

    Serial.printf("Broadcasting message: %s\n", espnowData);

    if (!broadcast_peer.send_message((uint8_t *)espnowData, sizeof(espnowData))) {
      Serial.println("Failed to broadcast message");
    }
  }

  delay(100);  // Small delay to allow the system to handle other tasks
}

