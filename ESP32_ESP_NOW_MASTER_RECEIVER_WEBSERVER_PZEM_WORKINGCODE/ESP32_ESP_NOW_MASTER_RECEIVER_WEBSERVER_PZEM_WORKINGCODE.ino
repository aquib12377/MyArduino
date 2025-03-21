#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>

// Configuration for PZEM sensors per slave
#define NUM_PZEMS 3
#define NUM_SLAVES 4

// Structure used by the slaves; must match the sender’s structure.
typedef struct struct_message {
  uint8_t slaveId;              // Unique identifier for the slave (1 to 4)
  float voltage[NUM_PZEMS];
  float current[NUM_PZEMS];
  float power[NUM_PZEMS];  // Note: There was a typo in previous versions; ensure consistent naming
  float energy[NUM_PZEMS];
  float frequency[NUM_PZEMS];
  float pf[NUM_PZEMS];
} struct_message;

// Global array to hold the latest data from each slave.
// Index 0 corresponds to slaveId 1, index 1 to slaveId 2, etc.
struct_message slavesData[NUM_SLAVES];

// Create a web server on port 80.
WebServer server(80);

// ESP‑NOW callback: called when data is received.
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) {
    Serial.print("Received unexpected data length: ");
    Serial.println(len);
    return;
  }
  
  struct_message received;
  memcpy(&received, incomingData, sizeof(received));
  
  Serial.print("Received data from slave ID: ");
  Serial.println(received.slaveId);
  
  // Check if slaveId is within expected range
  if (received.slaveId >= 1 && received.slaveId <= NUM_SLAVES) {
    // Update the corresponding entry in slavesData.
    slavesData[received.slaveId - 1] = received;
  } else {
    Serial.println("Received slaveId out of range.");
  }
}

// Handle the root URL ("/") of the webserver.
void handleRoot() {
  String html = "<html><head><title>ESP32 Sensor Data</title></head><body>";
  html += "<h1>Sensor Data from Slaves</h1>";
  html += "<table border='1' style='border-collapse:collapse;'>";
  html += "<tr><th>Slave ID</th><th>Sensor</th><th>Voltage (V)</th><th>Current (A)</th><th>Power (W)</th><th>Energy (kWh)</th><th>Frequency (Hz)</th><th>PF</th></tr>";
  
  // Loop through all slaves
  for (int slave = 0; slave < NUM_SLAVES; slave++) {
    // If slaveId is 0, we assume no data received yet.
    if (slavesData[slave].slaveId == 0) {
      html += "<tr><td>" + String(slave + 1) + "</td><td colspan='7'>No data received</td></tr>";
    } else {
      // For each sensor on this slave, add a row.
      for (int sensor = 0; sensor < NUM_PZEMS; sensor++) {
        html += "<tr>";
        // Only show slaveId on first sensor row for this slave.
        if (sensor == 0) {
          html += "<td rowspan='" + String(NUM_PZEMS) + "'>" + String(slavesData[slave].slaveId) + "</td>";
        }
        html += "<td>Sensor " + String(sensor) + "</td>";
        html += "<td>" + String(slavesData[slave].voltage[sensor], 2) + "</td>";
        html += "<td>" + String(slavesData[slave].current[sensor], 2) + "</td>";
        html += "<td>" + String(slavesData[slave].power[sensor], 2) + "</td>";
        html += "<td>" + String(slavesData[slave].energy[sensor], 3) + "</td>";
        html += "<td>" + String(slavesData[slave].frequency[sensor], 1) + "</td>";
        html += "<td>" + String(slavesData[slave].pf[sensor], 2) + "</td>";
        html += "</tr>";
      }
    }
  }
  
  html += "</table>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize the global slavesData array to zeros.
  memset(slavesData, 0, sizeof(slavesData));
  
  // Set WiFi mode to AP+STA for ESP‑NOW and webserver.
  WiFi.mode(WIFI_AP_STA);
  
  // Set up an access point for the webserver.
  const char *apSSID = "ESP32_AP";
  const char *apPassword = "esp32pass";  // Change as desired.
  WiFi.softAP(apSSID, apPassword);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Initialize ESP‑NOW.
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP‑NOW Init Failed");
    return;
  }
  
  // Register the ESP‑NOW receive callback.
  esp_now_register_recv_cb(OnDataRecv);
  
  // Start the webserver and define URL handlers.
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Webserver started.");
}

void loop() {
  // Handle incoming web requests.
  server.handleClient();
  
  // (Optional) Print out latest data to Serial for debugging.
  // Uncomment below if desired.
  /*
  for (int i = 0; i < NUM_SLAVES; i++) {
    if (slavesData[i].slaveId != 0) {
      Serial.print("Slave ");
      Serial.print(slavesData[i].slaveId);
      Serial.print(" Sensor 0 Voltage: ");
      Serial.println(slavesData[i].voltage[0]);
    }
  }
  */
  
  delay(100);
}
