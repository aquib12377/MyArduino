#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ===== USER VARS =====
const char* WIFI_SSID = "MyProject";
const char* WIFI_PASS = "12345678";

const char* MQTT_HOST = "mqtt.modelsofbrainwing.com";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USER = "reactuser";
const char* MQTT_PASS = "scaleModel";

// Root CA (ISRG Root X1 shown). You can keep this as-is, or paste your fullchain.pem instead.
static const char ROOT_CA_PEM[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)PEM";
// Topics
const char* SUB_TOPIC_CMD   = "building/ui/cmd";
const char* PUB_TOPIC_ACK   = "building/ui/ack";
const char* PUB_TOPIC_STATE = "building/ui/status";

// LWT (Last Will)
const char*   LWT_TOPIC    = "building/ui/status";
const char*   LWT_MESSAGE  = "offline";
const uint8_t LWT_QOS      = 0;
const bool    LWT_RETAIN   = true;

// ===== Networking / MQTT =====
WiFiClientSecure net;
PubSubClient mqtt(net);

// ACK helper (optional)
void publishAck(const char* type, const char* item, bool ok, const char* msg = "") {
  StaticJsonDocument<192> d;
  d["ok"]   = ok;
  d["type"] = type ? type : "";
  if (item && *item) d["item"] = item;
  if (msg && *msg)   d["msg"]  = msg;
  char buf[192];
  size_t n = serializeJson(d, buf, sizeof(buf));
  mqtt.publish(PUB_TOPIC_ACK, buf, false);
}

void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  static char buf[512];
  len = min(len, (unsigned int)sizeof(buf) - 1);
  memcpy(buf, payload, len);
  buf[len] = 0;

  Serial.printf("[MQTT] RX topic=%s payload=%s\n", topic, buf);

  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, buf);
  if (err) {
    Serial.printf("[JSON] parse error: %s\n", err.c_str());
    publishAck("?", "", false, "bad json");
    return;
  }

  const char* type  = doc["type"]  | "";
  const char* item  = doc["item"]  | "";   // for podium/bhk
  const char* wing  = doc["wing"]  | "";   // for wing_select/wing_click
  const char* color = doc["color"] | "";   // for pattern-like commands
  uint8_t     br    = doc["brightness"] | 255;

  // ---- buttons without submenu ----
  if (strcmp(type, "pattern") == 0) {
    Serial.println("[UI] Pattern clicked");
    publishAck("pattern", "", true, "received");
  }
  else if (strcmp(type, "surround") == 0) {
    Serial.println("[UI] Surround Lights clicked");
    publishAck("surround", "", true, "received");
  }
  else if (strcmp(type, "all_on") == 0) {
    Serial.println("[UI] All Lights ON clicked");
    publishAck("all_on", "", true, "received");
  }
  else if (strcmp(type, "all_off") == 0) {
    Serial.println("[UI] OFF clicked");
    publishAck("all_off", "", true, "received");
  }

  // ---- buttons with submenu ----
  else if (strcmp(type, "podium") == 0) {
    Serial.printf("[UI] Podium -> %s\n", item);
    publishAck("podium", item, true, "received");
  }
  else if (strcmp(type, "bhk") == 0) {
    Serial.printf("[UI] BHK -> %s\n", item);
    publishAck("bhk", item, true, "received");
  }

  // ---- wing interactions from SVG ----
  else if (strcmp(type, "wing_select") == 0) {
    Serial.printf("[UI] Wing SELECT -> %s\n", wing);
    publishAck("wing_select", wing, true, "received");
  }
  else if (strcmp(type, "wing_click") == 0) {
    Serial.printf("[UI] Wing CLICK -> %s\n", wing);
    publishAck("wing_click", wing, true, "received");
  }

  // ---- unknown ----
  else {
    Serial.printf("[UI] Unknown type: %s\n", type);
    publishAck(type, "", false, "unknown type");
  }
}


// connect / reconnect with LWT (PubSubClient 8-arg overload)
bool mqttConnect() {
  Serial.print("[MQTT] connecting... ");
  if (!mqtt.connect("esp32-led-receiver",
                    MQTT_USER, MQTT_PASS,
                    LWT_TOPIC, LWT_QOS, LWT_RETAIN, LWT_MESSAGE,
                    true)) {
    Serial.printf("fail rc=%d\n", mqtt.state());
    return false;
  }
  Serial.println("ok");
  mqtt.subscribe(SUB_TOPIC_CMD, 0);
  mqtt.publish(PUB_TOPIC_STATE, "online", true);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.printf("\n[WiFi] IP=%s\n", WiFi.localIP().toString().c_str());

  net.setCACert(ROOT_CA_PEM);  // or net.setInsecure() for quick test only
  net.setTimeout(15000);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(25);

  mqttConnect();
}

void loop() {
  if (!mqtt.connected()) {
    static uint32_t last = 0;
    if (millis() - last > 3000) { last = millis(); mqttConnect(); }
  } else {
    mqtt.loop();
  }
}