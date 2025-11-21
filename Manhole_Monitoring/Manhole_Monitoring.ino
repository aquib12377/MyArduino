#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

const char* WIFI_SSID = "rpihotspot";
const char* WIFI_PASS = "12345678";

// If you put Nginx on port 80, use "ws://192.168.4.1/ws"
const char* WS_URL    = "ws://10.42.0.1:5000/ws";  // change to your Pi IP

// Example GPIOs driven by button state
const int LED1 = 2;   // onboard LED on many ESP32 dev boards
const int LED2 = 4;

WebsocketsClient client;
bool btn1 = false, btn2 = false;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi:");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(500); }
  Serial.print(" connected, IP=");
  Serial.println(WiFi.localIP());
}

void handleStateMessage(const String& msg) {
  Serial.print("Raw message: "); Serial.println(msg);

  // Expected: {"button1":"Clicked/Not Clicked","button2":"Clicked/Not Clicked"}
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.print("JSON error: "); Serial.println(err.c_str());
    return;
  }
  const char* s1 = doc["button1"] | "Not Clicked";
  const char* s2 = doc["button2"] | "Not Clicked";
  bool n1 = (String(s1) == "Clicked");
  bool n2 = (String(s2) == "Clicked");

  if (n1 != btn1 || n2 != btn2) {
    btn1 = n1; btn2 = n2;
    Serial.printf("Update: button1=%s, button2=%s\n",
                  btn1 ? "Clicked" : "Not Clicked",
                  btn2 ? "Clicked" : "Not Clicked");
    digitalWrite(LED1, btn1 ? HIGH : LOW);
    digitalWrite(LED2, btn2 ? HIGH : LOW);
  }
}

void connectWebSocket() {
  Serial.print("WS connect: ");
  if (client.connect(WS_URL)) {
    Serial.println("ok");
    client.send("get");   // optional
  } else {
    Serial.println("failed");
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  connectWiFi();

  // Message callbacks
  client.onMessage([&](WebsocketsMessage message) {
      Serial.print("Raw message: "); Serial.println(message.data());

    if (message.isText()) {
      handleStateMessage(message.data());
    }
  });
  client.onEvent([&](WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionClosed) {
      Serial.println("WS closed");
    } else if (event == WebsocketsEvent::GotPing) {
      client.pong();
    } else if (event == WebsocketsEvent::GotPong) {
      // ignore
    }
  });

  connectWebSocket();
}

unsigned long lastPing = 0;
unsigned long lastRetry = 0;

void loop() {
  if (client.available()) {
    client.poll();
  } else {
    // try to reconnect periodically
    if (millis() - lastRetry > 3000) {
      lastRetry = millis();
      if (WiFi.status() != WL_CONNECTED) connectWiFi();
      connectWebSocket();
    }
  }

  // keepalive ping every 15s (optional)
  if (millis() - lastPing > 15000 && client.available()) {
    lastPing = millis();
    client.ping();
  }
}
