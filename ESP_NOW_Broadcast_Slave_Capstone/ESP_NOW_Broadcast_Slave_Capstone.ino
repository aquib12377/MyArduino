#include "ESP32_NOW.h"
#include "WiFi.h"
#include <esp_mac.h>  // For the MAC2STR and MACSTR macros
#include <vector>

/* Definitions */
#define ESPNOW_WIFI_CHANNEL 6

// Motor Pins
#define LEFT_MOTOR_FORWARD 13
#define LEFT_MOTOR_BACKWARD 12
#define RIGHT_MOTOR_FORWARD 14
#define RIGHT_MOTOR_BACKWARD 27

/* Function Declarations */
void moveForward(int duration);
void moveBackward(int duration);
void turnLeft(int duration);
void turnRight(int duration);
void stopMotors();
void handleProduct1MotorAction();
void handleProduct2MotorAction();
void handleProduct3MotorAction();
void handleProduct4MotorAction();

/* Classes */

// Creating a new class that inherits from the ESP_NOW_Peer class is required.
class ESP_NOW_Peer_Class : public ESP_NOW_Peer {
public:
  ESP_NOW_Peer_Class(const uint8_t *mac_addr, uint8_t channel, wifi_interface_t iface, const uint8_t *lmk) 
      : ESP_NOW_Peer(mac_addr, channel, iface, lmk) {}

  ~ESP_NOW_Peer_Class() {}

  bool add_peer() {
    if (!add()) {
      log_e("Failed to register the broadcast peer");
      return false;
    }
    return true;
  }

  void onReceive(const uint8_t *data, size_t len, bool broadcast) {
    Serial.printf("Received a message from master " MACSTR " (%s)\n", MAC2STR(addr()), broadcast ? "broadcast" : "unicast");
    Serial.printf("  Message: %s\n", (char *)data);

    String message = String((char*)data);
    message.trim();
    Serial.println(message);
    if (message.indexOf("1") >= 0) {
      handleProduct1MotorAction();
    } else if (message.indexOf("2") >= 0) {
      handleProduct2MotorAction();
    } else if (message.indexOf("3") >= 0) {
      handleProduct3MotorAction();
    } else if (message.indexOf("4") >= 0) {
      handleProduct4MotorAction();
    }
  }
};

/* Global Variables */
std::vector<ESP_NOW_Peer_Class> masters;

/* Motor Control Functions */
void moveForward(int duration) {
  Serial.println("Moving forward");
  digitalWrite(LEFT_MOTOR_FORWARD, HIGH);
  digitalWrite(LEFT_MOTOR_BACKWARD, LOW);
  digitalWrite(RIGHT_MOTOR_FORWARD, HIGH);
  digitalWrite(RIGHT_MOTOR_BACKWARD, LOW);
  delay(duration);
  stopMotors();
}

void moveBackward(int duration) {
  Serial.println("Moving backward");
  digitalWrite(LEFT_MOTOR_FORWARD, LOW);
  digitalWrite(LEFT_MOTOR_BACKWARD, HIGH);
  digitalWrite(RIGHT_MOTOR_FORWARD, LOW);
  digitalWrite(RIGHT_MOTOR_BACKWARD, HIGH);
  delay(duration);
  stopMotors();
}

void turnLeft(int duration) {
  Serial.println("Turning left");
  digitalWrite(LEFT_MOTOR_FORWARD, LOW);
  digitalWrite(LEFT_MOTOR_BACKWARD, HIGH);
  digitalWrite(RIGHT_MOTOR_FORWARD, HIGH);
  digitalWrite(RIGHT_MOTOR_BACKWARD, LOW);
  delay(duration);
  stopMotors();
}

void turnRight(int duration) {
  Serial.println("Turning right");
  digitalWrite(LEFT_MOTOR_FORWARD, HIGH);
  digitalWrite(LEFT_MOTOR_BACKWARD, LOW);
  digitalWrite(RIGHT_MOTOR_FORWARD, LOW);
  digitalWrite(RIGHT_MOTOR_BACKWARD, HIGH);
  delay(duration);
  stopMotors();
}

void stopMotors() {
  Serial.println("Stopping motors");
  digitalWrite(LEFT_MOTOR_FORWARD, LOW);
  digitalWrite(LEFT_MOTOR_BACKWARD, LOW);
  digitalWrite(RIGHT_MOTOR_FORWARD, LOW);
  digitalWrite(RIGHT_MOTOR_BACKWARD, LOW);
}

/* Handle Motor Actions for Products */
void handleProduct1MotorAction() {
  Serial.println("Handling motor action for Product 1...");
  moveForward(5000);  // Move forward for 5 seconds
  turnRight(2000);    // Turn right for 2 seconds
  turnLeft(3000);     // Turn left for 3 seconds
  stopMotors();       // Stop motors
}

void handleProduct2MotorAction() {
  Serial.println("Handling motor action for Product 2...");
  moveForward(4000);  // Move forward for 4 seconds
  turnRight(3000);    // Turn right for 3 seconds
  stopMotors();       // Stop motors
}

void handleProduct3MotorAction() {
  Serial.println("Handling motor action for Product 3...");
  moveBackward(5000); // Move backward for 5 seconds
  stopMotors();       // Stop motors
}

void handleProduct4MotorAction() {
  Serial.println("Handling motor action for Product 4...");
  moveForward(6000);  // Move forward for 6 seconds
  turnRight(4000);    // Turn right for 4 seconds
  turnLeft(2000);     // Turn left for 2 seconds
  stopMotors();       // Stop motors
}

/* Callbacks */
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
    log_v("Received a unicast message from " MACSTR, MAC2STR(info->src_addr));
    log_v("Ignoring the message");
  }
}

/* Main */
void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }

  // Initialize motor control pins
  pinMode(LEFT_MOTOR_FORWARD, OUTPUT);
  pinMode(LEFT_MOTOR_BACKWARD, OUTPUT);
  pinMode(RIGHT_MOTOR_FORWARD, OUTPUT);
  pinMode(RIGHT_MOTOR_BACKWARD, OUTPUT);

  // Initialize Wi-Fi and ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  while (!WiFi.STA.started()) {
    delay(100);
  }

  Serial.println("ESP-NOW Example - Broadcast Slave");
  Serial.println("Wi-Fi parameters:");
  Serial.println("  Mode: STA");
  Serial.println("  MAC Address: " + WiFi.macAddress());
  Serial.printf("  Channel: %d\n", ESPNOW_WIFI_CHANNEL);

  if (!ESP_NOW.begin()) {
    Serial.println("Failed to initialize ESP-NOW");
    Serial.println("Rebooting in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  // Register the new peer callback
  ESP_NOW.onNewPeer(register_new_master, NULL);

  Serial.println("Setup complete. Waiting for a master to broadcast a message...");
}

void loop() {
  delay(1000);
}
