#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/* ===================== PROJECT NAMESPACE ===================== */
static const char* PROJECT = "rsandesh";  // change per deployment
static inline void buildTopic(char* out, size_t outsz, const char* path) {
  snprintf(out, outsz, "%s/%s", PROJECT, path);
}

/* ===================== NETWORK / MQTT ===================== */
const char* WIFI_SSID = "Airtel_sour_3253";
const char* WIFI_PASS = "Air@19608";

const char* MQTT_HOST = "mqtt.modelsofbrainwing.com";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USER = "reactuser";
const char* MQTT_PASS = "scaleModel";

// TLS Root CA (Let’s Encrypt ISRG Root X1)
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

/* ===================== I2C (to slave) ===================== */
const uint8_t I2C_SDA = 11;
const uint8_t I2C_SCL = 12;
const uint8_t I2C_SLAVE_ADDR = 0x08;

// Slave opcodes (must match your slave)
enum : uint8_t {
  CMD_PATTERN = 0x10,
  CMD_ALL_ON = 0x11,
  CMD_ALL_OFF = 0x12,
  CMD_BHK = 0x13,  // arg1: 3 or 4
  CMD_WING_SELECT = 0x14,
  CMD_WING_CLICK = 0x15
};

static inline uint8_t wingNameToId(const char* wing) {
  if (!wing) return 0;
  if (!strcasecmp(wing, "a-wing")) return 1;
  if (!strcasecmp(wing, "b-wing")) return 2;
  if (!strcasecmp(wing, "shops")) return 3;
  return 0;
}

/* ---- I2C send with retry ---- */
bool i2cSend(uint8_t cmd, uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) {
  uint8_t pkt[8];
  pkt[0] = 0xAA;
  pkt[1] = cmd;
  pkt[2] = a;
  pkt[3] = b;
  pkt[4] = c;
  pkt[5] = d;
  pkt[6] = (uint8_t)(pkt[1] + pkt[2] + pkt[3] + pkt[4] + pkt[5]);
  pkt[7] = 0x55;
  const uint8_t MAX_TRY = 3;
  for (uint8_t t = 0; t < MAX_TRY; ++t) {
    Wire.beginTransmission(I2C_SLAVE_ADDR);
    Wire.write(pkt, sizeof(pkt));
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("[I2C] cmd=0x%02X args=[%u,%u,%u,%u] OK\n", cmd, a, b, c, d);
      return true;
    }
    Serial.printf("[I2C] cmd=0x%02X try#%u err=%u\n", cmd, t + 1, err);
    delay(2 + (t * 2));  // tiny backoff
  }
  return false;
}

/* ===================== Local relays (ESP32) ===================== */
#define RELAY_ACTIVE_LOW true
const int PIN_RELAY_SHOPS = 10;     // relay1
const int PIN_RELAY_LANDSCAPE = 9;  // relay2
const int PIN_RELAY_PARKING = 8;    // relay3
const int PIN_RELAY_SURROUND = 7;   // relay4

static inline void relayWriteRaw(int pin, bool on) {
  if (RELAY_ACTIVE_LOW) digitalWrite(pin, on ? LOW : HIGH);
  else digitalWrite(pin, on ? HIGH : LOW);
}
void setRelayOn(int pin, const char* name) {
  relayWriteRaw(pin, true);
  Serial.printf("[RELAY] %s -> ON\n", name);
}
void setRelayOff(int pin, const char* name) {
  relayWriteRaw(pin, false);
  Serial.printf("[RELAY] %s -> OFF\n", name);
}
void relaysSetup() {
  pinMode(PIN_RELAY_SHOPS, OUTPUT);
  pinMode(PIN_RELAY_LANDSCAPE, OUTPUT);
  pinMode(PIN_RELAY_PARKING, OUTPUT);
  pinMode(PIN_RELAY_SURROUND, OUTPUT);
  setRelayOff(PIN_RELAY_SHOPS, "Shops");
  setRelayOff(PIN_RELAY_LANDSCAPE, "Landscape");
  setRelayOff(PIN_RELAY_PARKING, "Parking");
  setRelayOff(PIN_RELAY_SURROUND, "Surround");
}

/* === Helpers to control all relays together === */
void setAllRelaysOn() {
  setRelayOn(PIN_RELAY_SHOPS, "Shops");
  setRelayOn(PIN_RELAY_LANDSCAPE, "Landscape");
  setRelayOn(PIN_RELAY_PARKING, "Parking");
  setRelayOn(PIN_RELAY_SURROUND, "Surround");
}
void setAllRelaysOff() {
  setRelayOff(PIN_RELAY_SHOPS, "Shops");
  setRelayOff(PIN_RELAY_LANDSCAPE, "Landscape");
  setRelayOff(PIN_RELAY_PARKING, "Parking");
  setRelayOff(PIN_RELAY_SURROUND, "Surround");
}

/* ===================== Buttons → mapped actions ===================== */
/*
   Button mapping (by index in `btns`):
   0 -> Button1 : OFF command (I2C CMD_ALL_OFF)
   1 -> Button2 : PATTERN command (I2C CMD_PATTERN)
   2 -> Button3 : Turn ON all relays (local only)
   3 -> Button4 : Send ALL LIGHTS ON (I2C CMD_ALL_ON) AND turn ON all relays
*/
// Adjust pins to your hardware
const int BTN_LANDSCAPE = 17;  // treated as Button1
const int BTN_PARKING = 18;    // Button2
const int BTN_SHOPS = 21;      // Button3
const int BTN_SURROUND = 38;   // Button4

struct Btn {
  int pin;
  uint32_t lastChangeMs;
  bool lastLevel;
  bool armed;  // edge-detect: true when released; fire once when goes LOW
};
Btn btns[4] = {
  { BTN_LANDSCAPE, 0, true, true },
  { BTN_PARKING, 0, true, true },
  { BTN_SHOPS, 0, true, true },
  { BTN_SURROUND, 0, true, true },
};
const uint16_t DEBOUNCE_MS = 50;

void buttonsSetup() {
  pinMode(BTN_LANDSCAPE, INPUT_PULLUP);
  pinMode(BTN_PARKING, INPUT_PULLUP);
  pinMode(BTN_SHOPS, INPUT_PULLUP);
  pinMode(BTN_SURROUND, INPUT_PULLUP);
}

void publishAck(const char* type, const char* detail, bool ok, const char* msg = "");

/* === Send ALL LIGHTS ON + relays ON as a single action === */
void sendAllLightsOnAndRelays() {
  i2cSend(CMD_ALL_ON);
  setAllRelaysOn();
  publishAck("all_on", "relays+leds", true, "i2c+relays");
}

void handleButtonPressIndex(uint8_t idx) {
  switch (idx) {
    case 0:  // Button1 -> OFF command
      i2cSend(CMD_ALL_OFF);
      setAllRelaysOff();
      publishAck("all_off", "button1", true, "i2c");
      break;

    case 1:  // Button2 -> PATTERN command
      i2cSend(CMD_PATTERN);
      publishAck("pattern", "button2", true, "i2c");
      break;

    case 2:  // Button3 -> Turn ON all relays
      setAllRelaysOn();
      publishAck("relay", "all_on_button3", true, "on");
      break;

    case 3:  // Button4 -> ALL LIGHTS ON + relays ON
      sendAllLightsOnAndRelays();
      // ack already inside helper
      break;
  }
}

void scanButtons() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < 4; i++) {
    bool lvl = digitalRead(btns[i].pin);  // active-low
    if (lvl != btns[i].lastLevel) {
      btns[i].lastChangeMs = now;
      btns[i].lastLevel = lvl;
    } else {
      if ((now - btns[i].lastChangeMs) > DEBOUNCE_MS) {
        if (lvl == LOW && btns[i].armed) {  // just pressed
          btns[i].armed = false;            // disarm until release
          handleButtonPressIndex(i);
        } else if (lvl == HIGH) {
          btns[i].armed = true;  // re-arm on release
        }
      }
    }
  }
}

/* ===================== MQTT CLIENT ===================== */
WiFiClientSecure net;
PubSubClient mqtt(net);

void publishAck(const char* type, const char* detail, bool ok, const char* msg) {
  char topic[64];
  buildTopic(topic, sizeof(topic), "ui/ack");
  StaticJsonDocument<192> d;
  d["ok"] = ok;
  if (type && *type) d["type"] = type;
  if (detail && *detail) d["detail"] = detail;
  if (msg && *msg) d["msg"] = msg;
  char buf[192];
  size_t n = serializeJson(d, buf, sizeof(buf));
  mqtt.publish(topic, buf, false);
}

/* ===================== Simple text parser (fallback) ===================== */
static inline void toLowerNoSpace(char* s) {
  char* w = s;
  for (char* r = s; *r; ++r) {
    char c = *r;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '_') continue;
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    *w++ = c;
  }
  *w = 0;
}

// Handles commands like: OFF, 3BHK, 4BHK, PATTERN, ALLLIGHTS, LANDSCAPE, ...
void handleSimpleCommand(char* s) {
  toLowerNoSpace(s);

  if (!strcmp(s, "pattern")) {
    i2cSend(CMD_PATTERN);
    publishAck("pattern", "", true, "i2c");
    return;
  }
  if (!strcmp(s, "off")) {
    i2cSend(CMD_ALL_OFF);
    publishAck("off", "", true, "i2c");
    return;
  }
  if (!strcmp(s, "3bhk")) {
    i2cSend(CMD_BHK, 3);
    publishAck("bhk", "3BHK", true, "i2c");
    return;
  }
  if (!strcmp(s, "4bhk")) {
    i2cSend(CMD_BHK, 4);
    publishAck("bhk", "4BHK", true, "i2c");
    return;
  }

  // CHANGED: all lights ON also turns on all relays
  if (!strcmp(s, "alllights")) {
    sendAllLightsOnAndRelays();
    return;
  }

  if (!strcmp(s, "landscape")) {
    setRelayOn(PIN_RELAY_LANDSCAPE, "Landscape");
    publishAck("relay", "Landscape", true, "on");
    return;
  }
  if (!strcmp(s, "parking")) {
    setRelayOn(PIN_RELAY_PARKING, "Parking");
    publishAck("relay", "Parking", true, "on");
    return;
  }
  if (!strcmp(s, "shops")) {
    setRelayOn(PIN_RELAY_SHOPS, "Shops");
    publishAck("relay", "Shops", true, "on");
    return;
  }
  if (!strcmp(s, "surroundlights") || !strcmp(s, "surround")) {
    setRelayOn(PIN_RELAY_SURROUND, "Surround");
    publishAck("relay", "Surround", true, "on");
    return;
  }

  publishAck("unknown", s, false, "not handled");
}

/* ===================== MQTT → ACTIONS (JSON or plain text) ===================== */
void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  static char buf[512];
  len = min(len, (unsigned int)sizeof(buf) - 1);
  memcpy(buf, payload, len);
  buf[len] = 0;

  Serial.printf("[MQTT] RX %s : %s\n", topic, buf);

  // Try JSON first
  StaticJsonDocument<384> doc;
  DeserializationError err = deserializeJson(doc, buf);
  if (!err) {
    const char* type = doc["type"] | "";
    const char* item = doc["item"] | "";
    const char* wing = doc["wing"] | "";

    if (strcmp(type, "ping") == 0) {
      publishAck("pong", "", true, "alive");
      return;
    }

    if (strcmp(type, "podium") == 0) {
      // ON (no toggle)
      if (!strcasecmp(item, "Landscape")) {
        setRelayOn(PIN_RELAY_LANDSCAPE, "Landscape");
        publishAck("relay", "Landscape", true, "on");
      } else if (!strcasecmp(item, "Parking")) {
        setRelayOn(PIN_RELAY_PARKING, "Parking");
        publishAck("relay", "Parking", true, "on");
      } else if (!strcasecmp(item, "Shops")) {
        setRelayOn(PIN_RELAY_SHOPS, "Shops");
        publishAck("relay", "Shops", true, "on");
      } else publishAck("podium", item, false, "unknown item");
      return;
    }
    if (strcmp(type, "surround") == 0) {
      setRelayOn(PIN_RELAY_PARKING, "Parking");
      setRelayOn(PIN_RELAY_SURROUND, "Surround");
      publishAck("relay", "Surround", true, "on");
      return;
    }
    if (strcmp(type, "shops") == 0) {
      setRelayOn(PIN_RELAY_SHOPS, "Shops");
      publishAck("relay", "Shops", true, "on");
      return;
    }

    if (strcmp(type, "pattern") == 0) {
      i2cSend(CMD_PATTERN);
      publishAck("pattern", "", true, "i2c");
    }
    // CHANGED: all_on => I2C + all relays ON
    else if (strcmp(type, "all_on") == 0) {
      sendAllLightsOnAndRelays();
    } else if (strcmp(type, "all_off") == 0) {
      i2cSend(CMD_ALL_OFF);
      setAllRelaysOff();
      publishAck("all_off", "", true, "i2c");
    } else if (strcmp(type, "bhk") == 0) {
      uint8_t which = (!strcasecmp(item, "3BHK")) ? 3 : (!strcasecmp(item, "4BHK") ? 4 : 0);
      if (which) {
        i2cSend(CMD_BHK, which);
        publishAck("bhk", item, true, "i2c");
      } else publishAck("bhk", item, false, "unknown bhk");
    } else if (strcmp(type, "wing_select") == 0) {
      uint8_t w = wingNameToId(wing);
      if (w) {
        i2cSend(CMD_WING_SELECT, w);
        if (w == 3) {
          setRelayOn(PIN_RELAY_SHOPS, "Shops");
        }
        publishAck("wing_select", wing, true, "i2c");
      } else publishAck("wing_select", wing, false, "unknown wing");
    } else if (strcmp(type, "wing_click") == 0) {
      uint8_t w = wingNameToId(wing);
      if (w) {
        i2cSend(CMD_WING_CLICK, w);
        if (w == 3) {
          setRelayOn(PIN_RELAY_SHOPS, "Shops");
        }
        publishAck("wing_click", wing, true, "i2c");
      } else publishAck("wing_click", wing, false, "unknown wing");
    } else {
      publishAck(type, "", false, "unknown type");
    }
    return;
  }

  // Fallback: plain text commands
  handleSimpleCommand(buf);
}

/* ===================== MQTT CONNECT (w/ stable subs) ===================== */
WiFiClientSecure netSecure;          // (kept 'net' above)
PubSubClient mqttClient(netSecure);  // not used; keep existing 'mqtt'

bool subscribeAll() {
  char subJson[64];
  buildTopic(subJson, sizeof(subJson), "ui/cmd");
  char subPlain[64];
  buildTopic(subPlain, sizeof(subPlain), "cmd");  // plain text channel
  bool ok1 = mqtt.subscribe(subJson, 0);
  bool ok2 = mqtt.subscribe(subPlain, 0);
  Serial.printf("[MQTT] subscribe %s : %s\n", subJson, ok1 ? "OK" : "FAIL");
  Serial.printf("[MQTT] subscribe %s : %s\n", subPlain, ok2 ? "OK" : "FAIL");
  return ok1 && ok2;
}

bool mqttConnectOnce() {
  char willTopic[64];
  buildTopic(willTopic, sizeof(willTopic), "ui/status");  // retained LWT 'offline'
  Serial.print("[MQTT] connecting... ");
  if (!mqtt.connect("esp32-mqtt-i2c-master",
                    MQTT_USER, MQTT_PASS,
                    willTopic, 0, true, "offline", true)) {
    Serial.printf("fail rc=%d\n", mqtt.state());
    return false;
  }
  Serial.println("ok");
  mqtt.publish(willTopic, "online", true);  // retained
  return subscribeAll();
}

/* ===================== Backoff helpers ===================== */
uint32_t backoffMs(uint8_t attempt, uint32_t base = 750, uint32_t maxMs = 15000) {
  uint32_t exp = base << (attempt > 5 ? 5 : attempt);  // cap shift
  if (exp > maxMs) exp = maxMs;
  uint32_t jitter = esp_random() % (exp / 3 + 1);
  return exp + jitter;
}

/* ===================== SETUP / LOOP ===================== */
void setup() {
  Serial.begin(115200);
  delay(50);

  relaysSetup();
  buttonsSetup();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] connecting");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n[WiFi] IP=%s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n[WiFi] initial connect failed, will retry in loop");

  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.printf("[I2C] SDA=%u SCL=%u addr=0x%02X\n", I2C_SDA, I2C_SCL, I2C_SLAVE_ADDR);

  net.setCACert(ROOT_CA_PEM);
  net.setTimeout(15000);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setKeepAlive(25);
  mqtt.setBufferSize(768);  // roomy for JSON acks

  mqttConnectOnce();
}

void loop() {
  // Button polling (debounced edge-detect)
  scanButtons();

  // Maintain WiFi
  static uint8_t wifiTry = 0;
  static uint32_t nextWifiAt = 0;
  if (WiFi.status() != WL_CONNECTED) {
    uint32_t now = millis();
    if (now >= nextWifiAt) {
      Serial.println("[WiFi] reconnecting...");
      WiFi.disconnect(true);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      wifiTry++;
      nextWifiAt = now + backoffMs(wifiTry, 750, 10000);
    }
  } else {
    wifiTry = 0;  // reset on success
  }

  // Maintain MQTT
  static uint8_t mqttTry = 0;
  static uint32_t nextMqttAt = 0;
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      uint32_t now = millis();
      if (now >= nextMqttAt) {
        bool ok = mqttConnectOnce();
        mqttTry = ok ? 0 : (uint8_t)(mqttTry + 1);
        nextMqttAt = now + (ok ? 1000 : backoffMs(mqttTry, 750, 15000));
      }
    } else {
      mqtt.loop();
    }
  }

  delay(5);
}
