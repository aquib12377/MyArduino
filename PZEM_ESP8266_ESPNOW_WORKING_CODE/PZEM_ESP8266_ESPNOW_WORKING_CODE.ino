#include <ESP8266WiFi.h>
#include <espnow.h>
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>

#define PZEM_RX_PIN 13
#define PZEM_TX_PIN 15
#define NUM_PZEMS 3
#define SLAVE_ID 1

// Create SoftwareSerial for PZEM communication
SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);

// Create an array of PZEM sensor objects
PZEM004Tv30 pzems[NUM_PZEMS];

// Define a structure to send sensor data including a slaveId
typedef struct struct_message {
    uint8_t slaveId;              // Unique identifier for this device
    float voltage[NUM_PZEMS];
    float current[NUM_PZEMS];
    float power[NUM_PZEMS];
    float energy[NUM_PZEMS];
    float frequency[NUM_PZEMS];
    float pf[NUM_PZEMS];
} struct_message;

struct_message myData;

// Replace with your ESP32 (receiver) MAC address
uint8_t receiverMAC[] = {0x78, 0x42, 0x1C, 0x6C, 0xA7, 0x04};

void sentCallback(uint8_t *mac, uint8_t status) {
    Serial.print("Send status: ");
    Serial.println(status == 0 ? "Success" : "Fail");
}

void setup() {
    Serial.begin(115200);
    
    // Initialize SoftwareSerial for the PZEM sensors
    pzemSWSerial.begin(9600);
    
    // Initialize each PZEM sensor with a unique address (0x01, 0x02, 0x03)
    for (int i = 0; i < NUM_PZEMS; i++) {
        pzems[i] = PZEM004Tv30(pzemSWSerial, 0x01 + i);
    }
    
    // Setup WiFi in Station mode and disconnect from any network
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Initialize ESP‑NOW
    if (esp_now_init() != 0) {
        Serial.println("ESP‑NOW Init Failed");
        return;
    }
    
    // Set ESP‑NOW role and register the send callback
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_register_send_cb(sentCallback);
    
    // Add the ESP32 (receiver) as a peer; default channel is 1 here
    esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
    
    // Set the slaveId field in our data structure
    myData.slaveId = SLAVE_ID;
}

void loop() {
    // Read sensor data from each PZEM sensor
    for (int i = 0; i < NUM_PZEMS; i++) {
        myData.voltage[i]   = pzems[i].voltage();
        myData.current[i]   = pzems[i].current();
        myData.power[i]     = pzems[i].power();
        myData.energy[i]    = pzems[i].energy();
        myData.frequency[i] = pzems[i].frequency();
        myData.pf[i]        = pzems[i].pf();
    }
    
    // (Optional) Print sensor data locally for debugging
    Serial.print("Slave ID: ");
    Serial.println(myData.slaveId);
    for (int i = 0; i < NUM_PZEMS; i++) {
        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(" Voltage: ");
        Serial.print(myData.voltage[i]);
        Serial.println(" V");
    }
    Serial.println("----------------------");
    
    // Send the sensor data structure via ESP‑NOW to the ESP32 receiver
    esp_now_send(receiverMAC, (uint8_t *)&myData, sizeof(myData));
    delay(2000); // Send every 2 seconds
}
