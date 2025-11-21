#include <WiFi.h>
#include <WebSocketsServer.h> // Include the new library

// --- Wi-Fi Network Details ---
const char* ssid = "ESP32-Control-Hub";
const char* password = "password123";

// Create a WebSocket server object that listens on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// This function gets called for every WebSocket event
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    // Use a switch statement to handle different event types
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WebSocket] Client #%u disconnected.\n", num);
            break;
        
        case WStype_CONNECTED: {
            // When a new client connects, get their IP address
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[WebSocket] Client #%u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
            
            // Send a welcome message to the connected client
            webSocket.sendTXT(num, "Welcome!");
            break;
        }

        case WStype_TEXT:
            // When a client sends a text message
            Serial.printf("[WebSocket] Received from client #%u: %s\n", num, payload);

            //
            // --- YOUR COMMAND PROCESSING LOGIC GOES HERE ---
            // The JSON string is in the 'payload' variable.
            // You can parse it and control your hardware.
            //

            // Send a confirmation message back to the client
            webSocket.sendTXT(num, "Command received by ESP32.");
            break;

        // Note: Other event types like binary, error, fragment, etc., are not handled here
    }
}

void setup() {
    Serial.begin(115200);

    Serial.println("Starting ESP32 in Access Point Mode...");
    WiFi.softAP(ssid, password);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); // This will still be 192.168.4.1

    // Start the WebSocket server
    webSocket.begin();
    
    // Set the function that will handle events
    webSocket.onEvent(webSocketEvent);
    
    Serial.println("WebSocket server started on port 81.");
}

void loop() {
    // This is VERY IMPORTANT. You must call webSocket.loop() continuously
    // to allow the server to process incoming connections and messages.
    webSocket.loop();
}