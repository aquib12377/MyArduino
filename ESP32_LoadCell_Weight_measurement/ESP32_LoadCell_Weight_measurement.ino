/***************************************************************************
 *  ESP32‑S3  |  HX711 weight scale  |  BLE UART‑style streaming
 *  Requires: “NimBLE‑Arduino” (comes with ESP32 core ≥ v2.0.6)
 ***************************************************************************/
#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include <NimBLEDevice.h>                      // ★ BLE (NimBLE)

// ─── HX711 wiring ─────────────────────────────────────────────────────────
const int LOADCELL_DOUT_PIN = 12;
const int LOADCELL_SCK_PIN  = 11;
HX711 scale;

// ─── BLE UUIDs (Nordic UART style) ────────────────────────────────────────
#define UART_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_RX_CHAR_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_TX_CHAR_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

NimBLECharacteristic *txChar;                  // notify / read
NimBLECharacteristic *rxChar;                  // write

// ─── Helper: print to USB & BLE at once ───────────────────────────────────
void broadcast(const String &s) {
  Serial.println(s);
  if (txChar && txChar->getSubscribedCount())
    txChar->setValue(s.c_str());               // NimBLE copies internally
    txChar->notify();
}

// ─── CPU frequency fix (your original) ────────────────────────────────────
void fixCpuFrequency() {
  rtc_cpu_freq_config_t cfg;
  rtc_clk_cpu_freq_get_config(&cfg);
  rtc_clk_cpu_freq_set_config(&cfg);
  rtc_clk_cpu_freq_set_config_fast(&cfg);
}

// ─── BLE RX callback: accept new scale factor ─────────────────────────────
class RxCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *chr) override {
    std::string data = chr->getValue();        // ASCII line
    float newFactor = atof(data.c_str());
    if (newFactor != 0.0f) {
      scale.set_scale(newFactor);
      scale.tare();
      broadcast("Scale factor set to: " + String(newFactor, 3));
    }
  }
};

// ─── Setup ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  fixCpuFrequency();

  // HX711 init -------------------------------------------------------------
  broadcast("HX711 BLE Demo – ESP32‑S3");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(25246.0 / 71.0);   // your default factor
  scale.tare();
  broadcast("Scale ready…");

  // BLE init ---------------------------------------------------------------
  NimBLEDevice::init("HX711_Scale_BLE");
  NimBLEServer   *srv = NimBLEDevice::createServer();

  NimBLEService  *uartService = srv->createService(UART_SERVICE_UUID);

  txChar = uartService->createCharacteristic(
              UART_TX_CHAR_UUID,
              NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  rxChar = uartService->createCharacteristic(
              UART_RX_CHAR_UUID,
              NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallback());

  uartService->start();
  srv->getAdvertising()->addServiceUUID(uartService->getUUID());
  srv->getAdvertising()->start();
  broadcast("BLE advertising as HX711_Scale_BLE");
}

// ─── Main loop ────────────────────────────────────────────────────────────
void loop() {
  // Periodic weight (every 5 s)
  float single  = scale.get_units();
  float average = scale.get_units(10);

  broadcast("one reading:\t" + String(single, 1) +
            "\t| average:\t" + String(average, 5));

  scale.power_down();
  delay(5000);
  scale.power_up();
}
